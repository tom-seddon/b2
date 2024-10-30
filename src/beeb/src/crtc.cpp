#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/6502.h>
#include <string.h>
#include <beeb/crtc.h>
#include <beeb/Trace.h>

#include <shared/enum_def.h>
#include <beeb/crtc.inl>
#include <shared/enum_end.h>

// https://stardot.org.uk/forums/viewtopic.php?f=4&t=14971

// https://stardot.org.uk/forums/viewtopic.php?f=57&t=14988

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// bottom 8 bits are the data mask.
enum {
    CrtcRegister_CanRead = 256,
    CrtcRegister_CanWrite = 512,
};

// this is indexed by the full 5 bits of the address register, but the
// CRTC registers array only has 18 slots. So indexes 18 and above
// must be 0!
static const uint16_t CRTC_REGISTERS[32] = {
    0xff | CrtcRegister_CanWrite,                        //0
    0xff | CrtcRegister_CanWrite,                        //1
    0xff | CrtcRegister_CanWrite,                        //2
    0xff | CrtcRegister_CanWrite,                        //3
    0x7f | CrtcRegister_CanWrite,                        //4
    0x1f | CrtcRegister_CanWrite,                        //5
    0x7f | CrtcRegister_CanWrite,                        //6
    0x7f | CrtcRegister_CanWrite,                        //7
    0xf3 | CrtcRegister_CanWrite,                        //8
    0x1f | CrtcRegister_CanWrite,                        //9
    0x7f | CrtcRegister_CanWrite,                        //10
    0x1f | CrtcRegister_CanWrite,                        //11
    0x3f | CrtcRegister_CanWrite | CrtcRegister_CanRead, //12
    0xff | CrtcRegister_CanWrite | CrtcRegister_CanRead, //13
    0x3f | CrtcRegister_CanWrite | CrtcRegister_CanRead, //14
    0xff | CrtcRegister_CanWrite | CrtcRegister_CanRead, //15
    CrtcRegister_CanRead,                                //16
    CrtcRegister_CanRead,                                //17
};

