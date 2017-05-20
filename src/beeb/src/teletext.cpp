#include <shared/system.h>
#include <shared/debug.h>
#include <string.h>
#include <stdio.h>
#include <beeb/teletext.h>
#include <beeb/video.h>

#include <shared/enum_def.h>
#include <beeb/teletext.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Random notes
// ------------
//
// 1. test card: http://stardot.org.uk/forums/viewtopic.php?f=4&t=4686
// (this code reproduces it accurately... I think? I haven't had
// emulator and TV side by side, and there was some inter-chair
// movement involved...)
//
// 2. I've gone by the docs rather than necessarily going through and
// fully testing every case. For example, which codes, exactly,
// terminate Conceal mode?
//
// 3. Seems there are bugs anyway, e.g.,
// http://www.stardot.org.uk/forums/viewtopic.php?f=53&p=163095&sid=2cced0ed559a6a56acda6e491133914f#p163092
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint16_t teletext_debug_font[128][10];
static uint16_t teletext_debug_font_bgmask[128][10];

//[(bool)aa][(TeletextCharset)style][ch-32][row]
static uint16_t teletext_font[2][3][96][20];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define _ 0
#define X 1
#define BYTE(A,B,C,D,E,F) ((A)<<0|(B)<<1|(C)<<2|(D)<<3|(E)<<4|(F)<<5)
#define ROW(N0,N1,N2,N3,N4,N5,G0,G1,G2,G3,G4,G5,S0,S1,S2,S3,S4,S5) {BYTE(N0,N1,N2,N3,N4,N5),BYTE(G0,G1,G2,G3,G4,G5),BYTE(S0,S1,S2,S3,S4,S5)}

static const uint8_t TELETEXT_FONT[96][10][3]={
#include "teletext_font.inl"
};

#undef _
#undef X
#undef BYTE
#undef ROW
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t FRAMES_PER_SECOND=50;
static const uint8_t NUM_FLASH_OFF_FRAMES=16;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Column widths for characters by type.
//
// For alpha chars, left column is the blank one.
//
// Graphics chars ought to have the same weight for columns 3 and 6 so
// that separated graphics don't look too ugly.

#if BBCMICRO_PRESTRETCH_TELETEXT
static const size_t ALPHA_WIDTHS[]={
    3,2,3,2,3,2,
};
#endif

#if BBCMICRO_PRESTRETCH_TELETEXT
static const size_t GRAPHIC_WIDTHS[]={
    3,3,2,3,3,2,
};
#endif

