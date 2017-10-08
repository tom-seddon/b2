#ifndef HEADER_2B1F0A986AD74CCF8938EB7F804A61E2
#define HEADER_2B1F0A986AD74CCF8938EB7F804A61E2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "video.h"

union VideoDataHalfUnit;
union M6502Word;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The VideoULA class actually emulates a Video NuLA... but provided
// the WriteNuLAXXX mmio functions aren't mapped in, it's impossible
// to tell the difference.

class VideoULA {
public:
#include <shared/pushwarn_bitfields.h>
    struct ControlBits {
        uint8_t flash:1;
        uint8_t teletext:1;
        uint8_t line_width:2;
        uint8_t fast_6845:1;
        uint8_t cursor:3;
    };
#include <shared/popwarn.h>

    struct NuLAAttributeModeBits {
        uint8_t enabled:1;
        uint8_t text:1;
    };

    union NuLAAttributeMode {
        NuLAAttributeModeBits bits;
        uint8_t value;
    };

    union Control {
        ControlBits bits;
        uint8_t value;
    };

    Control control={};

    static void WriteControlRegister(void *ula,M6502Word a,uint8_t value);
    static void WritePalette(void *ula,M6502Word a,uint8_t value);

    static void WriteNuLAControlRegister(void *ula,M6502Word a,uint8_t value);
    static void WriteNuLAPalette(void *ula,M6502Word a,uint8_t value);

    VideoULA();

    void Byte(uint8_t byte);

    void EmitPixels(union VideoDataHalfUnit *hu);
protected:
private:
    uint8_t m_palette[16]={};
    VideoDataBitmapPixel m_output_palette[16]={};
    uint8_t m_byte=0;
    uint8_t m_original_byte=0;
    uint8_t m_flash[16]={};
    uint8_t m_nula_palette_write_state=0;
    uint8_t m_nula_palette_write_buffer=0;
    uint8_t m_direct_palette=0;
    uint8_t m_disable_a1=0;
    NuLAAttributeMode m_attribute_mode={};

    void ResetNuLAState();
    VideoDataBitmapPixel GetPalette(uint8_t index);
    VideoDataBitmapPixel Shift();
    VideoDataBitmapPixel ShiftAttributeMode0();
    VideoDataBitmapPixel ShiftAttributeMode1();
    VideoDataBitmapPixel ShiftAttributeText();

    void Emit2MHz(VideoDataHalfUnit *hu);
    void Emit4MHz(VideoDataHalfUnit *hu);
    void Emit8MHz(VideoDataHalfUnit *hu);
    void Emit16MHz(VideoDataHalfUnit *hu);
    void EmitNuLAAttributeMode0(VideoDataHalfUnit *hu);
    void EmitNuLAAttributeMode1(VideoDataHalfUnit *hu);
    void EmitNuLAAttributeMode4(VideoDataHalfUnit *hu);
    void EmitNuLAAttributeTextMode4(VideoDataHalfUnit *hu);
    void EmitNuLAAttributeTextMode0(VideoDataHalfUnit *hu);
    void EmitNothing(VideoDataHalfUnit *hu);

    typedef void (VideoULA::*EmitMFn)(union VideoDataHalfUnit *);
    static const EmitMFn EMIT_MFNS[4][2][4];
    //static const EmitMFn NULA_EMIT_MFNS[2][4];
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CHECK_SIZEOF(VideoULA::Control,1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
