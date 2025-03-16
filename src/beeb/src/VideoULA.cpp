#include <shared/system.h>
#include <beeb/6502.h>
#include <string.h>
#include <beeb/VideoULA.h>
#include <beeb/video.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/Trace.h>

#include <shared/enum_decl.h>
#include "VideoULA_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "VideoULA_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(VU, "video", "VIDULA", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr uint16_t PIXEL_VALUE_CURSOR_XOR = 0x0fff;
static constexpr uint64_t PIXEL_VALUES_CURSOR_XOR = 0x0fff0fff0fff0fffull;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The cursor pattern is spread over the next 4 displayed columns.
//
// <pre>
// b7 b6 b5  Shape
// -- -- --  -----
//  0  0  0  <none>
//  0  0  1    __
//  0  1  0   _
//  0  1  1   ___
//  1  0  0  _
//  1  0  1  _ __
//  1  1  0  __
//  1  1  1  ____
// </pre>
//
// Bit 7 control the first column, bit 6 controls the second column, and bit 5
// controls the 3rd and 4th.
//
// 2 (identical) bits per column as this simplifies shifting the data out.

static constexpr uint8_t GetCursorPattern(uint8_t index) {
    uint8_t result = 0;

    if (index & 1) {
        result |= 3 << 4 | 3 << 6;
    }

    if (index & 2) {
        result |= 3 << 2;
    }

    if (index & 4) {
        result |= 3 << 0;
    }

    return result;
}

