#ifndef HEADER_2B1F0A986AD74CCF8938EB7F804A61E2
#define HEADER_2B1F0A986AD74CCF8938EB7F804A61E2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include "video.h"

union VideoDataUnitPixels;
union M6502Word;
class Trace;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The VideoULA class actually emulates a Video NuLA... but provided
// the WriteNuLAXXX mmio functions aren't mapped in, it's impossible
// to tell the difference.

class VideoULA {
  public:
#include <shared/pushwarn_bitfields.h>
    struct ControlBits {
        uint8_t flash : 1;
        uint8_t teletext : 1;
        uint8_t line_width : 2;
        uint8_t fast_6845 : 1;
        uint8_t cursor : 3;
    };
#include <shared/popwarn.h>

    struct NuLAAttributeModeBits {
        uint8_t enabled : 1;
        uint8_t text : 1;
    };

    union NuLAAttributeMode {
        uint8_t value;
        NuLAAttributeModeBits bits;
    };

    union Control {
        uint8_t value;
        ControlBits bits;
    };

    // The NuLA/non-NuLA differences are handled by simply never mapping the
    // NuLA control registers in non-NuLA. This flag exists just so the debug
    // window can query it and hide the NuLA state.
    //
    // (Eventually, maybe the slightly different NuLA cursor handling should
    // be handled.)
    bool nula = false;

    Control control = {};

    VideoDataPixel output_palette[16] = {};

    static void WriteControlRegister(void *ula, M6502Word a, uint8_t value);
    static void WritePalette(void *ula, M6502Word a, uint8_t value);

    static void WriteNuLAControlRegister(void *ula, M6502Word a, uint8_t value);
    static void WriteNuLAPalette(void *ula, M6502Word a, uint8_t value);

    VideoULA();

    void DisplayEnabled();

    void Byte(uint8_t byte);

    void EmitPixels(VideoDataUnitPixels *pixels);

#if BBCMICRO_TRACE
    void SetTrace(Trace *t);
#endif
  protected:
  private:
    union PixelBuffer {
        uint64_t values[8];
        VideoDataPixel pixels[32];
    };

    uint8_t m_palette[16] = {};
    uint8_t m_work_byte = 0;
    uint8_t m_original_byte = 0;
    uint8_t m_flash[16] = {};
    uint8_t m_nula_palette_write_state = 0;
    uint8_t m_nula_palette_write_buffer = 0;
    uint8_t m_direct_palette = 0;
    uint8_t m_disable_a1 = 0;
    uint8_t m_scroll_offset = 0;
    uint8_t m_pixel_buffer_offset = 0;
    uint8_t m_blanking_size = 0;
    uint8_t m_blanking_counter = 0;
    NuLAAttributeMode m_attribute_mode = {};
    PixelBuffer m_pixel_buffer = {};
#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
#endif

    void UpdatePixelBufferOffset();

    void ResetNuLAState();
    VideoDataPixel GetPalette(uint8_t index);
    VideoDataPixel Shift();
    VideoDataPixel ShiftAttributeMode0();
    VideoDataPixel ShiftAttributeMode1();
    VideoDataPixel ShiftAttributeText();

    void Emit2MHz(VideoDataUnitPixels *pixels);
    void Emit4MHz(VideoDataUnitPixels *pixels);
    void Emit8MHz(VideoDataUnitPixels *pixels);
    void Emit16MHz(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeMode0(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeMode1(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeMode4(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeTextMode4(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeTextMode0(VideoDataUnitPixels *pixels);
    void EmitNothing(VideoDataUnitPixels *pixels);

    typedef void (VideoULA::*EmitMFn)(VideoDataUnitPixels *);
    static const EmitMFn EMIT_MFNS[4][2][4];
    //static const EmitMFn NULA_EMIT_MFNS[2][4];

#if BBCMICRO_DEBUGGER
    friend class VideoULADebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CHECK_SIZEOF(VideoULA::Control, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
