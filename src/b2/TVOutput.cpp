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
#if VIDEO_TRACK_METADATA
    m_texture_metadata.resize(m_texture_data.size());
#endif

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

static const int HEIGHT_SCALE=2;

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

#define NOTHING_PALETTE_INDEX (0)

#if BUILD_TYPE_Debug
#ifdef _MSC_VER
#pragma optimize("tsg",on)
#endif
#endif

void TVOutput::UpdateOneUnit(const VideoDataUnit *unit,float amt) {
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
#if VIDEO_TRACK_METADATA
        m_metadata_line=m_texture_metadata.data();
#endif

        // "Fun" (translation: brain-eating) fake interlace

        // if(m_num_fields&1) {
        //     m_line+=m_texture_pitch;
        //     ++m_y;
        // }

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
            switch(unit->type.x) {
            case VideoDataType_Data:
                {
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

                            if(unit->pixels[i]&1) {
                                r+=amt;

                                if(r>1.f) {
                                    r=1.f;
                                }
                            }

                            if(unit->pixels[i]&2) {
                                g+=amt;

                                if(g>1.f) {
                                    g=1.f;
                                }
                            }

                            if(unit->pixels[i]&4) {
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
                        uint32_t *line2=line+TV_TEXTURE_WIDTH;

#if TV_OUTPUT_ONE_BIG_TABLE

                        union PixelIndex {
                            uint16_t index;
                            VideoDataBitmapPixel pixel;
                        };

                        auto pixel=(PixelIndex *)unit->bitmap.pixels;

                        *line2++=*line++=m_rgbs[pixel++->index];
                        *line2++=*line++=m_rgbs[pixel++->index];
                        *line2++=*line++=m_rgbs[pixel++->index];
                        *line2++=*line++=m_rgbs[pixel++->index];
                        *line2++=*line++=m_rgbs[pixel++->index];
                        *line2++=*line++=m_rgbs[pixel++->index];
                        *line2++=*line++=m_rgbs[pixel++->index];
                        *line2++=*line++=m_rgbs[pixel++->index];

#else

                        VideoDataBitmapPixel tmp;

                        tmp=unit->bitmap.pixels[0];
                        line2[0]=line[0]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

                        tmp=unit->bitmap.pixels[1];
                        line2[1]=line[1]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

                        tmp=unit->bitmap.pixels[2];
                        line2[2]=line[2]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

                        tmp=unit->bitmap.pixels[3];
                        line2[3]=line[3]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

                        tmp=unit->bitmap.pixels[4];
                        line2[4]=line[4]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

                        tmp=unit->bitmap.pixels[5];
                        line2[5]=line[5]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

                        tmp=unit->bitmap.pixels[6];
                        line2[6]=line[6]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

                        tmp=unit->bitmap.pixels[7];
                        line2[7]=line[7]=m_rs[tmp.r]|m_gs[tmp.g]|m_bs[tmp.b];

#endif

#endif

#if VIDEO_TRACK_METADATA
                        VideoDataUnitMetadata *metadata_line=m_metadata_line+m_x;

                        metadata_line[0]=unit->bitmap.metadata;
                        metadata_line[1]=unit->bitmap.metadata;
                        metadata_line[2]=unit->bitmap.metadata;
                        metadata_line[3]=unit->bitmap.metadata;
                        metadata_line[4]=unit->bitmap.metadata;
                        metadata_line[5]=unit->bitmap.metadata;
                        metadata_line[6]=unit->bitmap.metadata;
                        metadata_line[7]=unit->bitmap.metadata;

                        metadata_line+=TV_TEXTURE_WIDTH;

                        metadata_line[0]=unit->bitmap.metadata;
                        metadata_line[1]=unit->bitmap.metadata;
                        metadata_line[2]=unit->bitmap.metadata;
                        metadata_line[3]=unit->bitmap.metadata;
                        metadata_line[4]=unit->bitmap.metadata;
                        metadata_line[5]=unit->bitmap.metadata;
                        metadata_line[6]=unit->bitmap.metadata;
                        metadata_line[7]=unit->bitmap.metadata;
#endif

                    }
                    m_x+=8;
                }
                break;

            case VideoDataType_HSync:
                {
                    m_state=TVOutputState_HorizontalRetrace;
                }
                break;

            case VideoDataType_VSync:
                {
                    m_state=TVOutputState_VerticalRetrace;
                }
                break;

            case VideoDataType_Nothing:
                {
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line=m_line+m_x;

                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][NOTHING_PALETTE_INDEX];

                        line+=TV_TEXTURE_WIDTH;
                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][NOTHING_PALETTE_INDEX];

#if VIDEO_TRACK_METADATA
                        VideoDataUnitMetadata *metadata_line=m_metadata_line+m_x;

                        metadata_line[7]=metadata_line[6]=metadata_line[5]=metadata_line[4]=metadata_line[3]=metadata_line[2]=metadata_line[1]=metadata_line[0]=NULL_VIDEO_METADATA;
                        metadata_line+=TV_TEXTURE_WIDTH;
                        metadata_line[7]=metadata_line[6]=metadata_line[5]=metadata_line[4]=metadata_line[3]=metadata_line[2]=metadata_line[1]=metadata_line[0]=NULL_VIDEO_METADATA;
#endif
                    }
                    m_x+=8;
                }
                break;

            case VideoDataType_Cursor:
                {
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line=m_line+m_x;

                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][7];

                        line+=TV_TEXTURE_WIDTH;
                        line[7]=line[6]=line[5]=line[4]=line[3]=line[2]=line[1]=line[0]=m_palette[0][7];

