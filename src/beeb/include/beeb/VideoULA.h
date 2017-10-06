#ifndef HEADER_2B1F0A986AD74CCF8938EB7F804A61E2
#define HEADER_2B1F0A986AD74CCF8938EB7F804A61E2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

union VideoDataHalfUnit;
union M6502Word;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

    union Control {
        ControlBits bits;
        uint8_t value;
    };

    Control control={};

    typedef void (VideoULA::*EmitMFn)(union VideoDataHalfUnit *);
    static const EmitMFn EMIT_MFNS[4];

    static void WriteControlRegister(void *ula,M6502Word a,uint8_t value);
    static void WritePalette(void *ula,M6502Word a,uint8_t value);

    void Byte(uint8_t byte);

protected:
private:
    uint8_t m_palette[16]={};
    uint8_t m_byte=0;

    uint16_t Shift();
    void Emit2MHz(VideoDataHalfUnit *hu);
    void Emit4MHz(VideoDataHalfUnit *hu);
    void Emit8MHz(VideoDataHalfUnit *hu);
    void Emit16MHz(VideoDataHalfUnit *hu);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CHECK_SIZEOF(VideoULA::Control,1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
