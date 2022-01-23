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
// The data sheet says the LOSE to display on time is "typically" 2.6 usec -
// here modeled by a buffer that delays 4 VideoDataUnits, i.e., 2 usec,
// between processing and output.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static uint16_t teletext_debug_font[128][10];
static uint16_t teletext_debug_font_bgmask[128][10];
#endif

//[(bool)aa][(TeletextCharset)style][ch-32][row]
static uint16_t teletext_font[2][3][96][20];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define _ 0
#define X 1
#define ROW(A,B,C,D,E,F) ((A)<<0|(B)<<1|(C)<<2|(D)<<3|(E)<<4|(F)<<5)

static const uint8_t TELETEXT_FONT_ALPHA[96][10]={
#include "teletext_font.inl"
};

#undef _
#undef X
#undef BYTE
#undef ROW
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t NUM_FLASH_OFF_FRAMES=16;
static const uint8_t NUM_FLASH_CYCLE_FRAMES=64;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static int GetHexChar(int nybble) {
    ASSERT(nybble>=0&&nybble<16);
    if(nybble<10) {
        return '0'+nybble-32;
    } else {
        return 'A'+nybble-10-32;
    }
}
#endif

static int ShouldAntialias(TeletextCharset charset,size_t ch) {
    if(charset==TeletextCharset_Alpha) {
        return 1;
    } else {
        return !(ch&0x20);
    }
}

static uint8_t GetTeletextAlphaFontByte(size_t ch,unsigned y) {
    ASSERT(ch>=32&&ch<128);
    ASSERT(y<10);

    return TELETEXT_FONT_ALPHA[ch-32][y];
}

static uint8_t GetTeletextGraphicsFontByte(uint8_t ch,
                                           unsigned y,
                                           uint8_t blank_column_mask,
                                           unsigned blank_row_mask)
{
    ASSERT(ch>=32&&ch<128);
    ASSERT(y<10);

    if(blank_row_mask&1<<y) {
        return 0;
    }

    uint8_t lmask,rmask;
    if(y<3) {
        lmask=1<<0;
        rmask=1<<1;
    } else if(y<7) {
        lmask=1<<2;
        rmask=1<<3;
    } else {
        lmask=1<<4;
        rmask=1<<6;
    }

    uint8_t byte=0;

    if(ch&lmask) {
        byte|=7<<0;
    }

    if(ch&rmask) {
        byte|=7<<3;
    }

    return byte&~blank_column_mask;
}

static uint8_t GetTeletextFontByte(TeletextCharset charset,uint8_t ch,unsigned y) {
    switch(charset) {
    default:
        ASSERT(false);
    case TeletextCharset_Alpha:
    TeletextCharset_Alpha:
        return GetTeletextAlphaFontByte(ch,y);

    case TeletextCharset_SeparatedGraphics:
        if(ch&0x20) {
            return GetTeletextGraphicsFontByte(ch,y,1<<0|1<<3,1<<2|1<<6|1<<9);
        } else {
            goto TeletextCharset_Alpha;
        }
        break;

    case TeletextCharset_ContiguousGraphics:
        if(ch&0x20) {
            return GetTeletextGraphicsFontByte(ch,y,0,0);
        } else {
            goto TeletextCharset_Alpha;
        }
        break;
    }
}

// Need to revisit this...
static uint16_t Get16WideRow(TeletextCharset charset,uint8_t ch,unsigned y) {
    ASSERT(ch>=32&&ch<128);

    uint16_t w=0;

    if(y>=0&&y<20) {
        size_t left=0;
        uint8_t byte=GetTeletextFontByte(charset,ch,y/2);
        for(size_t i=0;i<6;++i) {
            if(byte&1<<i) {
                w|=3<<left;
            }
            left+=2;
        }
    }

    return w;
}

static uint16_t GetAARow(TeletextCharset charset,uint8_t ch,unsigned y) {
    if(ShouldAntialias(charset,ch)) {
        uint16_t a=Get16WideRow(charset,ch,y);
        uint16_t b=Get16WideRow(charset,ch,y-1+y%2*2);

        return a|(a>>1&b&~(b>>1))|(a<<1&b&~(b<<1));
    } else {
        return Get16WideRow(charset,ch,y);
    }
}

struct InitTeletextFont {
    InitTeletextFont() {
        for(int charset_index=0;charset_index<3;++charset_index) {
            auto charset=(TeletextCharset)charset_index;
            for(uint8_t ch=0;ch<128;++ch) {
                for(unsigned y=0;y<20;++y) {
                    if(ch>=32) {
                        // No AA
                        teletext_font[0][charset][ch-32][y]=Get16WideRow(charset,ch,y);

                        // AA
                        teletext_font[1][charset][ch-32][y]=GetAARow(charset,ch,y);
                    }
                }

#if BBCMICRO_DEBUGGER
                // debug
                {
                    int hc=GetHexChar(ch>>4);
                    int lc=GetHexChar(ch&0x0f);

                    for(int y=0;y<10;++y) {
                        teletext_debug_font[ch][y]=(TELETEXT_FONT_ALPHA[hc][y]|
                                                    TELETEXT_FONT_ALPHA[lc][y]<<6);
                    }

                    for(int y=0;y<10;++y) {
                        uint16_t a=y>0?teletext_debug_font[ch][y-1]:0;
                        uint16_t b=teletext_debug_font[ch][y];
                        uint16_t c=y<9?teletext_debug_font[ch][y+1]:0;

                        teletext_debug_font_bgmask[ch][y]=~(a|b<<1|b|b>>1|c);
                    }
                }
#endif
            }
        }
    }
};