#if VIDEO_TRACK_METADATA
                        VideoDataUnitMetadata *metadata_line=m_metadata_line+m_x;

                        metadata_line[7]=metadata_line[6]=metadata_line[5]=metadata_line[4]=metadata_line[3]=metadata_line[2]=metadata_line[1]=metadata_line[0]=NULL_VIDEO_METADATA;
                        metadata_line+=TV_TEXTURE_WIDTH;
                        metadata_line[7]=metadata_line[6]=metadata_line[5]=metadata_line[4]=metadata_line[3]=metadata_line[2]=metadata_line[1]=metadata_line[0]=NULL_VIDEO_METADATA;
#endif
                    }
                    m_x+=8;
                }
                break;

            case VideoDataType_Teletext:
                {
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line0=m_line+m_x;
                        uint32_t *line1=line0+TV_TEXTURE_WIDTH;

                        uint8_t c00=unit->teletext.colours[unit->teletext.data0&1];
                        uint8_t c01=unit->teletext.colours[unit->teletext.data0>>1&1];
                        uint8_t c02=unit->teletext.colours[unit->teletext.data0>>2&1];
                        uint8_t c03=unit->teletext.colours[unit->teletext.data0>>3&1];
                        uint8_t c04=unit->teletext.colours[unit->teletext.data0>>4&1];
                        uint8_t c05=unit->teletext.colours[unit->teletext.data0>>5&1];

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

                        uint8_t c10=unit->teletext.colours[unit->teletext.data1&1];
                        uint8_t c11=unit->teletext.colours[unit->teletext.data1>>1&1];
                        uint8_t c12=unit->teletext.colours[unit->teletext.data1>>2&1];
                        uint8_t c13=unit->teletext.colours[unit->teletext.data1>>3&1];
                        uint8_t c14=unit->teletext.colours[unit->teletext.data1>>4&1];
                        uint8_t c15=unit->teletext.colours[unit->teletext.data1>>5&1];

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

#if VIDEO_TRACK_METADATA
                        VideoDataUnitMetadata *metadata_line=m_metadata_line+m_x;

                        metadata_line[0]=unit->teletext.metadata;
                        metadata_line[1]=unit->teletext.metadata;
                        metadata_line[2]=unit->teletext.metadata;
                        metadata_line[3]=unit->teletext.metadata;
                        metadata_line[4]=unit->teletext.metadata;
                        metadata_line[5]=unit->teletext.metadata;
                        metadata_line[6]=unit->teletext.metadata;
                        metadata_line[7]=unit->teletext.metadata;

                        metadata_line+=TV_TEXTURE_WIDTH;

                        metadata_line[0]=unit->teletext.metadata;
                        metadata_line[1]=unit->teletext.metadata;
                        metadata_line[2]=unit->teletext.metadata;
                        metadata_line[3]=unit->teletext.metadata;
                        metadata_line[4]=unit->teletext.metadata;
                        metadata_line[5]=unit->teletext.metadata;
                        metadata_line[6]=unit->teletext.metadata;
                        metadata_line[7]=unit->teletext.metadata;
#endif


                    }
                    m_x+=8;
                }
                break;

            case VideoDataType_NuLAAttribute:
                {
                    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
                        uint32_t *line0=m_line+m_x;
                        uint32_t *line1=line0+TV_TEXTURE_WIDTH;


#define PIXEL(INDEX,A,B,C)\
    VideoDataBitmapPixel p##INDEX##a=unit->bitmap.pixels[1+(A)];\
    VideoDataBitmapPixel p##INDEX##b=unit->bitmap.pixels[1+(B)];\
    VideoDataBitmapPixel p##INDEX##c=unit->bitmap.pixels[1+(C)];\
    int32_t r##INDEX=((p##INDEX##a.r<<4|p##INDEX##a.r)+(p##INDEX##b.r<<4|p##INDEX##b.r)+(p##INDEX##c.r<<4|p##INDEX##c.r))/3;\
    int32_t g##INDEX=((p##INDEX##a.g<<4|p##INDEX##a.g)+(p##INDEX##b.g<<4|p##INDEX##b.g)+(p##INDEX##c.g<<4|p##INDEX##c.g))/3;\
    int32_t b##INDEX=((p##INDEX##a.b<<4|p##INDEX##a.b)+(p##INDEX##b.b<<4|p##INDEX##b.b)+(p##INDEX##c.b<<4|p##INDEX##c.b))/3;\
    /*ASSERT(r##INDEX<256);*/\
    /*ASSERT(g##INDEX<256);*/\
    /*ASSERT(b##INDEX<256);*/\
    line1[INDEX]=line0[INDEX]=(uint32_t)r##INDEX<<m_rshift|(uint32_t)g##INDEX<<m_gshift|(uint32_t)b##INDEX<<m_bshift|m_alpha;

                        PIXEL(0,0,0,0);
                        PIXEL(1,0,1,1);
                        PIXEL(2,1,1,2);
                        PIXEL(3,2,2,2);
                        PIXEL(4,3,3,3);
                        PIXEL(5,3,4,4);
                        PIXEL(6,4,4,5);
                        PIXEL(7,5,5,5);

#undef PIXEL

#if VIDEO_TRACK_METADATA
                        VideoDataUnitMetadata *metadata_line=m_metadata_line+m_x;

                        metadata_line[0]=unit->bitmap.metadata;
                        metadata_line[1]=unit->bitmap.metadata;
                        metadata_line[2]=unit->bitmap.metadata;
                        metadata_line[3]=unit->bitmap.metadata;
                        metadata_line[4]=unit->bitmap.metadata;
                        metadata_line[5]=unit->bitmap.metadata;
                        metadata_line[6]=unit->bitmap.metadata;
                        metadata_line[7]=unit->bitmap.metadata;

                        metadata_line+=TV_TEXTURE_WIDTH;

                        metadata_line[0]=unit->bitmap.metadata;
                        metadata_line[1]=unit->bitmap.metadata;
                        metadata_line[2]=unit->bitmap.metadata;
                        metadata_line[3]=unit->bitmap.metadata;
                        metadata_line[4]=unit->bitmap.metadata;
                        metadata_line[5]=unit->bitmap.metadata;
                        metadata_line[6]=unit->bitmap.metadata;
                        metadata_line[7]=unit->bitmap.metadata;
#endif
                    }
                    m_x+=8;
                }
                break;
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
#if VIDEO_TRACK_METADATA
        m_metadata_line+=TV_TEXTURE_WIDTH*HEIGHT_SCALE;
