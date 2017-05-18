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
#include <shared/log.h>

#include <shared/enum_def.h>
#include "TVOutput.inl"
#include <shared/enum_end.h>

LOG_EXTERN(OUTPUT);

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

    this->InitPalette();

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

                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][0];
#if FULL_PAL_HEIGHT
                        line+=TV_TEXTURE_WIDTH;
                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][0];
#endif
                    }
                    m_x+=8;
                    break;

                case BeebControlPixel_Cursor:
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line=m_line+m_x;

                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][7];
#if FULL_PAL_HEIGHT
                        line+=TV_TEXTURE_WIDTH;
                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][7];
#endif
                    }
                    m_x+=8;
                    break;

#if BBCMICRO_FINER_TELETEXT
                case BeebControlPixel_Teletext:
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line0=m_line+m_x;
#if FULL_PAL_HEIGHT
                        uint32_t *line1=line0+TV_TEXTURE_WIDTH;
#endif

#if BBCMICRO_PRESTRETCH_TELETEXT
                        
#define P_(N,I) line##N[I]=m_palette[0][hu->teletext.colours[((hu->teletext.data##N)>>(I))&1]]
                        
#if FULL_PAL_HEIGHT
#define P(I) P_(0,I); P_(1,I)
#else
                        // It's going to look kind of crap. But it
                        // won't break.
#define P(I) P_(0,I)
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
#undef P_

#else

                        uint8_t c00=hu->teletext.colours[hu->teletext.data0&1];
                        uint8_t c01=hu->teletext.colours[hu->teletext.data0>>1&1];
                        uint8_t c02=hu->teletext.colours[hu->teletext.data0>>2&1];
                        uint8_t c03=hu->teletext.colours[hu->teletext.data0>>3&1];
                        uint8_t c04=hu->teletext.colours[hu->teletext.data0>>4&1];
                        uint8_t c05=hu->teletext.colours[hu->teletext.data0>>5&1];

                        uint8_t c010=c01<<3|c00;
                        uint8_t c012=c01<<3|c02;
                        uint8_t c043=c04<<3|c03;
                        uint8_t c045=c04<<3|c05;

                        line0[0]=m_palette[0][c010]; // (c0+c0+c0)/3
                        line0[1]=m_palette[1][c010]; // (c0+c1+c1)/3
                        line0[2]=m_palette[1][c012]; // (c1+c1+c2)/3
                        line0[3]=m_palette[0][c012]; // (c2+c2+c2)/3

                        line0[4]=m_palette[0][c043];     
                        line0[5]=m_palette[1][c043];   
                        line0[6]=m_palette[1][c045];   
                        line0[7]=m_palette[0][c045];     
                        
#if FULL_PAL_HEIGHT
                        uint8_t c10=hu->teletext.colours[hu->teletext.data1&1];
                        uint8_t c11=hu->teletext.colours[hu->teletext.data1>>1&1];
                        uint8_t c12=hu->teletext.colours[hu->teletext.data1>>2&1];
                        uint8_t c13=hu->teletext.colours[hu->teletext.data1>>3&1];
                        uint8_t c14=hu->teletext.colours[hu->teletext.data1>>4&1];
                        uint8_t c15=hu->teletext.colours[hu->teletext.data1>>5&1];

                        uint8_t c110=c11<<3|c10;
                        uint8_t c112=c11<<3|c12;
                        uint8_t c143=c14<<3|c13;
                        uint8_t c145=c14<<3|c15;
                        
                        line1[0]=m_palette[0][c110];
                        line1[1]=m_palette[1][c110];
                        line1[2]=m_palette[1][c112];
                        line1[3]=m_palette[0][c112];
                        line1[4]=m_palette[0][c143];
                        line1[5]=m_palette[1][c143];
                        line1[6]=m_palette[1][c145];
                        line1[7]=m_palette[0][c145];
#endif
                        
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
#define V(I) m_palette[0][hu->pixels[I]]
                    
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

double TVOutput::GetGamma() const {
    return m_gamma;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::SetGamma(double gamma) {
    m_gamma=gamma;

    this->InitPalette();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t GetByte(double x) {
    if(x<0.) {
        return 0;
    } else if(x>=1.) {
        return 255;
    } else {
        return (uint8_t)(x*255.);
    }
}

void TVOutput::InitPalette(size_t palette,double fa) {
    for(size_t i=0;i<64;++i) {
        double ra=i&1?1.:0.;
        double ga=i&2?1.:0.;
        double ba=i&4?1.:0.;
            
        double rb=i&8?1.:0.;
        double gb=i&16?1.:0.;
        double bb=i&32?1.:0.;

        double pr=fa*ra+(1.-fa)*rb;
        double pg=fa*ga+(1.-fa)*gb;
        double pb=fa*ba+(1.-fa)*bb;

        pr=pow(pr,1./m_gamma);
        pg=pow(pg,1./m_gamma);
        pb=pow(pb,1./m_gamma);

        uint8_t prb=GetByte(pr);
        uint8_t pgb=GetByte(pg);
        uint8_t pbb=GetByte(pb);

        m_palette[palette][i]=SDL_MapRGBA(m_pixel_format,prb,pgb,pbb,255);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::InitPalette() {
    this->InitPalette(0,1.);
    this->InitPalette(1,1./3);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