static constexpr uint8_t CURSOR_PATTERNS[8] = {
    GetCursorPattern(0),
    GetCursorPattern(1),
    GetCursorPattern(2),
    GetCursorPattern(3),
    GetCursorPattern(4),
    GetCursorPattern(5),
    GetCursorPattern(6),
    GetCursorPattern(7),
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WriteControlRegister(void *ula_, M6502Word a, uint8_t value) {
    auto ula = (VideoULA *)ula_;
    (void)a;

    if (value != ula->control.value) {
        ula->control.value = value;

        TRACEF(ula->m_trace, "ULA Control: Flash=%s Teletext=%s Line Width=%d Fast6845=%s Cursor=%d\n", BOOL_STR(ula->control.bits.flash), BOOL_STR(ula->control.bits.teletext), ula->control.bits.line_width, BOOL_STR(ula->control.bits.fast_6845), ula->control.bits.cursor);

        ula->UpdatePixelBufferOffset();

        ula->UpdateEmitMFn();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WritePalette(void *ula_, M6502Word a, uint8_t value) {
    auto ula = (VideoULA *)ula_;
    (void)a;

    uint8_t phy = (value & 0x0f) ^ 7;
    uint8_t log = value >> 4;

    ula->m_palette[log] = phy;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WriteNuLAControlRegister(void *ula_, M6502Word a, uint8_t value) {
    auto ula = (VideoULA *)ula_;

    if (ula->m_disable_a1) {
        WriteControlRegister(ula_, a, value);
    } else {
        uint8_t code = value >> 4, param = value & 0xf;

        switch (code) {
        case 1:
            // Toggle direct palette mode.
            ula->m_logical_mode = param & 1;
            TRACEF(ula->m_trace, "NuLA Control: Logical Mode=%s\n", BOOL_STR(ula->m_logical_mode));
            break;

        case 2:
            ula->m_scroll_offset = param & 7;
            ula->UpdatePixelBufferOffset();
            TRACEF(ula->m_trace, "NuLA Control: Scroll Offset=%u\n", ula->m_scroll_offset);
            break;

        case 3:
            ula->m_blanking_size = param;
            TRACEF(ula->m_trace, "NuLA Control: Blanking Size=%u\n", ula->m_blanking_size);
            break;

        case 4:
            // Reset NuLA state.
            ula->ResetNuLAState();
            TRACEF(ula->m_trace, "NuLA Control: Reset NuLA state\n");
            break;

        case 5:
            // Disable A1.
            ula->m_disable_a1 = 1;
            TRACEF(ula->m_trace, "NuLA Control: Disable A1\n");
            break;

        case 6:
            // Attribute modes on/off.
            ula->m_attribute_mode = param & 3;
            TRACEF(ula->m_trace, "NuLA Control: Attribute Mode=%d\n", ula->m_attribute_mode);
            ula->UpdateEmitMFn();
            break;

        case 7:
            // Text attribute modes on/off.
            ula->m_text_attribute_mode = param & 1;
            TRACEF(ula->m_trace, "NuLA Control: Text Attribute Mode=%s\n", BOOL_STR(ula->m_text_attribute_mode));
            ula->UpdateEmitMFn();
            break;

        case 8:
            // Set flashing flags for logical colours 8-11.
            ula->m_flash[8] = param & 0x08;
            ula->m_flash[9] = param & 0x04;
            ula->m_flash[10] = param & 0x02;
            ula->m_flash[11] = param & 0x01;
            TRACEF(ula->m_trace, "NuLA Control: Flash: 8=%s 9=%s 10=%s 11=%s\n", BOOL_STR(ula->m_flash[8]), BOOL_STR(ula->m_flash[9]), BOOL_STR(ula->m_flash[10]), BOOL_STR(ula->m_flash[11]));
            break;

        case 9:
            // Set flashing flags for logical colours 12-15.
            ula->m_flash[12] = param & 0x08;
            ula->m_flash[13] = param & 0x04;
            ula->m_flash[14] = param & 0x02;
            ula->m_flash[15] = param & 0x01;
            TRACEF(ula->m_trace, "NuLA Control: Flash: 12=%s 13=%s 14=%s 15=%s\n", BOOL_STR(ula->m_flash[12]), BOOL_STR(ula->m_flash[13]), BOOL_STR(ula->m_flash[14]), BOOL_STR(ula->m_flash[15]));
            break;

        default:
            // Ignore...
            TRACEF(ula->m_trace, "NuLA Control: code=%u, param=%u\n", code, param);
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WriteNuLAPalette(void *ula_, M6502Word a, uint8_t value) {
    auto ula = (VideoULA *)ula_;

    if (ula->m_disable_a1) {
        WritePalette(ula_, a, value);
    } else {
        if (ula->m_nula_palette_write_state) {
            uint8_t index = ula->m_nula_palette_write_buffer >> 4;
            VideoDataPixel *entry = &ula->output_palette[index];

            entry->bits.r = ula->m_nula_palette_write_buffer & 0xf;
            entry->bits.g = value >> 4;
            entry->bits.b = value & 0xf;
            entry->bits.x = 0;

            ula->m_flash[index] = 0;

            TRACEF(ula->m_trace,
                   "NuLA Palette: index=%u, rgb=0x%x%x%x\n",
                   index,
                   entry->bits.r,
                   entry->bits.g,
                   entry->bits.b);
        } else {
            ula->m_nula_palette_write_buffer = value;
        }

        ula->m_nula_palette_write_state = !ula->m_nula_palette_write_state;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoULA::VideoULA() {
    this->ResetNuLAState();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::InitStuff() {
    this->UpdateEmitMFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::DisplayEnabled() {
    // 1 fractional bit - it counts halves in slow clock mode.
    m_blanking_counter = m_blanking_size << 1;

    m_pixel_buffer.values[0] = 0;
    m_pixel_buffer.values[1] = 0;

    if (m_scroll_offset > 0) {
        VideoDataUnitPixels pixels;
        (this->*m_emit_mfn)(&pixels);
        if (!this->control.bits.fast_6845) {
            (this->*m_emit_mfn)(&pixels);
        }

        // The cursor address may have matched when the byte was being fetched,
        // but the cursor isn't included in any byte remnant.
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Byte(uint8_t byte, uint8_t cudisp) {
    m_work_byte = byte;
    m_original_byte = byte;

    if (cudisp) {
        this->cursor_pattern = CURSOR_PATTERNS[this->control.bits.cursor];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitPixels(VideoDataUnitPixels *pixels) {
    (this->*m_emit_mfn)(pixels);

    this->cursor_pattern >>= 1 + this->control.bits.fast_6845;

    if (m_blanking_counter > 0) {
        m_blanking_counter -= 1 + this->control.bits.fast_6845;

        pixels->values[1] = pixels->values[0] = 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitBlank(VideoDataUnitPixels *pixels) {
    this->EmitNothing(pixels);

    this->cursor_pattern >>= 1 + this->control.bits.fast_6845;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void VideoULA::SetTrace(Trace *t) {
    m_trace = t;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::UpdatePixelBufferOffset() {
    // The units for the scroll offset are different depending on
    // the 6845 clock rate.
    m_pixel_buffer_offset = m_scroll_offset << 1 >> this->control.bits.fast_6845;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::ResetNuLAState() {
    // Reset output palette.
    for (size_t i = 0; i < 16; ++i) {
        VideoDataPixel *pixel = &this->output_palette[i];

        pixel->bits.x = 0;
        pixel->bits.r = i & 1 ? 15 : 0;
        pixel->bits.g = i & 2 ? 15 : 0;
        pixel->bits.b = i & 4 ? 15 : 0;
    }

    // Reset flash flags.
    for (size_t i = 0; i < 8; ++i) {
        m_flash[8 + i] = 1;
    }

    // Reset scrolling.
    m_scroll_offset = 0;
    m_blanking_size = 0;

    // Use physical mode mapping by default.
    m_logical_mode = 0;

    // Reset palette write state.
    m_nula_palette_write_state = 0;

    // Reset attribute mode.
    m_attribute_mode = {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataPixel VideoULA::GetPalette(uint8_t index) {
    if (!m_logical_mode) {
        index = m_palette[index];

        if (m_flash[index]) {
            if (this->control.bits.flash) {
                index ^= 7;
            }
        }
    }

    return this->output_palette[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <bool LOGICAL, int BPP>
VideoDataPixel VideoULA::ShiftNuLA() {
    uint8_t index = m_work_byte;

    if constexpr (LOGICAL && BPP == 1) {
        index >>= 7;
    } else if constexpr (LOGICAL && BPP == 2) {
        index = index >> 6 & 2 | index >> 3 & 1;
    } else if constexpr (!LOGICAL || BPP == 4) {
        index = ((index >> 4) & 8) | ((index >> 3) & 4) | ((index >> 2) & 2) | ((index >> 1) & 1);
    } else {
        unhandled_ShiftNuLA_case;
    }

    m_work_byte <<= 1;
    m_work_byte |= 1;

    if constexpr (!LOGICAL) {
        index = m_palette[index];
    }

    if (m_flash[index]) {
        if (this->control.bits.flash) {
            index ^= 7;
        }
    }

    return this->output_palette[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint16_t VideoULA::ShiftULA() {
    uint8_t index = m_work_byte;
    index = ((index >> 1) & 1) | ((index >> 2) & 2) | ((index >> 3) & 4) | ((index >> 4) & 8);

    m_work_byte <<= 1;
    m_work_byte |= 1;

    return ULA_PALETTE[this->control.bits.flash][index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataPixel VideoULA::ShiftAttributeMode0() {
    uint8_t attribute = m_original_byte & 0x03;

    uint8_t index = m_work_byte >> 7 | attribute << 2;

    m_work_byte <<= 1;
    m_work_byte |= 1;

    return this->GetPalette(index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataPixel VideoULA::ShiftAttributeMode1() {
    uint8_t index = (m_original_byte >> 1 & 8) | (m_original_byte << 2 & 4) | (m_work_byte >> 6 & 2) | (m_work_byte >> 3 & 1);

    m_work_byte <<= 1;
    m_work_byte |= 1;

    return this->GetPalette(index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataPixel VideoULA::ShiftAttributeText() {
    uint8_t index = (m_work_byte >> 7 | m_original_byte << 1) & 0xf;

    m_work_byte <<= 1;
    m_work_byte &= 0xf0;

    return this->GetPalette(index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The ordinary video ULA only has 1 mode and it's a question of arranging the
// palette and the input bytes to make something sensible of it.
//
// In logical palette mode the NuLA tidies this up a bit by seemingly hardcoding
// the effective results and only reading the relevant bits. So in mode 0 (2 MHz
// CRTC clock, 16 MHz shift rate), only bit 7 of the shifted byte is used - and
// so on. But the resulting combinations aren't quite regular, as the bits per
// pixel is now a function of the CRTC clock rate too.
//
// TODO though I'm probably just not quite modelling this right...
template <uint32_t FLAGS> //bool LOGICAL, int MHz, bool FAST>
void VideoULA::EmitNuLA(VideoDataUnitPixels *pixels) {
    VideoDataPixel *dest = &m_pixel_buffer.pixels[m_pixel_buffer_offset];

    static constexpr uint32_t MODE = FLAGS >> VideoNuLAModeFlag_ModeShift & VideoNuLAModeFlag_ModeMask;
    static constexpr bool FAST = !!(FLAGS & VideoNuLAModeFlag_Fast6845);
    static constexpr bool LOGICAL = !!(FLAGS & VideoNuLAModeFlag_LogicalPalette);

    if constexpr (MODE == 0) {
        // 80 px
        dest[7] = dest[6] = dest[5] = dest[4] = dest[3] = dest[2] = dest[1] = dest[0] = this->ShiftNuLA<LOGICAL, 4>();
    } else if constexpr (MODE == 1) {
        // 160 px
        if constexpr (FAST) {
            dest[3] = dest[2] = dest[1] = dest[0] = this->ShiftNuLA<LOGICAL, 4>();
            dest[7] = dest[6] = dest[5] = dest[4] = this->ShiftNuLA<LOGICAL, 4>();
        } else {
            dest[3] = dest[2] = dest[1] = dest[0] = this->ShiftNuLA<LOGICAL, 2>();
            dest[7] = dest[6] = dest[5] = dest[4] = this->ShiftNuLA<LOGICAL, 2>();
        }
    } else if constexpr (MODE == 2) {
        // 320 px
        if constexpr (FAST) {
            dest[1] = dest[0] = this->ShiftNuLA<LOGICAL, 2>();
            dest[3] = dest[2] = this->ShiftNuLA<LOGICAL, 2>();
            dest[5] = dest[4] = this->ShiftNuLA<LOGICAL, 2>();
            dest[7] = dest[6] = this->ShiftNuLA<LOGICAL, 2>();
        } else {
            dest[1] = dest[0] = this->ShiftNuLA<LOGICAL, 1>();
            dest[3] = dest[2] = this->ShiftNuLA<LOGICAL, 1>();
            dest[5] = dest[4] = this->ShiftNuLA<LOGICAL, 1>();
            dest[7] = dest[6] = this->ShiftNuLA<LOGICAL, 1>();
        }
    } else if constexpr (MODE == 3) {
        // 640 px
        if constexpr (FAST) {
            dest[0] = this->ShiftNuLA<LOGICAL, 1>();
            dest[1] = this->ShiftNuLA<LOGICAL, 1>();
            dest[2] = this->ShiftNuLA<LOGICAL, 1>();
            dest[3] = this->ShiftNuLA<LOGICAL, 1>();
            dest[4] = this->ShiftNuLA<LOGICAL, 1>();
            dest[5] = this->ShiftNuLA<LOGICAL, 1>();
            dest[6] = this->ShiftNuLA<LOGICAL, 1>();
            dest[7] = this->ShiftNuLA<LOGICAL, 1>();
        } else {
            // actually still 320 px output in this mode, it seems?

            m_work_byte <<= 1;
            m_work_byte |= 1;

            dest[1] = dest[0] = this->ShiftNuLA<LOGICAL, 1>();

            m_work_byte <<= 1;
            m_work_byte |= 1;

            dest[3] = dest[2] = this->ShiftNuLA<LOGICAL, 1>();

            m_work_byte <<= 1;
            m_work_byte |= 1;

            dest[5] = dest[4] = this->ShiftNuLA<LOGICAL, 1>();

            m_work_byte <<= 1;
            m_work_byte |= 1;

            dest[7] = dest[6] = this->ShiftNuLA<LOGICAL, 1>();
        }
    } else {
        unhandled_EmitNuLA_case;
    }

    if (this->cursor_pattern & 1) {
        dest[0].all ^= PIXEL_VALUE_CURSOR_XOR;
        dest[1].all ^= PIXEL_VALUE_CURSOR_XOR;
        dest[2].all ^= PIXEL_VALUE_CURSOR_XOR;
        dest[3].all ^= PIXEL_VALUE_CURSOR_XOR;
        dest[4].all ^= PIXEL_VALUE_CURSOR_XOR;
        dest[5].all ^= PIXEL_VALUE_CURSOR_XOR;
        dest[6].all ^= PIXEL_VALUE_CURSOR_XOR;
        dest[7].all ^= PIXEL_VALUE_CURSOR_XOR;
    }

    pixels->values[0] = m_pixel_buffer.values[0];
    pixels->values[1] = m_pixel_buffer.values[1];

    m_pixel_buffer.values[0] = m_pixel_buffer.values[2];
    m_pixel_buffer.values[1] = m_pixel_buffer.values[3];
    m_pixel_buffer.values[2] = m_pixel_buffer.values[4];
    m_pixel_buffer.values[3] = m_pixel_buffer.values[5];
    m_pixel_buffer.values[4] = m_pixel_buffer.values[6];
    m_pixel_buffer.values[5] = m_pixel_buffer.values[7];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeMode0(VideoDataUnitPixels *pixels) {
    VideoDataPixel pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[0] = pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[1] = pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[2] = pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[3] = pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[4] = pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[5] = pixel;

    pixels->pixels[0].bits.x = VideoDataType_Bitmap12MHz;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeMode1(VideoDataUnitPixels *pixels) {
    VideoDataPixel pixel;

    pixel = this->ShiftAttributeMode1();
    pixels->pixels[0] = pixel;
    pixels->pixels[1] = pixel;

    pixel = this->ShiftAttributeMode1();
    pixels->pixels[2] = pixel;
    pixels->pixels[3] = pixel;

    pixel = this->ShiftAttributeMode1();
    pixels->pixels[4] = pixel;
    pixels->pixels[5] = pixel;

    pixels->pixels[0].bits.x = VideoDataType_Bitmap12MHz;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeMode4(VideoDataUnitPixels *pixels) {
    VideoDataPixel pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[0] = pixel;
    pixels->pixels[1] = pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[2] = pixel;
    pixels->pixels[3] = pixel;

    pixel = this->ShiftAttributeMode0();
    pixels->pixels[4] = pixel;
    pixels->pixels[5] = pixel;

    pixels->pixels[0].bits.x = VideoDataType_Bitmap12MHz;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeTextMode4(VideoDataUnitPixels *pixels) {
    VideoDataPixel pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[0] = pixel;
    pixels->pixels[1] = pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[2] = pixel;
    pixels->pixels[3] = pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[4] = pixel;
    pixels->pixels[5] = pixel;

    pixels->pixels[0].bits.x = VideoDataType_Bitmap12MHz;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeTextMode0(VideoDataUnitPixels *pixels) {
    VideoDataPixel pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[0] = pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[1] = pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[2] = pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[3] = pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[4] = pixel;

    pixel = this->ShiftAttributeText();
    pixels->pixels[5] = pixel;

    pixels->pixels[0].bits.x = VideoDataType_Bitmap12MHz;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNothing(VideoDataUnitPixels *pixels) {
    pixels->values[1] = pixels->values[0] = 0;

    if (this->cursor_pattern & 1) {
        pixels->values[0] ^= PIXEL_VALUES_CURSOR_XOR;
        pixels->values[1] ^= PIXEL_VALUES_CURSOR_XOR;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitULA2MHz(VideoDataUnitPixels *pixels) {
    pixels->pixels[7].all = pixels->pixels[6].all = pixels->pixels[5].all = pixels->pixels[4].all = pixels->pixels[3].all = pixels->pixels[2].all = pixels->pixels[1].all = pixels->pixels[0].all = this->ShiftULA();

    if (this->cursor_pattern & 1) {
        pixels->values[0] ^= PIXEL_VALUES_CURSOR_XOR;
        pixels->values[1] ^= PIXEL_VALUES_CURSOR_XOR;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitULA4MHz(VideoDataUnitPixels *pixels) {
    pixels->pixels[3].all = pixels->pixels[2].all = pixels->pixels[1].all = pixels->pixels[0].all = this->ShiftULA();
    pixels->pixels[7].all = pixels->pixels[5].all = pixels->pixels[6].all = pixels->pixels[4].all = this->ShiftULA();

    if (this->cursor_pattern & 1) {
        pixels->values[0] ^= PIXEL_VALUES_CURSOR_XOR;
        pixels->values[1] ^= PIXEL_VALUES_CURSOR_XOR;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitULA8MHz(VideoDataUnitPixels *pixels) {
    pixels->pixels[1].all = pixels->pixels[0].all = this->ShiftULA();
    pixels->pixels[3].all = pixels->pixels[2].all = this->ShiftULA();
    pixels->pixels[5].all = pixels->pixels[4].all = this->ShiftULA();
    pixels->pixels[7].all = pixels->pixels[6].all = this->ShiftULA();

    if (this->cursor_pattern & 1) {
        pixels->values[0] ^= PIXEL_VALUES_CURSOR_XOR;
        pixels->values[1] ^= PIXEL_VALUES_CURSOR_XOR;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitULA16MHz(VideoDataUnitPixels *pixels) {
    pixels->pixels[0].all = this->ShiftULA();
    pixels->pixels[1].all = this->ShiftULA();
    pixels->pixels[2].all = this->ShiftULA();
    pixels->pixels[3].all = this->ShiftULA();
    pixels->pixels[4].all = this->ShiftULA();
    pixels->pixels[5].all = this->ShiftULA();
    pixels->pixels[6].all = this->ShiftULA();
    pixels->pixels[7].all = this->ShiftULA();

    if (this->cursor_pattern & 1) {
        pixels->values[0] ^= PIXEL_VALUES_CURSOR_XOR;
        pixels->values[1] ^= PIXEL_VALUES_CURSOR_XOR;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::UpdateEmitMFn() {
    if (this->nula) {
        uint32_t index = 0;
        index |= this->control.bits.line_width << VideoNuLAModeFlag_ModeShift;
        index |= this->control.bits.fast_6845 << VideoNuLAModeFlag_Fast6845Shift;
        index |= m_attribute_mode << VideoNuLAModeFlag_AttributeModeShift;
        index |= m_text_attribute_mode << VideoNuLAModeFlag_TextAttributeModeShift;
        index |= m_logical_mode << VideoNuLAModeFlag_LogicalPaletteShift;
        m_emit_mfn = NULA_EMIT_MFNS[index];
    } else {
        m_emit_mfn = ULA_EMIT_MFNS[this->control.bits.line_width];
    }

    ASSERT(m_emit_mfn);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint16_t VideoULA::ULA_PALETTE[2][16] = {
    {
        0x0000,
        0x0f00,
        0x00f0,
        0x0ff0,
        0x000f,
        0x0f0f,
        0x00ff,
        0x0fff,
        0x0000,
        0x0f00,
        0x00f0,
        0x0ff0,
        0x000f,
        0x0f0f,
        0x00ff,
        0x0fff,
    },
    {
        0x0000,
        0x0f00,
        0x00f0,
        0x0ff0,
        0x000f,
        0x0f0f,
        0x00ff,
        0x0fff,
        0x0fff,
        0x00ff,
        0x0f0f,
        0x000f,
        0x0ff0,
        0x00f0,
        0x0f00,
        0x0000,
    },
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoULA::EmitMFn VideoULA::ULA_EMIT_MFNS[4] = {
    &VideoULA::EmitULA2MHz,
    &VideoULA::EmitULA4MHz,
    &VideoULA::EmitULA8MHz,
    &VideoULA::EmitULA16MHz,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define EMIT1(N) &VideoULA::EmitNuLA<N>,
#define EMIT4(BASE) \
    EMIT1(BASE + 0) \
    EMIT1(BASE + 1) \
    EMIT1(BASE + 2) \
    EMIT1(BASE + 3)
#define EMIT16(BASE) \
    EMIT4(BASE + 0)  \
    EMIT4(BASE + 4)  \
    EMIT4(BASE + 8)  \
    EMIT4(BASE + 12)

const VideoULA::EmitMFn VideoULA::NULA_EMIT_MFNS[128] = {EMIT16(0) EMIT16(16) EMIT16(32) EMIT16(48) EMIT16(64) EMIT16(80) EMIT16(96) EMIT16(112)};
