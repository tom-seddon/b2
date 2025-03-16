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

    union Control {
        uint8_t value;
        ControlBits bits;
    };

    // The NuLA/non-NuLA differences are handled by simply never mapping the
    // NuLA control registers in non-NuLA. This flag exists just so the debug
    // window can query it and hide the NuLA state.
    bool nula = false;

    // For reading only. Writes must be performed through WriteControlRegister.
    Control control = {};

    VideoDataPixel output_palette[16] = {};

    // Public because the Mode 7 handling works differently.
    uint8_t cursor_pattern = 0;

    static void WriteControlRegister(void *ula, M6502Word a, uint8_t value);
    static void WritePalette(void *ula, M6502Word a, uint8_t value);

    static void WriteNuLAControlRegister(void *ula, M6502Word a, uint8_t value);
    static void WriteNuLAPalette(void *ula, M6502Word a, uint8_t value);

    VideoULA();

    // Not a great name, but it's called from BBCMicro::InitStuff, soooo...
    //
    // Could do with rationalizing this a bit, maybe.
    void InitStuff();

    void DisplayEnabled();

    void Byte(uint8_t byte, uint8_t cudisp);

    void EmitPixels(VideoDataUnitPixels *pixels);
    void EmitBlank(VideoDataUnitPixels *pixels);

#if BBCMICRO_TRACE
    void SetTrace(Trace *t);
#endif
  protected:
  private:
    typedef void (VideoULA::*EmitMFn)(VideoDataUnitPixels *);

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
    uint8_t m_logical_mode = 0;
    uint8_t m_disable_a1 = 0;
    uint8_t m_scroll_offset = 0;
    uint8_t m_pixel_buffer_offset = 0;
    uint8_t m_blanking_size = 0;
    uint8_t m_blanking_counter = 0;
    uint8_t m_attribute_mode = 0;
    uint8_t m_text_attribute_mode = 0;
    PixelBuffer m_pixel_buffer = {};
#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
#endif
    EmitMFn m_emit_mfn = nullptr;

    void UpdatePixelBufferOffset();

    void ResetNuLAState();
    VideoDataPixel GetPalette(uint8_t index);
    template <bool LOGICAL, int BPP>
    VideoDataPixel ShiftNuLA();
    uint16_t ShiftULA();
    VideoDataPixel ShiftAttributeMode0();
    VideoDataPixel ShiftAttributeMode1();
    VideoDataPixel ShiftAttributeText();

    template <uint32_t FLAGS>
    void EmitNuLA(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeMode0(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeMode1(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeMode4(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeTextMode4(VideoDataUnitPixels *pixels);
    void EmitNuLAAttributeTextMode0(VideoDataUnitPixels *pixels);
    void EmitNothing(VideoDataUnitPixels *pixels);
    void EmitULA2MHz(VideoDataUnitPixels *pixels);
    void EmitULA4MHz(VideoDataUnitPixels *pixels);
    void EmitULA8MHz(VideoDataUnitPixels *pixels);
    void EmitULA16MHz(VideoDataUnitPixels *pixels);

    void UpdateEmitMFn();

    static const uint16_t ULA_PALETTE[2][16];
    static const EmitMFn ULA_EMIT_MFNS[4];
    static const EmitMFn NULA_EMIT_MFNS[];

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