#if !BBCMICRO_PRESTRETCH_TELETEXT
static const size_t UNIFORM_WIDTHS[]={
    2,2,2,2,2,2,
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_PRESTRETCH_TELETEXT
static constexpr size_t CONSUMER_SHIFT=8;
#else
static constexpr size_t CONSUMER_SHIFT=6;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int GetHexChar(int nybble) {
    ASSERT(nybble>=0&&nybble<16);
    if(nybble<10) {
        return '0'+nybble-32;
    } else {
        return 'A'+nybble-10-32;
    }
}

static int ShouldAntialias(size_t style,size_t ch) {
    if(style==TeletextCharset_Alpha) {
        return 1;
    } else {
        return !(ch&0x20);
    }
}

// Need to revisit this...
static uint16_t Get16WideRow(size_t style,size_t ch,int y) {
    ASSERT(style<3);
    ASSERT(ch>=32&&ch<128);

    uint16_t w=0;

    const size_t *widths;
#if BBCMICRO_PRESTRETCH_TELETEXT
    if(style==TeletextCharset_Alpha||(ch&0x20)==0) {
        widths=ALPHA_WIDTHS;
    } else {
        widths=GRAPHIC_WIDTHS;
    }
#else
    widths=UNIFORM_WIDTHS;
#endif

    if(y>=0&&y<20) {
        size_t left=0;
        for(size_t i=0;i<6;++i) {
            if(TELETEXT_FONT[ch-32][y/2][style]&1<<i) {
                w|=((1<<widths[i])-1)<<left;
            }
            left+=widths[i];
        }
    }

    return w;
}

static uint16_t GetAARow(size_t style,size_t ch,int y) {
    if(ShouldAntialias(style,ch)) {
        uint16_t a=Get16WideRow(style,ch,y);
        uint16_t b=Get16WideRow(style,ch,y-1+y%2*2);

        return a|(a>>1&b&~(b>>1))|(a<<1&b&~(b<<1));
    } else {
        return Get16WideRow(style,ch,y);
    }
}

struct InitTeletextFont {
    InitTeletextFont() {
        for(size_t style=0;style<3;++style) {
            for(uint8_t ch=0;ch<128;++ch) {
                for(int y=0;y<20;++y) {
                    if(ch>=32) {
                        // No AA
                        teletext_font[0][style][ch-32][y]=Get16WideRow(style,ch,y);

                        // AA
                        teletext_font[1][style][ch-32][y]=GetAARow(style,ch,y);
                    }
                }

                // debug
                {
                    int hc=GetHexChar(ch>>4);
                    int lc=GetHexChar(ch&0x0f);

                    for(int y=0;y<10;++y) {
                        teletext_debug_font[ch][y]=TELETEXT_FONT[hc][y][0]<<(CONSUMER_SHIFT-6)|TELETEXT_FONT[lc][y][0]<<CONSUMER_SHIFT;
                    }

                    for(int y=0;y<10;++y) {
                        uint16_t a=y>0?teletext_debug_font[ch][y-1]:0;
                        uint16_t b=teletext_debug_font[ch][y];
                        uint16_t c=y<9?teletext_debug_font[ch][y+1]:0;

                        teletext_debug_font_bgmask[ch][y]=~(a|b<<1|b|b>>1|c);
                    }
                }
            }
        }
    }
};

static InitTeletextFont g_init_teletext_font;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SAA5050::SAA5050() {
#if !BBCMICRO_FINER_TELETEXT
    m_aa=1;
#endif

    this->HSync();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::Byte(uint8_t value) {
    value&=0x7f;

    if(value<32) {
        if(m_conceal||!m_hold) {
#if BBCMICRO_FINER_TELETEXT
            m_data0=0;
            m_data1=0;
#else
            m_data=0;
#endif
        } else {
#if BBCMICRO_FINER_TELETEXT
            m_data0=m_last_graphics_data0;
            m_data1=m_last_graphics_data1;
#else
            m_data=m_last_graphics_data;
#endif
        }
        m_data_colours[0]=m_bg;
        m_data_colours[1]=m_fg;

        switch(value) {
        case 0x00:
            // NUL
            break;

        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // Alpha
            m_fg=value;
            m_charset=TeletextCharset_Alpha;
            m_conceal=0;
#if BBCMICRO_FINER_TELETEXT
            m_last_graphics_data0=0;
            m_last_graphics_data1=0;
#else
            m_last_graphics_data=0;
#endif
            break;

        case 0x08:
            // Flash
            m_flash=m_frame_flash;
            break;

        case 0x09:
            //Steady
            m_flash=1;
            break;

        case 0x0a:
            // End Box
            break;

        case 0x0b:
            // Start Box
            break;

        case 0x0c:
            // Normal Height
            m_raster_shift=0;
            break;

        case 0x0d:
            // Double Height
            m_any_double_height=1;
            m_raster_shift=1;
            break;

        case 0x0e:
            // S0
            break;

        case 0x0f:
            // S1
            break;

        case 0x10:
            // DLE
            break;

        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            // Graphics
            m_fg=value&7;
            m_conceal=0;
        set_charset:;
            m_charset=m_graphics_charset;
            break;

        case 0x18:
            // Conceal Display
            m_conceal=1;
            goto display_non_control_char;

        case 0x19:
            // Contiguous Graphics
            m_graphics_charset=TeletextCharset_ContiguousGraphics;
            goto set_charset;

        case 0x1a:
            // Separated Graphics
            m_graphics_charset=TeletextCharset_SeparatedGraphics;
            goto set_charset;

        case 0x1b:
            // ESC
            break;

        case 0x1c:
            // Black Background
            m_data_colours[0]=m_bg=0;
            break;

        case 0x1d:
            // New Background
            m_data_colours[0]=m_bg=m_fg;
            break;

        case 0x1e:
            // Hold Graphics
            m_hold=1;
            break;

        case 0x1f:
            // Release Graphics
            m_hold=0;
            break;
        }
    } else {
    display_non_control_char:;
        //size_t offset=(value-32)*20+m_raster;
        //ASSERT(offset<TELETEXT_CHARSET_SIZE);
        uint8_t glyph_raster=(m_raster+m_raster_offset)>>m_raster_shift;

        if(glyph_raster<20&&m_flash&&!m_conceal) {
#if BBCMICRO_FINER_TELETEXT
            m_data0=teletext_font[1][m_charset][value-32][glyph_raster];
            m_data1=teletext_font[1][m_charset][value-32][glyph_raster+(1>>m_raster_shift)];
#else
            m_data=teletext_font[m_aa][m_charset][value-32][glyph_raster];
#endif
        } else {
#if BBCMICRO_FINER_TELETEXT
            m_data0=0;
            m_data1=0;
#else
            m_data=0;
#endif
        }

        m_data_colours[0]=m_bg;
        m_data_colours[1]=m_fg;

        if(value&0x20) {
            if(!m_conceal) {
#if BBCMICRO_FINER_TELETEXT
                m_last_graphics_data0=m_data0;
                m_last_graphics_data1=m_data1;
#else
                m_last_graphics_data=m_data;
#endif
            }
        }
    }

    if(m_debug) {
        size_t ch=value&0x7f;
        size_t row=m_raster/2;
#if BBCMICRO_FINER_TELETEXT
        m_data0&=teletext_debug_font_bgmask[ch][row];
        m_data0|=teletext_debug_font[ch][row];
        m_data1&=teletext_debug_font_bgmask[ch][row];
        m_data1|=teletext_debug_font[ch][row];
#else
        m_data&=teletext_debug_font_bgmask[ch][row];
        m_data|=teletext_debug_font[ch][row];
#endif
    }
}

void SAA5050::EmitVideoDataHalfUnit(VideoDataHalfUnit *hu) {
#if BBCMICRO_FINER_TELETEXT

    hu->teletext.pixel0=BeebControlPixel_Teletext;
    hu->teletext.colours[0]=m_data_colours[0];
    hu->teletext.colours[1]=m_data_colours[1];
    hu->teletext.data0=(uint8_t)m_data0;
    hu->teletext.data1=(uint8_t)m_data1;

    m_data0>>=CONSUMER_SHIFT;
    m_data1>>=CONSUMER_SHIFT;

#else

    hu->pixels[0]=m_data_colours[(m_data>>0)&1];
    hu->pixels[1]=m_data_colours[(m_data>>1)&1];
    hu->pixels[2]=m_data_colours[(m_data>>2)&1];
    hu->pixels[3]=m_data_colours[(m_data>>3)&1];
    hu->pixels[4]=m_data_colours[(m_data>>4)&1];
    hu->pixels[5]=m_data_colours[(m_data>>5)&1];
    hu->pixels[6]=m_data_colours[(m_data>>6)&1];
    hu->pixels[7]=m_data_colours[(m_data>>7)&1];

    m_data>>=8;

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::HSync() {
    m_conceal=0;
    m_fg=7;
    m_bg=0;
    m_graphics_charset=TeletextCharset_ContiguousGraphics;
    m_charset=TeletextCharset_Alpha;
#if BBCMICRO_FINER_TELETEXT
    m_last_graphics_data0=0;
    m_last_graphics_data1=0;
#else
    m_last_graphics_data=0;
#endif
    m_hold=0;
    m_flash=1;
    m_raster_shift=0;

    m_raster+=2;
    if(m_raster>=20) {
        m_raster-=20;

        if(m_any_double_height&&m_raster_offset==0) {
            // Use second row of double height.
            m_raster_offset=20;
        } else {
            // Use first row of double height.
            m_raster_offset=0;
        }

        m_any_double_height=0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::VSync(uint8_t odd_frame) {
#if BBCMICRO_FINER_TELETEXT

    (void)odd_frame;

    m_raster=0;

#else

    // m->saa5050.raster=m->crtc.registers.bits.r8.bits.v&&m->crtc.registers.bits.r8.bits.s&&(m->crtc.num_frames&1);

    if(m_aa) {
        m_raster=odd_frame;
    } else {
        m_raster=0;
    }

#endif

    ++m_frame;
    if(m_frame>=FRAMES_PER_SECOND) {
        m_frame=0;
    }

    m_frame_flash=m_frame<NUM_FLASH_OFF_FRAMES;

    m_any_double_height=0;
    m_raster_offset=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SAA5050::IsDebug() const {
    return m_debug;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::SetDebug(bool debug) {
    m_debug=debug;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !BBCMICRO_FINER_TELETEXT
bool SAA5050::IsAA() const {
    return m_aa;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !BBCMICRO_FINER_TELETEXT
void SAA5050::SetAA(bool aa) {
    m_aa=aa;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
