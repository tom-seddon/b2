#include <shared/system.h>
#include <beeb/conf.h>
#include <beeb/TVOutput.h>
#include <beeb/OutputData.h>
#include <shared/debug.h>
#include <string.h>
#include <stdlib.h>
#include <beeb/video.h>
#include <shared/log.h>
#include <math.h>

#include <shared/enum_def.h>
#include <beeb/TVOutput.inl>
#include <shared/enum_end.h>

//LOG_EXTERN(OUTPUT);

#if !CPU_LITTLE_ENDIAN
#error TVOutput will need some fixing up for non-little-endian systems
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TVOutput::TVOutput() {
    // +1 to accommodate writing an extra row when emulating interlace. (This
    // extra row is ignored.)
    m_texture_pixels.resize(TV_TEXTURE_WIDTH * (TV_TEXTURE_HEIGHT + 1));
#if VIDEO_TRACK_METADATA
    m_texture_units.resize(m_texture_pixels.size());
#endif
    m_last_vsync_texture_pixels.resize(TV_TEXTURE_WIDTH * (TV_TEXTURE_HEIGHT + 1));

    MUTEX_SET_NAME(m_last_vsync_texture_pixels_mutex, "Last vsync texture pixels");

    this->InitPalette();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TVOutput::~TVOutput() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::PrepareForUpdate() {
#if BBCMICRO_DEBUGGER
    if (m_texture_dirty) {
        std::fill(m_texture_pixels.begin(), m_texture_pixels.end(), this->GetTexelValue(0, 0, 0));
        m_texture_dirty = false;
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t DIGITS[10][13] = {
    {0x00, 0x00, 0x04, 0x0A, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04, 0x00, 0x00}, // 48 (0x30) '0'
    {0x00, 0x00, 0x04, 0x06, 0x05, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x00, 0x00}, // 49 (0x31) '1'
    {0x00, 0x00, 0x0E, 0x11, 0x11, 0x10, 0x08, 0x04, 0x02, 0x01, 0x1F, 0x00, 0x00}, // 50 (0x32) '2'
    {0x00, 0x00, 0x1F, 0x10, 0x08, 0x04, 0x0E, 0x10, 0x10, 0x11, 0x0E, 0x00, 0x00}, // 51 (0x33) '3'
    {0x00, 0x00, 0x08, 0x08, 0x0C, 0x0A, 0x0A, 0x09, 0x1F, 0x08, 0x08, 0x00, 0x00}, // 52 (0x34) '4'
    {0x00, 0x00, 0x1F, 0x01, 0x01, 0x0D, 0x13, 0x10, 0x10, 0x11, 0x0E, 0x00, 0x00}, // 53 (0x35) '5'
    {0x00, 0x00, 0x0E, 0x11, 0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, // 54 (0x36) '6'
    {0x00, 0x00, 0x1F, 0x10, 0x08, 0x08, 0x04, 0x04, 0x02, 0x02, 0x02, 0x00, 0x00}, // 55 (0x37) '7'
    {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, // 56 (0x38) '8'
    {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x11, 0x0E, 0x00, 0x00}, // 57 (0x39) '9'
};

// (272 scanned lines + 40 retrace lines + 0.5 interlace lines) * (52+4+8=64)us = 20000us = 20ms

static const int HORIZONTAL_RETRACE_CYCLES = 2 * 4;
static const int BACK_PORCH_CYCLES = 2 * 8;
static const int SCAN_OUT_CYCLES = 2 * 52;
static const int SCANLINE_CYCLES = HORIZONTAL_RETRACE_CYCLES + BACK_PORCH_CYCLES + SCAN_OUT_CYCLES;
static_assert(SCANLINE_CYCLES == 128, "one scanline must be 64us");
static const int VERTICAL_RETRACE_SCANLINES = 12;

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
static const int MAX_NUM_SCANNED_LINES = 500;

#define NOTHING_PALETTE_INDEX (0)

#if BUILD_TYPE_Debug
#ifdef _MSC_VER
#pragma optimize("tsg", on)
#endif
#endif

void TVOutput::Update(const VideoDataUnit *units, size_t num_units) {
    const VideoDataUnit *unit = units;

    for (size_t i = 0; i < num_units; ++i, ++unit) {
        switch (m_state) {
        default:
            ASSERT(0);
            break;

        case TVOutputState_VerticalRetrace:
            {
                LockGuard<Mutex> lock(m_last_vsync_texture_pixels_mutex);

                ASSERT(m_last_vsync_texture_pixels.size() == m_texture_pixels.size());
                memcpy(m_last_vsync_texture_pixels.data(),
                       m_texture_pixels.data(),
                       m_texture_pixels.size() * sizeof m_texture_pixels[0]);
            }

            // With interlaced output, odd fields start 1 scanline lower.
            //
            // If interlace flag off: draw odd fields at y=0, so they start
            // (visually speaking) at y=2. Draw even fields at y=0. No
            // apparent interlace.
            //
            // If interlace flag on: draw odd fields at y=0, so they appear to
            // start at y=2. Draw even fields at y=1. Even fields appear one
            // half scanline earlier than odd fields.
            if (m_x >= TV_TEXTURE_WIDTH / 2) {
                // Odd field.
                m_y = 0;
            } else {
                // Even field.
                if (m_interlace) {
                    m_y = 1;
                } else {
                    m_y = 2;
                }
            }

            ++m_num_fields;
            m_state = TVOutputState_VerticalRetraceWait;
            ++m_texture_data_version;
            m_x = 0;
            m_pixels_line = m_texture_pixels.data() + m_y * TV_TEXTURE_WIDTH;
#if VIDEO_TRACK_METADATA
            m_units_line = m_texture_units.data() + m_y * TV_TEXTURE_WIDTH;
#endif
            m_state_timer = 1;
            break;

        case TVOutputState_VerticalRetraceWait:
            {
                // Ignore everything.
                if (m_state_timer++ >= VERTICAL_RETRACE_SCANLINES * SCANLINE_CYCLES) {
                    m_state_timer = 0;
                    m_state = TVOutputState_Scanout;
                }
            }
            break;

        case TVOutputState_Scanout:
            {
                if (unit->pixels.pixels[1].bits.x & VideoDataUnitFlag_VSync) {
                    m_state = TVOutputState_VerticalRetrace;
                    break;
                }

                if (unit->pixels.pixels[1].bits.x & VideoDataUnitFlag_HSync) {
                    m_state = TVOutputState_HorizontalRetrace;
                    break;
                }

                uint32_t *pixels0;
                uint32_t *pixels1;

                switch (unit->pixels.pixels[0].bits.x) {
                default:
                    {
                        ASSERT(false);
                    }
                    break;

                case VideoDataType_Bitmap16MHz:
                    {
                        if (m_x < TV_TEXTURE_WIDTH && m_y < TV_TEXTURE_HEIGHT) {
                            pixels0 = m_pixels_line + m_x;
                            pixels1 = pixels0 + TV_TEXTURE_WIDTH;

#define EXPAND_16MHZ(I)                                 \
    const VideoDataPixel p##I = unit->pixels.pixels[I]; \
    pixels1[I] = pixels0[I] = (uint32_t)p##I.bits.b << 0u | (uint32_t)p##I.bits.b << 4u | (uint32_t)p##I.bits.g << 8u | (uint32_t)p##I.bits.g << 12u | (uint32_t)p##I.bits.r << 16u | (uint32_t)p##I.bits.r << 20u

                            EXPAND_16MHZ(0);
                            EXPAND_16MHZ(1);
                            EXPAND_16MHZ(2);
                            EXPAND_16MHZ(3);
                            EXPAND_16MHZ(4);
                            EXPAND_16MHZ(5);
                            EXPAND_16MHZ(6);
                            EXPAND_16MHZ(7);

#if VIDEO_TRACK_METADATA
                            VideoDataUnit *units0 = m_units_line + m_x;
                            units0[7] = units0[6] = units0[5] = units0[4] = units0[3] = units0[2] = units0[1] = units0[0] = *unit;

                            VideoDataUnit *units1 = units0 + TV_TEXTURE_WIDTH;
                            units1[7] = units1[6] = units1[5] = units1[4] = units1[3] = units1[2] = units1[1] = units1[0] = *unit;
#endif
                        }
                    }
                    break;

                case VideoDataType_Teletext:
                    {
                        if (m_x < TV_TEXTURE_WIDTH && m_y < TV_TEXTURE_HEIGHT) {
                            pixels0 = m_pixels_line + m_x;
                            pixels1 = pixels0 + TV_TEXTURE_WIDTH;

                            uint16_t p_0 = unit->pixels.pixels[2].all;
                            uint16_t p_1 = unit->pixels.pixels[3].all;

                            const VideoDataPixel p00 = unit->pixels.pixels[p_0 & 1];
                            const VideoDataPixel p01 = unit->pixels.pixels[p_1 & 1];

                            const VideoDataPixel p10 = unit->pixels.pixels[p_0 >> 1 & 1];
                            const VideoDataPixel p11 = unit->pixels.pixels[p_1 >> 1 & 1];

                            const VideoDataPixel p20 = unit->pixels.pixels[p_0 >> 2 & 1];
                            const VideoDataPixel p21 = unit->pixels.pixels[p_1 >> 2 & 1];

                            const VideoDataPixel p30 = unit->pixels.pixels[p_0 >> 3 & 1];
                            const VideoDataPixel p31 = unit->pixels.pixels[p_1 >> 3 & 1];

                            const VideoDataPixel p40 = unit->pixels.pixels[p_0 >> 4 & 1];
                            const VideoDataPixel p41 = unit->pixels.pixels[p_1 >> 4 & 1];

                            const VideoDataPixel p50 = unit->pixels.pixels[p_0 >> 5 & 1];
                            const VideoDataPixel p51 = unit->pixels.pixels[p_1 >> 5 & 1];

#define EXPAND_12MHZ_VDP(VAR) ((uint32_t)(VAR).bits.b << 0u | (uint32_t)(VAR).bits.b << 4u | (uint32_t)(VAR).bits.g << 8u | (uint32_t)(VAR).bits.g << 12u | (uint32_t)(VAR).bits.r << 16u | (uint32_t)(VAR).bits.r << 20u)
#define EXPAND_12MHZ_VARS(SUFFIX) ((uint32_t)(b##SUFFIX) << 0u | (uint32_t)(g##SUFFIX) << 8u | (uint32_t)(r##SUFFIX) << 16u)

                            // 6 pixels:
                            // <pre>
                            // 0 1 2 3 4 5
                            // </pre>
                            //
                            // Expand into 8. Scale up 4x, producing 24 pixels:
                            //
                            // <pre>
                            // 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 4 4 4 4 5 5 5 5
                            // </pre>
                            //
                            // Scale to 2/3 size, producing 8: (each pixel here
                            // is the gamma-corrected average of all
                            // contributing pixels)
                            //
                            // <pre>
                            // 000 011 112 222 333 344 445 555
                            // </pre>

                            uint8_t r011_0 = m_blend[p00.bits.r][p10.bits.r];
                            uint8_t g011_0 = m_blend[p00.bits.g][p10.bits.g];
                            uint8_t b011_0 = m_blend[p00.bits.b][p10.bits.b];
                            uint8_t r011_1 = m_blend[p01.bits.r][p11.bits.r];
                            uint8_t g011_1 = m_blend[p01.bits.g][p11.bits.g];
                            uint8_t b011_1 = m_blend[p01.bits.b][p11.bits.b];

                            // 112
                            uint8_t r112_0 = m_blend[p20.bits.r][p10.bits.r];
                            uint8_t g112_0 = m_blend[p20.bits.g][p10.bits.g];
                            uint8_t b112_0 = m_blend[p20.bits.b][p10.bits.b];
                            uint8_t r112_1 = m_blend[p21.bits.r][p11.bits.r];
                            uint8_t g112_1 = m_blend[p21.bits.g][p11.bits.g];
                            uint8_t b112_1 = m_blend[p21.bits.b][p11.bits.b];

                            // 344
                            uint8_t r344_0 = m_blend[p30.bits.r][p40.bits.r];
                            uint8_t g344_0 = m_blend[p30.bits.g][p40.bits.g];
                            uint8_t b344_0 = m_blend[p30.bits.b][p40.bits.b];
                            uint8_t r344_1 = m_blend[p31.bits.r][p41.bits.r];
                            uint8_t g344_1 = m_blend[p31.bits.g][p41.bits.g];
                            uint8_t b344_1 = m_blend[p31.bits.b][p41.bits.b];

                            // 445
                            uint8_t r445_0 = m_blend[p50.bits.r][p40.bits.r];
                            uint8_t g445_0 = m_blend[p50.bits.g][p40.bits.g];
                            uint8_t b445_0 = m_blend[p50.bits.b][p40.bits.b];
                            uint8_t r445_1 = m_blend[p51.bits.r][p41.bits.r];
                            uint8_t g445_1 = m_blend[p51.bits.g][p41.bits.g];
                            uint8_t b445_1 = m_blend[p51.bits.b][p41.bits.b];

                            pixels0[0] = EXPAND_12MHZ_VDP(p00);    //000
                            pixels0[1] = EXPAND_12MHZ_VARS(011_0); //011
                            pixels0[2] = EXPAND_12MHZ_VARS(112_0); //112
                            pixels0[3] = EXPAND_12MHZ_VDP(p20);    //222
                            pixels0[4] = EXPAND_12MHZ_VDP(p30);    //333
                            pixels0[5] = EXPAND_12MHZ_VARS(344_0); //344
                            pixels0[6] = EXPAND_12MHZ_VARS(445_0); //445
                            pixels0[7] = EXPAND_12MHZ_VDP(p50);    //555

                            pixels1[0] = EXPAND_12MHZ_VDP(p01);
                            pixels1[1] = EXPAND_12MHZ_VARS(011_1);
                            pixels1[2] = EXPAND_12MHZ_VARS(112_1);
                            pixels1[3] = EXPAND_12MHZ_VDP(p21);
                            pixels1[4] = EXPAND_12MHZ_VDP(p31);
                            pixels1[5] = EXPAND_12MHZ_VARS(344_1);
                            pixels1[6] = EXPAND_12MHZ_VARS(445_1);
                            pixels1[7] = EXPAND_12MHZ_VDP(p51);

#if VIDEO_TRACK_METADATA
                            VideoDataUnit *units0 = m_units_line + m_x;
                            units0[7] = units0[6] = units0[5] = units0[4] = units0[3] = units0[2] = units0[1] = units0[0] = *unit;

                            VideoDataUnit *units1 = units0 + TV_TEXTURE_WIDTH;
                            units1[7] = units1[6] = units1[5] = units1[4] = units1[3] = units1[2] = units1[1] = units1[0] = *unit;
#endif
                        }
                    }
                    break;

                case VideoDataType_Bitmap12MHz:
                    {
                        if (m_x < TV_TEXTURE_WIDTH && m_y < TV_TEXTURE_HEIGHT) {
                            pixels0 = m_pixels_line + m_x;
                            pixels1 = pixels0 + TV_TEXTURE_WIDTH;

                            const VideoDataPixel p0 = unit->pixels.pixels[0];
                            const VideoDataPixel p1 = unit->pixels.pixels[1];
                            const VideoDataPixel p2 = unit->pixels.pixels[2];
                            const VideoDataPixel p3 = unit->pixels.pixels[3];
                            const VideoDataPixel p4 = unit->pixels.pixels[4];
                            const VideoDataPixel p5 = unit->pixels.pixels[5];

                            uint8_t r011 = m_blend[p0.bits.r][p1.bits.r];
                            uint8_t g011 = m_blend[p0.bits.g][p1.bits.g];
                            uint8_t b011 = m_blend[p0.bits.b][p1.bits.b];

                            uint8_t r112 = m_blend[p2.bits.r][p1.bits.r];
                            uint8_t g112 = m_blend[p2.bits.g][p1.bits.g];
                            uint8_t b112 = m_blend[p2.bits.b][p1.bits.b];

                            uint8_t r334 = m_blend[p3.bits.r][p4.bits.r];
                            uint8_t g334 = m_blend[p3.bits.g][p4.bits.g];
                            uint8_t b334 = m_blend[p3.bits.b][p4.bits.b];

                            uint8_t r445 = m_blend[p5.bits.r][p4.bits.r];
                            uint8_t g445 = m_blend[p5.bits.g][p4.bits.g];
                            uint8_t b445 = m_blend[p5.bits.b][p4.bits.b];

                            pixels1[0] = pixels0[0] = EXPAND_12MHZ_VDP(p0);
                            pixels1[1] = pixels0[1] = EXPAND_12MHZ_VARS(011);
                            pixels1[2] = pixels0[2] = EXPAND_12MHZ_VARS(112);
                            pixels1[3] = pixels0[3] = EXPAND_12MHZ_VDP(p2);
                            pixels1[4] = pixels0[4] = EXPAND_12MHZ_VDP(p3);
                            pixels1[5] = pixels0[5] = EXPAND_12MHZ_VARS(334);
                            pixels1[6] = pixels0[6] = EXPAND_12MHZ_VARS(445);
                            pixels1[7] = pixels0[7] = EXPAND_12MHZ_VDP(p5);

#if VIDEO_TRACK_METADATA
                            VideoDataUnit *units0 = m_units_line + m_x;
                            units0[7] = units0[6] = units0[5] = units0[4] = units0[3] = units0[2] = units0[1] = units0[0] = *unit;

                            VideoDataUnit *units1 = units0 + TV_TEXTURE_WIDTH;
                            units1[7] = units1[6] = units1[5] = units1[4] = units1[3] = units1[2] = units1[1] = units1[0] = *unit;
#endif
                        }
                    }
                    break;
                }

                m_x += 8;

                if (m_state_timer++ >= SCAN_OUT_CYCLES) {
                    m_state = TVOutputState_HorizontalRetrace;
                }
            }
            break;

        case TVOutputState_HorizontalRetrace:
            {
                m_x = 0;
                m_y += 2;

                if (m_y >= 2 * MAX_NUM_SCANNED_LINES) {
                    // VBlank time anyway.
                    m_state = TVOutputState_VerticalRetrace;
                    break;
                }

                m_pixels_line += TV_TEXTURE_WIDTH * 2;
#if VIDEO_TRACK_METADATA
                m_units_line += TV_TEXTURE_WIDTH * 2;
#endif
                m_state_timer = 2; //+1 for Scanout; +1 for this state
                m_state = TVOutputState_HorizontalRetraceWait;
            }
            break;

        case TVOutputState_HorizontalRetraceWait:
            {
                // Ignore input in this state.
                ++m_state_timer;
                if (m_state_timer >= HORIZONTAL_RETRACE_CYCLES) {
                    m_state_timer = 0;
                    m_state = TVOutputState_BackPorch;
                }
            }
            break;

        case TVOutputState_BackPorch:
            {
                // Ignore input in this state.
                ++m_state_timer;
                if (m_state_timer >= BACK_PORCH_CYCLES) {
                    m_state_timer = 0;
                    m_state = TVOutputState_Scanout;
                }
            }
            break;
        }
    }
}

#if BUILD_TYPE_Debug
#ifdef _MSC_VER
#pragma optimize("", on)
#endif
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

void TVOutput::FillWithTestPattern() {
    m_texture_pixels.clear();
    m_texture_dirty = true;

    uint32_t palette[8];
    for (size_t i = 0; i < 8; ++i) {
        palette[i] = (i & 1 ? 0xff0000 : 0x000000) | (i & 2 ? 0x00ff00 : 0x000000) | (i & 4 ? 0x0000ff : 0x000000);
    }

    for (int y = 0; y < TV_TEXTURE_HEIGHT + 1; ++y) {
        for (int x = 0; x < TV_TEXTURE_WIDTH; ++x) {
            if ((x ^ y) & 1) {
                m_texture_pixels.push_back(palette[1]);
            } else {
                m_texture_pixels.push_back(palette[3]);
            }
        }
    }

    uint32_t colours[] = {palette[7], palette[0], palette[6], palette[0]};

    for (size_t i = 0; i < sizeof colours / sizeof colours[0]; ++i) {
        uint32_t colour = colours[i];

        for (size_t x = i; x < TV_TEXTURE_WIDTH - i; ++x) {
            m_texture_pixels[i * TV_TEXTURE_WIDTH + x] = colour;
            m_texture_pixels[(TV_TEXTURE_HEIGHT - 1 - i) * TV_TEXTURE_WIDTH + x] = colour;
        }

        for (size_t y = i; y < TV_TEXTURE_HEIGHT - i; ++y) {
            m_texture_pixels[y * TV_TEXTURE_WIDTH + i] = colour;
            m_texture_pixels[y * TV_TEXTURE_WIDTH + TV_TEXTURE_WIDTH - 1 - i] = colour;
        }
    }

    {
        for (size_t cy = 0; cy < TV_TEXTURE_HEIGHT; cy += 50) {
            size_t tmp = cy;
            size_t cx = 100;
            do {
                const uint8_t *digit = DIGITS[tmp % 10];

                for (size_t gy = 0; gy < 13; ++gy) {
                    if (cy + gy >= TV_TEXTURE_HEIGHT) {
                        break;
                    }

                    uint32_t *line = &m_texture_pixels[cx + (cy + gy) * TV_TEXTURE_WIDTH];
                    uint8_t row = *digit++;

                    for (size_t gx = 0; gx < 6; ++gx) {
                        if (row & 1) {
                            *line = palette[0];
                        }

                        row >>= 1;
                        ++line;
                    }
                }

                cx -= 6;
                tmp /= 10;
            } while (tmp != 0);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t *TVOutput::GetTexturePixels(uint64_t *texture_data_version) const {
    if (texture_data_version) {
        *texture_data_version = m_texture_data_version;
    }

    return m_texture_pixels.data();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t *TVOutput::GetLastVSyncTexturePixels(UniqueLock<Mutex> *lock) const {
    *lock = UniqueLock<Mutex>(m_last_vsync_texture_pixels_mutex);

    return m_last_vsync_texture_pixels.data();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::CopyTexturePixels(void *dest_pixels, size_t dest_pitch_bytes) const {
    ASSERT(dest_pitch_bytes > 0);
    size_t src_pitch_bytes = TV_TEXTURE_WIDTH * 4;

    if (src_pitch_bytes == dest_pitch_bytes) {
        memcpy(dest_pixels, m_texture_pixels.data(), TV_TEXTURE_HEIGHT * TV_TEXTURE_WIDTH * 4);
    } else {
        auto dest = (char *)dest_pixels;
        auto src = (const char *)m_texture_pixels.data();

        for (size_t y = 0; y < TV_TEXTURE_HEIGHT; ++y) {
            memcpy(dest, src, src_pitch_bytes);
            dest += dest_pitch_bytes;
            src += src_pitch_bytes;
        }
    }

    if (this->show_usec_markers || this->show_half_usec_markers) {
        for (size_t x = 0; x < TV_TEXTURE_WIDTH; x += 8) {
            char *dest = (char *)((uint32_t *)dest_pixels + x);
            const char *src = (const char *)(m_texture_pixels.data() + x);

            if (this->show_usec_markers && (x & 15) == 0) {
                for (size_t y = 0; y < TV_TEXTURE_HEIGHT; ++y) {
                    *(uint32_t *)dest = *(const uint32_t *)src ^ m_usec_marker_xor;

                    dest += dest_pitch_bytes;
                    src += src_pitch_bytes;
                }
            } else if (this->show_half_usec_markers && (x & 7) == 0) {
                for (size_t y = 0; y < TV_TEXTURE_HEIGHT; ++y) {
                    *(uint32_t *)dest = *(const uint32_t *)src ^ m_half_usec_marker_xor;

                    dest += dest_pitch_bytes;
                    src += src_pitch_bytes;
                }
            }
        }
    }

#if VIDEO_TRACK_METADATA
    // It's best to do this stuff as a postprocess, so it can be toggled
    // while the emulation is paused.
    this->AddMetadataMarkers(dest_pixels,
                             dest_pitch_bytes,
                             this->show_6845_row_markers,
                             VideoDataUnitMetadataFlag_6845Raster0,
                             m_6845_raster0_marker_xor);

    this->AddMetadataMarkers(dest_pixels,
                             dest_pitch_bytes,
                             this->show_6845_dispen_markers,
                             VideoDataUnitMetadataFlag_6845DISPEN,
                             m_6845_dispen_marker_xor);
#endif

    if (this->show_usec_markers ||
        this->show_half_usec_markers ||
        this->show_6845_dispen_markers ||
        this->show_6845_row_markers) {
        uint32_t bg = 0;
        uint32_t fg = 0xffffffu;

        for (size_t x = 0; x < TV_TEXTURE_WIDTH; x += 16) {
            size_t column = x / 16;
            size_t tens = column / 10 % 10;
            size_t units = column % 10;
            char *dest_bytes = (char *)((uint32_t *)dest_pixels + x + 3);

            for (size_t y = 0; y < 13; ++y) {
                auto dest = (uint32_t *)dest_bytes;

                dest[0] = DIGITS[tens][y] & 0x01 ? fg : bg;
                dest[1] = DIGITS[tens][y] & 0x02 ? fg : bg;
                dest[2] = DIGITS[tens][y] & 0x04 ? fg : bg;
                dest[3] = DIGITS[tens][y] & 0x08 ? fg : bg;
                dest[4] = DIGITS[tens][y] & 0x10 ? fg : bg;
                dest[5] = DIGITS[tens][y] & 0x20 ? fg : bg;
                dest[6] = DIGITS[units][y] & 0x01 ? fg : bg;
                dest[7] = DIGITS[units][y] & 0x02 ? fg : bg;
                dest[8] = DIGITS[units][y] & 0x04 ? fg : bg;
                dest[9] = DIGITS[units][y] & 0x08 ? fg : bg;
                dest[10] = DIGITS[units][y] & 0x10 ? fg : bg;
                dest[11] = DIGITS[units][y] & 0x20 ? fg : bg;

                dest_bytes += dest_pitch_bytes;
            }
        }
    }

#if BBCMICRO_DEBUGGER
    if (this->show_beam_position) {
        if (m_x < TV_TEXTURE_WIDTH && m_y < TV_TEXTURE_HEIGHT) {
            // Sneak it in on top. Don't read from the
            // locked data.

            //if(m_pixel_format->format==SDL_PIXELFORMAT_ARGB8888) {
            //} else {
            auto dest = (uint32_t *)((char *)dest_pixels + m_y * (size_t)dest_pitch_bytes);
            auto src = (const uint32_t *)((const char *)m_texture_pixels.data() + m_y * src_pitch_bytes);

            for (size_t i = m_x; i < TV_TEXTURE_WIDTH; ++i) {
                dest[i] = src[i] ^ m_beam_marker_xor;
            }
        }
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if VIDEO_TRACK_METADATA
const VideoDataUnit *TVOutput::GetTextureUnits() const {
    return m_texture_units.data();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TVOutput::GetBeamPosition(size_t *x, size_t *y) const {
    if (m_x >= TV_TEXTURE_WIDTH || m_y >= TV_TEXTURE_HEIGHT) {
        return false;
    } else {
        *x = m_x;
        *y = m_y;

        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TVOutput::IsInVerticalBlank() const {
    return m_state == TVOutputState_VerticalRetrace || m_state == TVOutputState_VerticalRetraceWait;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double TVOutput::GetGamma() const {
    return m_gamma;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::SetGamma(double gamma) {
    m_gamma = gamma;

    this->InitPalette();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TVOutput::GetInterlace() const {
    return m_interlace;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TVOutput::SetInterlace(bool interlace) {
    m_interlace = interlace;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t GetByte(double x) {
    if (x < 0.) {
        return 0;
    } else if (x >= 1.) {
        return 255;
    } else {
        return (uint8_t)(x * 255.);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t TVOutput::GetTexelValue(uint8_t r, uint8_t g, uint8_t b) const {
    return (uint32_t)b << 0u | (uint32_t)g << 8u | (uint32_t)r << 16u;
}

void TVOutput::InitPalette() {
    for (size_t i = 0; i < 16; ++i) {
        for (size_t j = 0; j < 16; ++j) {
            double a = pow(i / 15., m_gamma);
            double b = pow(j / 15., m_gamma);

            double value = pow((a + b + b) / 3, 1. / m_gamma);

            m_blend[i][j] = GetByte(value);
        }
    }

    m_usec_marker_xor = this->GetTexelValue(0, 128, 128);
    m_half_usec_marker_xor = this->GetTexelValue(128, 0, 0);
    m_6845_raster0_marker_xor = this->GetTexelValue(128, 128, 0);
    m_6845_dispen_marker_xor = this->GetTexelValue(128, 0, 128);

    m_beam_marker_xor = this->GetTexelValue(255, 255, 255);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if VIDEO_TRACK_METADATA
void TVOutput::AddMetadataMarkers(void *dest_pixels,
                                  size_t dest_pitch_bytes,
                                  bool add,
                                  uint8_t metadata_flag,
                                  uint32_t xor_value) const {
    if (!add) {
        return;
    }

    for (size_t y = 0; y < TV_TEXTURE_HEIGHT; y += 2) {
        auto dest = (uint32_t *)((char *)dest_pixels + y * dest_pitch_bytes);
        const uint32_t *src = m_texture_pixels.data() + y * TV_TEXTURE_WIDTH;
        const VideoDataUnit *unit = m_texture_units.data() + y * TV_TEXTURE_WIDTH;

        for (size_t x = 0; x < TV_TEXTURE_WIDTH; ++x) {
            if (unit++->metadata.flags & metadata_flag) {
                dest[x] = src[x] ^ xor_value;
            }
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
