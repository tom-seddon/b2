#include <shared/system.h>
#include "TVOutput.h"
#include <beeb/OutputData.h>
#include <shared/debug.h>
#include <string.h>
#include <stdlib.h>
#include <beeb/video.h>
#include <SDL.h>
#include "misc.h"
#include "conf.h"

#include <shared/enum_def.h>
#include "TVOutput.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define BLEND 0

//#if BLEND
//uint8_t n=0,x=128;
//#else
//uint8_t n=0,x=255;
//#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TVOutput::TVOutput() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TVOutput::~TVOutput() {
    if(m_pixel_format) {
        SDL_FreeFormat(m_pixel_format);
        m_pixel_format=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TVOutput::InitTexture(const SDL_PixelFormat *pixel_format) {
    if(pixel_format->BytesPerPixel!=4) {
        return false;
    }

    ASSERT(!m_pixel_format);
    m_pixel_format=ClonePixelFormat(pixel_format);

    m_texture_data.resize(TV_TEXTURE_WIDTH*TV_TEXTURE_HEIGHT);

    uint32_t r_mask=SDL_MapRGBA(pixel_format,0xff,0x00,0x00,0xff);
    uint32_t g_mask=SDL_MapRGBA(pixel_format,0x00,0xff,0x00,0xff);
    uint32_t b_mask=SDL_MapRGBA(pixel_format,0x00,0x00,0xff,0xff);

    for(size_t i=0;i<8;++i) {
        m_palette[i]=0;

        if(i&1) {
            m_palette[i]|=r_mask;
        }

        if(i&2) {
            m_palette[i]|=g_mask;
        }

        if(i&4) {
            m_palette[i]|=b_mask;
        }
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// (272 scanned lines + 40 retrace lines + 0.5 interlace lines) * (52+4+8=64)us = 20000us = 20ms

static const int HORIZONTAL_RETRACE_CYCLES=2*4;
static const int BACK_PORCH_CYCLES=2*8;
static const int SCAN_OUT_CYCLES=2*52;
static const int SCANLINE_CYCLES=HORIZONTAL_RETRACE_CYCLES+BACK_PORCH_CYCLES+SCAN_OUT_CYCLES;
static_assert(SCANLINE_CYCLES==128,"one scanline must be 64us");
static const int VERTICAL_RETRACE_SCANLINES=10;

#if FULL_PAL_HEIGHT
static const int HEIGHT_SCALE=2;
#else
static const int HEIGHT_SCALE=1;
#endif

// If this many lines are scanned without a vertical retrace, the TV
// retraces anyway.
//
// Originally I just set this to 272, because... well, why not? But
// Firetrack's loading screen has a flickery bit near the top with
// that value. I also found a few tests that displayed fine on a CRT
// TV looked a mess on the emulator with reasonable-sounding values
// for MAX_NUM_SCANNED_LINES.
//
// 500 seems OK. It doesn't have to be perfect, just something that
// means emulated TV output keeps going when there's no CRTC vsync
// output...
static const int MAX_NUM_SCANNED_LINES=500*HEIGHT_SCALE;

void TVOutput::UpdateOneHalfUnit(const VideoDataHalfUnit *hu,float amt) {
#if !BLEND
    (void)amt;
#endif

    switch(m_state) {
    default:
        ASSERT(0);
        break;

    case TVOutputState_VerticalRetrace:
        ++m_num_fields;
        m_state=TVOutputState_VerticalRetraceWait;
        m_x=0;
        m_y=0;
        m_line=m_texture_data.data();
#if FULL_PAL_HEIGHT
        // "Fun" (translation: brain-eating) fake interlace
        
        // if(m_num_fields&1) {
        //     m_line+=m_texture_pitch;
        //     ++m_y;
        // }
#endif
        
        m_state_timer=1;
        break;

    case TVOutputState_VerticalRetraceWait:
        {
            // Ignore everything.
            if(m_state_timer++>=VERTICAL_RETRACE_SCANLINES*SCANLINE_CYCLES) {
                ++m_texture_data_version;
                m_state_timer=0;
                m_state=TVOutputState_Scanout;
            }
        }
        break;

    case TVOutputState_Scanout:
        {
            if(hu->pixels[0]&0x80) {
                switch(hu->pixels[0]) {
                case BeebControlPixel_HSync:
                    m_state=TVOutputState_HorizontalRetrace;
                    break;

                case BeebControlPixel_VSync:
                    m_state=TVOutputState_VerticalRetrace;
                    break;

                case BeebControlPixel_Nothing:
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line=m_line+m_x;

                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0];
#if FULL_PAL_HEIGHT
                        line+=TV_TEXTURE_WIDTH;
                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0];
#endif
                    }
                    m_x+=8;
                    break;

                case BeebControlPixel_Cursor:
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line=m_line+m_x;

                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[7];
#if FULL_PAL_HEIGHT
                        line+=TV_TEXTURE_WIDTH;
                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[7];
#endif
                    }
                    m_x+=8;
                    break;

#if BBCMICRO_FINER_TELETEXT
                case BeebControlPixel_Teletext:
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line=m_line+m_x;

                        line[0]=m_palette[hu->teletext.colours[(hu->teletext.data0>>0)&1]];
                        line[1]=m_palette[hu->teletext.colours[(hu->teletext.data0>>1)&1]];
                        line[2]=m_palette[hu->teletext.colours[(hu->teletext.data0>>2)&1]];
                        line[3]=m_palette[hu->teletext.colours[(hu->teletext.data0>>3)&1]];
                        line[4]=m_palette[hu->teletext.colours[(hu->teletext.data0>>4)&1]];
                        line[5]=m_palette[hu->teletext.colours[(hu->teletext.data0>>5)&1]];
                        line[6]=m_palette[hu->teletext.colours[(hu->teletext.data0>>6)&1]];
                        line[7]=m_palette[hu->teletext.colours[(hu->teletext.data0>>7)&1]];

#if FULL_PAL_HEIGHT
                        uint32_t *line2=line+TV_TEXTURE_WIDTH;
                        line2[0]=m_palette[hu->teletext.colours[(hu->teletext.data1>>0)&1]];
                        line2[1]=m_palette[hu->teletext.colours[(hu->teletext.data1>>1)&1]];
                        line2[2]=m_palette[hu->teletext.colours[(hu->teletext.data1>>2)&1]];
                        line2[3]=m_palette[hu->teletext.colours[(hu->teletext.data1>>3)&1]];
                        line2[4]=m_palette[hu->teletext.colours[(hu->teletext.data1>>4)&1]];
                        line2[5]=m_palette[hu->teletext.colours[(hu->teletext.data1>>5)&1]];
                        line2[6]=m_palette[hu->teletext.colours[(hu->teletext.data1>>6)&1]];
                        line2[7]=m_palette[hu->teletext.colours[(hu->teletext.data1>>7)&1]];
#else
                        // Well, it's going to look kind of crap. But
                        // it won't break.
#endif
                        
                    }
                    m_x+=8;
                    break;
#endif
                }
            } else {
                if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
#if BLEND

                    float inv_amt=1.f-amt;

                    for(size_t i=0;i<8;++i) {
                        uint8_t rgb[4];
                        memcpy(rgb,&m_line[m_x+i],4);

                        float b=rgb[0]/255.f;
                        float g=rgb[1]/255.f;
                        float r=rgb[2]/255.f;

                        b*=inv_amt;
                        g*=inv_amt;
                        r*=inv_amt;

                        if(hu->pixels[i]&1) {
                            r+=amt;

                            if(r>1.f) {
                                r=1.f;
                            }
                        }

                        if(hu->pixels[i]&2) {
                            g+=amt;

                            if(g>1.f) {
                                g=1.f;
                            }
                        }

                        if(hu->pixels[i]&4) {
                            b+=amt;

                            if(b>1.f) {
                                b=1.f;
                            }
                        }

                        rgb[0]=(uint8_t)(b*255.f);
                        rgb[1]=(uint8_t)(g*255.f);
                        rgb[2]=(uint8_t)(r*255.f);

                        memcpy(&m_line[m_x+i],rgb,4);
                    }
#else
                    
                    uint32_t *line=m_line+m_x;
#define V(I) m_palette[hu->pixels[I]]
                    
#if FULL_PAL_HEIGHT
                    uint32_t *line2=line+TV_TEXTURE_WIDTH;
#define P(I) line2[I]=line[I]=V(I)
#else
#define P(I) line[I]=V(I)
#endif

                    P(0);
                    P(1);
                    P(2);
                    P(3);
                    P(4);
                    P(5);
                    P(6);
                    P(7);

#undef P
#undef V
                    
#endif
                }
                m_x+=8;
            }

            if(m_state_timer++>=SCAN_OUT_CYCLES) {
                m_state=TVOutputState_HorizontalRetrace;
            }
        }
        break;

    case TVOutputState_HorizontalRetrace:
        m_state=TVOutputState_HorizontalRetraceWait;
        m_x=0;
        m_y+=HEIGHT_SCALE;
        if(m_y>=MAX_NUM_SCANNED_LINES) {
            // VBlank time anyway.
            m_state=TVOutputState_VerticalRetrace;
            break;
        }
        m_line+=TV_TEXTURE_WIDTH*HEIGHT_SCALE;
        m_state_timer=1;
        m_state=TVOutputState_HorizontalRetraceWait;
        break;

    case TVOutputState_HorizontalRetraceWait:
        {
            // Ignore input in this state.
            ++m_state_timer;
            if(m_state_timer>=HORIZONTAL_RETRACE_CYCLES) {
                m_state_timer=0;
                m_state=TVOutputState_BackPorch;
            }
        }
        break;

    case TVOutputState_BackPorch:
        {
            // Ignore input in this state.
            ++m_state_timer;
            if(m_state_timer>=BACK_PORCH_CYCLES) {
                m_state_timer=0;
                m_state=TVOutputState_Scanout;
            }
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::Update(const VideoDataUnit *units,size_t num_units) {
    const VideoDataUnit *unit=units;

    for(size_t i=0;i<num_units;++i,++unit) {
        this->UpdateOneHalfUnit(&unit->a,0.f);
        this->UpdateOneHalfUnit(&unit->b,0.f);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const void *TVOutput::GetTextureData(uint64_t *texture_data_version) const {
    if(texture_data_version) {
        *texture_data_version=m_texture_data_version;
    }

    return m_texture_data.data();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const SDL_PixelFormat *TVOutput::GetPixelFormat() const {
    return m_pixel_format;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TVOutput::IsInVerticalBlank() const {
    return m_state==TVOutputState_VerticalRetrace||m_state==TVOutputState_VerticalRetraceWait;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