#endif
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

#if BUILD_TYPE_Debug
#ifdef _MSC_VER
#pragma optimize("",on)
#endif
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::Update(const VideoDataUnit *units,size_t num_units) {
    const VideoDataUnit *unit=units;

    for(size_t i=0;i<num_units;++i) {
        this->UpdateOneUnit(unit++,1.f);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void TVOutput::AddBeamMarker() {
    if(m_x<TV_TEXTURE_WIDTH&&m_y<TV_TEXTURE_HEIGHT) {
        m_texture_data[m_x+m_y*TV_TEXTURE_WIDTH]=0xffffffff;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

static const uint8_t DIGITS[10][13]={
    {0x00,0x00,0x04,0x0A,0x11,0x11,0x11,0x11,0x11,0x0A,0x04,0x00,0x00,},// 48 (0x30) '0'
    {0x00,0x00,0x04,0x06,0x05,0x04,0x04,0x04,0x04,0x04,0x1F,0x00,0x00,},// 49 (0x31) '1'
    {0x00,0x00,0x0E,0x11,0x11,0x10,0x08,0x04,0x02,0x01,0x1F,0x00,0x00,},// 50 (0x32) '2'
    {0x00,0x00,0x1F,0x10,0x08,0x04,0x0E,0x10,0x10,0x11,0x0E,0x00,0x00,},// 51 (0x33) '3'
    {0x00,0x00,0x08,0x08,0x0C,0x0A,0x0A,0x09,0x1F,0x08,0x08,0x00,0x00,},// 52 (0x34) '4'
    {0x00,0x00,0x1F,0x01,0x01,0x0D,0x13,0x10,0x10,0x11,0x0E,0x00,0x00,},// 53 (0x35) '5'
    {0x00,0x00,0x0E,0x11,0x01,0x01,0x0F,0x11,0x11,0x11,0x0E,0x00,0x00,},// 54 (0x36) '6'
    {0x00,0x00,0x1F,0x10,0x08,0x08,0x04,0x04,0x02,0x02,0x02,0x00,0x00,},// 55 (0x37) '7'
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E,0x11,0x11,0x11,0x0E,0x00,0x00,},// 56 (0x38) '8'
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x1E,0x10,0x10,0x11,0x0E,0x00,0x00,},// 57 (0x39) '9'
};

void TVOutput::FillWithTestPattern() {
    m_texture_data.clear();
    m_texture_data.reserve(TV_TEXTURE_WIDTH*TV_TEXTURE_HEIGHT);

    for(int y=0;y<TV_TEXTURE_HEIGHT;++y) {
        for(int x=0;x<TV_TEXTURE_WIDTH;++x) {
            if((x^y)&1) {
                m_texture_data.push_back(m_palette[0][1]);
            } else {
                m_texture_data.push_back(m_palette[0][3]);
            }
        }
    }

    uint32_t colours[]={m_palette[0][1],m_palette[0][6]};

    for(size_t i=0;i<sizeof colours/sizeof colours[0];++i) {
        uint32_t colour=colours[i];

        for(size_t x=i;x<TV_TEXTURE_WIDTH-i;++x) {
            m_texture_data[i*TV_TEXTURE_WIDTH+x]=colour;
            m_texture_data[(TV_TEXTURE_HEIGHT-1-i)*TV_TEXTURE_WIDTH+x]=colour;
        }

        for(size_t y=i;y<TV_TEXTURE_HEIGHT-i;++y) {
            m_texture_data[y*TV_TEXTURE_WIDTH+i]=colour;
            m_texture_data[y*TV_TEXTURE_WIDTH+TV_TEXTURE_WIDTH-1-i]=colour;
        }
    }

    {
        for(size_t cy=0;cy<TV_TEXTURE_HEIGHT;cy+=50) {
            size_t tmp=cy;
            size_t cx=100;
            do {
                const uint8_t *digit=DIGITS[tmp%10];

                for(size_t gy=0;gy<13;++gy) {
                    if(cy+gy>=TV_TEXTURE_HEIGHT) {
                        break;
                    }

                    uint32_t *line=&m_texture_data[cx+(cy+gy)*TV_TEXTURE_WIDTH];
                    uint8_t row=*digit++;

                    for(size_t gx=0;gx<6;++gx) {
                        if(row&1) {
                            *line=m_palette[0][0];
                        }

                        row>>=1;
                        ++line;
                    }
                }

                cx-=6;
                tmp/=10;
            } while(tmp!=0);
        }
    }
}
#endif

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

#if VIDEO_TRACK_METADATA
const VideoDataUnitMetadata *TVOutput::GetTextureMetadata() const {
    return m_texture_metadata.data();
}
#endif

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

#if TV_OUTPUT_ONE_BIG_TABLE

    for(uint8_t r=0;r<16;++r) {
        for(uint8_t g=0;g<16;++g) {
            for(uint8_t b=0;b<16;++b) {
                VideoDataBitmapPixel pixel={};

                pixel.r=r;
                pixel.g=g;
                pixel.b=b;

                uint16_t index;
                memcpy(&index,&pixel,2);
                ASSERT(index<4096);

                m_rgbs[index]=SDL_MapRGBA(m_pixel_format,r<<4|r,g<<4|g,b<<4|b,255);
            }
        }
    }

#else

    for(uint8_t i=0;i<16;++i) {
        uint8_t value=i<<4|i;

        m_rs[i]=SDL_MapRGBA(m_pixel_format,value,0,0,255);
        m_gs[i]=SDL_MapRGBA(m_pixel_format,0,value,0,255);
        m_bs[i]=SDL_MapRGBA(m_pixel_format,0,0,value,255);
    }

#endif

    m_rshift=m_pixel_format->Rshift;
    m_gshift=m_pixel_format->Gshift;
    m_bshift=m_pixel_format->Bshift;
    m_alpha=SDL_MapRGBA(m_pixel_format,0,0,0,255);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