static InitTeletextFont g_init_teletext_font;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SAA5050::SAA5050() {
    // Cheaty reset.
    this->EndOfLine();
    m_raster=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::Byte(uint8_t value,uint8_t dispen) {
    value&=0x7f;

    ASSERT((m_write_index&1)==0);
    Output *output=&m_output[m_write_index];

    output[0].fg=m_fg;
    output[1].fg=m_fg;

    uint16_t data0,data1;

    if(value<32) {
        if(m_conceal||!m_hold) {
            data0=0;
            data1=0;
        } else {
            data0=m_last_graphics_data0;
            data1=m_last_graphics_data1;
        }

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
            m_conceal=false;
            m_last_graphics_data0=0;
            m_last_graphics_data1=0;
            break;

        case 0x08:
            // Flash
            m_text_visible=m_frame_flash_visible;
            break;

        case 0x09:
            //Steady
            m_text_visible=true;
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
            data0=0;
            data1=0;
            m_last_graphics_data0=0;
            m_last_graphics_data1=0;
            break;

        case 0x0d:
            // Double Height
            m_any_double_height=true;
            m_raster_shift=1;
            data0=0;
            data1=0;
            m_last_graphics_data0=0;
            m_last_graphics_data1=0;
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
            m_conceal=false;
        set_charset:;
            m_charset=m_graphics_charset;
            break;

        case 0x18:
            // Conceal Display
            m_conceal=true;
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
            m_bg=0;
            break;

        case 0x1d:
            // New Background
            m_bg=m_fg;
            break;

        case 0x1e:
            // Hold Graphics
            m_hold=true;
            data0=m_last_graphics_data0;
            data1=m_last_graphics_data1;
            break;

        case 0x1f:
            // Release Graphics
            m_hold=false;
            break;
        }

        if(!m_hold) {
            m_last_graphics_data0=0;
            m_last_graphics_data1=0;
        }
    } else {
    display_non_control_char:;
        //size_t offset=(value-32)*20+m_raster;
        //ASSERT(offset<TELETEXT_CHARSET_SIZE);
        uint8_t glyph_raster=(m_raster+m_raster_offset)>>m_raster_shift;

        if(glyph_raster<20&&m_text_visible&&!m_conceal) {
            data0=teletext_font[1][m_charset][value-32][glyph_raster];
            data1=teletext_font[1][m_charset][value-32][glyph_raster+(1>>m_raster_shift)];
        } else {
            data0=0;
            data1=0;
        }

        if(value&0x20&&m_charset!=TeletextCharset_Alpha) {
            if(!m_conceal) {
                m_last_graphics_data0=data0;
                m_last_graphics_data1=data1;
            }
        }
    }

#if BBCMICRO_DEBUGGER
    if(m_debug) {
        size_t ch=value&0x7f;
        size_t row=m_raster/2;
        data0&=teletext_debug_font_bgmask[ch][row];
        data0|=teletext_debug_font[ch][row];
        data1&=teletext_debug_font_bgmask[ch][row];
        data1|=teletext_debug_font[ch][row];
    }
#endif

    if(!dispen) {
        data0=0;
        data1=0;
    }

    output->bg=m_bg;
    output->data0=(uint8_t)data0;
    output->data1=(uint8_t)data1;

    ++output;

    output->bg=m_bg;
    output->data0=(uint8_t)(data0>>6);
    output->data1=(uint8_t)(data1>>6);

    m_write_index=(m_write_index+2)&7;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::EmitPixels(VideoDataUnitPixels *pixels,
                         const VideoDataPixel *palette)
{
    Output *output=&m_output[m_read_index];

    pixels->pixels[0]=palette[output->bg];
    pixels->pixels[0].bits.x=VideoDataType_Teletext;

    pixels->pixels[1]=palette[output->fg];

    pixels->pixels[2].all=output->data0;
    pixels->pixels[3].all=output->data1;

    m_read_index=(m_read_index+1)&7;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::StartOfLine() {
    m_conceal=false;
    m_fg=7;
    m_bg=0;
    m_graphics_charset=TeletextCharset_ContiguousGraphics;
    m_charset=TeletextCharset_Alpha;
    m_last_graphics_data0=0;
    m_last_graphics_data1=0;
    m_hold=false;
    m_text_visible=true;
    m_raster_shift=0;

    m_read_index=0;
    m_write_index=4;

    memset(m_output,0,sizeof m_output);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::EndOfLine() {
    m_bg=0;
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

        m_any_double_height=false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SAA5050::VSync() {
    m_raster=0;

    ++m_frame;
    if(m_frame>=NUM_FLASH_CYCLE_FRAMES) {
        m_frame=0;
    }

    m_frame_flash_visible=m_frame>=NUM_FLASH_OFF_FRAMES;

    m_any_double_height=false;
    m_raster_offset=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool SAA5050::IsDebug() const {
    return m_debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void SAA5050::SetDebug(bool debug) {
    m_debug=debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