static const uint8_t RASTER_MASK = 0x1f;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void CRTC_Init(CRTC *c) {
//    memset(c,0,sizeof *c);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t CRTC::ReadAddress(void *c_, M6502Word a) {
    (void)c_, (void)a;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CRTC::WriteAddress(void *c_, M6502Word a, uint8_t value) {
    auto c = (CRTC *)c_;
    (void)a;

    c->m_address = value & 31;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t CRTC::ReadData(void *c_, M6502Word a) {
    auto c = (CRTC *)c_;
    (void)a;

    if (CRTC_REGISTERS[c->m_address] & CrtcRegister_CanRead) {
        return c->m_registers.values[c->m_address];
    } else {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CRTC::WriteData(void *c_, M6502Word a, uint8_t value) {
    auto c = (CRTC *)c_;
    (void)a;

    uint16_t reg = CRTC_REGISTERS[c->m_address];
    if (reg & CrtcRegister_CanWrite) {
        TRACEF(c->m_trace, "6845 - %u - R%u was %u ($%02x), now %u ($%02x)", c->m_st.num_updates, c->m_address, c->m_registers.values[c->m_address], c->m_registers.values[c->m_address], value & reg, value & reg);
        c->m_registers.values[c->m_address] = value & reg;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CRTC::Output CRTC::Update(uint8_t lightpen) {
    ++m_st.num_updates;

    if (lightpen && !m_st.old_lightpen) {
        m_registers.bits.penl = m_st.char_addr.b.l;
        m_registers.bits.penh = m_st.char_addr.b.h;
    }

    m_st.old_lightpen = !!lightpen;

    // Handle hsync.
    if (m_st.hsync_counter >= 0) {
        ++m_st.hsync_counter;
        m_st.hsync_counter &= 0xf;

        if (m_st.hsync_counter == m_registers.bits.nsw.bits.wh) {
            m_st.hsync_counter = -1;
        }
    }

    // Handle end of horizontal displayed.
    if (m_st.column == m_registers.values[1]) {
        // Latch next line address in case we are in the last line of a
        // character row.
        m_st.next_line_addr = m_st.char_addr;

        m_st.hdisp = false;
    }

    // The last column never displays.
    if (m_st.column == m_registers.values[0]) {
        m_st.hdisp = false;
    }

    // Initiate hsync.
    if (m_st.column == m_registers.values[2]) {
        if (m_st.hsync_counter < 0) {
            m_st.hsync_counter = 0;
        }
    }

    // Handle vsync.
    bool half_r0_hit = m_st.column == (m_registers.values[0] >> 1);
    bool is_vsync_point = !m_registers.bits.r8.bits.s || !m_st.do_even_frame_logic || half_r0_hit;
    bool vsync_ending = false;
    bool vsync_starting = false;

    if (m_st.vsync_counter == m_registers.bits.nsw.bits.wv && is_vsync_point) {
        vsync_ending = true;
        m_st.vsync_counter = -1;
    }

    if (m_st.row == m_registers.values[7] && m_st.vsync_counter < 0 && !m_st.had_vsync_this_row && is_vsync_point) {
        vsync_starting = true;
    }

    // A vsync will initiate at any character and scanline position, provided
    // there isn't one in progress and provided there wasn't already one in
    // this character row.
    if (vsync_starting && !vsync_ending) {
        m_st.had_vsync_this_row = true;
        m_st.vsync_counter = 0;
    }

    M6502Word used_char_addr = m_st.char_addr;

    Output output;

    output.address = used_char_addr.w;
    output.raster = m_st.raster;

    // CRTC MA always increments, display or not.
    ++m_st.char_addr.w;

    // The Hitachi 6845 decides to end (or never enter) vertical adjust here,
    // one clock after checking whether to enter vertical adjust.
    if (m_st.check_vadj) {
        m_st.check_vadj = false;
        if (m_st.end_of_main_latched) {
            if (m_st.vadj_counter == m_registers.values[5]) {
                m_st.end_of_vadj_latched = true;
            }

            ++m_st.vadj_counter;
            m_st.vadj_counter &= 0x1f;
        }
    }

    // The Hitachi 6845 appears to latch some form of "last scanline of the
    // frame" state.
    if (m_st.column == 1) {
        if (m_st.row == m_registers.values[4]) {
            if (m_st.raster == m_registers.values[9]) {
                m_st.end_of_main_latched = true;
                m_st.vadj_counter = 0;
            }
        }

        m_st.check_vadj = true;
    }

    // Handle horizontal total.
    if (m_st.column == m_registers.values[0]) {
        TRACEF_IF(m_trace_scanlines, m_trace, "6845 - %u - end of scanline", m_st.num_updates);
        this->EndOfScanline();

        m_st.column = 0;
        m_st.hdisp = true;
    } else {
        if (m_st.hdisp && m_st.vdisp) {
            if (m_registers.bits.r8.bits.d != 3) {
                m_st.skewed_display |= 1 << m_registers.bits.r8.bits.d;
            }

            if (used_char_addr.b.h == m_registers.bits.cursorh &&
                used_char_addr.b.l == m_registers.bits.cursorl) {
                //if (m_st.raster == 0) {
                //    m_st.cursor = false;
                //}

                //if (m_st.raster == m_registers.bits.ncstart.bits.start) {
                //    m_st.cursor = true;
                //}

                m_st.cursor = m_st.raster >= m_registers.bits.ncstart.bits.start && m_st.raster <= m_registers.bits.ncend;

                if (m_st.cursor) {
                    if (m_registers.bits.r8.bits.c != 3) {
                        switch ((CRTCCursorMode)m_registers.bits.ncstart.bits.mode) {
                        case CRTCCursorMode_On:
                            m_st.skewed_cudisp |= 1 << m_registers.bits.r8.bits.c;
                            break;

                        case CRTCCursorMode_Off:
                            break;

                        case CRTCCursorMode_Blink16:
                            // 8 frames on, 8 frames off
                            if ((m_num_frames & 8) != 0) {
                                m_st.skewed_cudisp |= 1 << m_registers.bits.r8.bits.c;
                            }
                            break;

                        case CRTCCursorMode_Blink32:
                            // 16 frames on, 16 frames off
                            if ((m_num_frames & 16) != 0) {
                                m_st.skewed_cudisp |= 1 << m_registers.bits.r8.bits.c;
                            }
                            break;
                        }
                    }
                }

                //if (m_st.raster == m_registers.bits.ncend) {
                //    m_st.cursor = false;
                //}
            }
        }

        ++m_st.column;
        TRACEF_IF(m_trace_scanlines, m_trace, "6845 - %u - column was %u now %u", m_st.num_updates, m_st.column - 1, m_st.column);
    }

    // Handle end of vertical displayed.
    bool r6_hit = m_st.row == m_registers.values[6];
    if (r6_hit && !m_st.first_scanline && m_st.vdisp) {
        m_st.vdisp = false;

        ++m_num_frames;
    }

    bool r7_hit = m_st.row == m_registers.values[7];

    if (r6_hit || r7_hit) {
        m_st.do_even_frame_logic = !!(m_num_frames & 1);
    }

    output.display = m_st.skewed_display & 1;
    m_st.skewed_display >>= 1;

    output.cudisp = m_st.skewed_cudisp & 1;
    m_st.skewed_cudisp >>= 1;

    output.hsync = m_st.hsync_counter >= 0;
    output.vsync = m_st.vsync_counter >= 0;

    return output;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CRTC::EndOfFrame() {
    m_st.row = 0;
    m_st.first_scanline = true;
    m_st.next_line_addr.b.l = m_registers.values[13];
    m_st.next_line_addr.b.h = m_registers.values[12];
    m_st.line_addr = m_st.next_line_addr;
    m_st.vdisp = true;
    m_st.hdisp = false;
    if (m_st.vsync_counter < 0) {
        m_st.do_even_frame_logic = false;
    }
}

void CRTC::EndOfRow() {
    ++m_st.row;

    m_st.raster = 0;
    m_st.had_vsync_this_row = false;
}

void CRTC::EndOfScanline() {
    m_st.first_scanline = false;

    if (m_st.vsync_counter >= 0) {
        ++m_st.vsync_counter;
        m_st.vsync_counter &= 0xf;
    }

    // Increment scanline and check R9.
    bool r9_hit;
    if (m_registers.bits.r8.bits.s && m_registers.bits.r8.bits.v) {
        // Judging by MODE7:?&FE00=9:?&FE01=17, the real hardware
        // must do something similar when comparing?
        r9_hit = (m_st.raster >> 1) == (m_registers.values[9] >> 1);

        m_st.raster += 2;
    } else {
        r9_hit = m_st.raster == m_registers.values[9];

        ++m_st.raster;
    }
    m_st.raster &= RASTER_MASK;

    if (r9_hit) {
        m_st.line_addr = m_st.next_line_addr;
    }

    if (!m_st.in_vadj && r9_hit) {
        this->EndOfRow();
    }

    if (m_st.end_of_main_latched && !m_st.end_of_vadj_latched) {
        m_st.in_vadj = true;
    }

    bool end_of_frame = false;

    if (m_st.end_of_frame_latched) {
        end_of_frame = true;
    }

    if (m_st.end_of_vadj_latched) {
        m_st.in_vadj = false;

        if (m_registers.bits.r8.bits.s && m_st.do_even_frame_logic) {
            m_st.in_dummy_raster = true;
            m_st.end_of_frame_latched = true;
        } else {
            end_of_frame = true;
        }
    }

    if (end_of_frame) {
        m_st.end_of_main_latched = false;
        m_st.end_of_vadj_latched = false;
        m_st.end_of_frame_latched = false;
        m_st.in_dummy_raster = false;

        this->EndOfRow();
        this->EndOfFrame();
    }

    m_st.char_addr = m_st.line_addr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void CRTC::SetTrace(Trace *t,
                    bool trace_scanlines,
                    bool trace_scanlines_separators) {
    // these just need to go somewhere... anywhere will do...
    CHECK_SIZEOF(RegisterBits, 18);
    CHECK_SIZEOF(Registers, 18);
    CHECK_SIZEOF(R3, 1);
    CHECK_SIZEOF(R8, 1);
    CHECK_SIZEOF(R10, 1);
    CHECK_SIZEOF(Output, 4);

    m_trace = t;
    m_trace_scanlines = trace_scanlines;
    m_trace_scanlines_separators = trace_scanlines_separators;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
