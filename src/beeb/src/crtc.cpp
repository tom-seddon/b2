#include <shared/system.h>
#include <shared/debug.h>
#include <6502/6502.h>
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
    CrtcRegister_CanRead=256,
    CrtcRegister_CanWrite=512,
};

// this is indexed by the full 5 bits of the address register, but the
// CRTC registers array only has 18 slots. So indexes 18 and above
// must be 0!
static const uint16_t CRTC_REGISTERS[32]={
    0xff|CrtcRegister_CanWrite,//0
    0xff|CrtcRegister_CanWrite,//1
    0xff|CrtcRegister_CanWrite,//2
    0xff|CrtcRegister_CanWrite,//3
    0x7f|CrtcRegister_CanWrite,//4
    0x1f|CrtcRegister_CanWrite,//5
    0x7f|CrtcRegister_CanWrite,//6
    0x7f|CrtcRegister_CanWrite,//7
    0xf3|CrtcRegister_CanWrite,//8
    0x1f|CrtcRegister_CanWrite,//9
    0x7f|CrtcRegister_CanWrite,//10
    0x1f|CrtcRegister_CanWrite,//11
    0x3f|CrtcRegister_CanWrite|CrtcRegister_CanRead,//12
    0xff|CrtcRegister_CanWrite|CrtcRegister_CanRead,//13
    0x3f|CrtcRegister_CanWrite|CrtcRegister_CanRead,//14
    0xff|CrtcRegister_CanWrite|CrtcRegister_CanRead,//15
    CrtcRegister_CanRead,//16
    CrtcRegister_CanRead,//17
};

static const uint8_t RASTER_MASK=0x1f;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void CRTC_Init(CRTC *c) {
//    memset(c,0,sizeof *c);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t CRTC::ReadAddress(void *c_,M6502Word a) {
    (void)c_,(void)a;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CRTC::WriteAddress(void *c_,M6502Word a,uint8_t value) {
    auto c=(CRTC *)c_;
    (void)a;

    c->m_address=value&31;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t CRTC::ReadData(void *c_,M6502Word a) {
    auto c=(CRTC *)c_;
    (void)a;

    if(CRTC_REGISTERS[c->m_address]&CrtcRegister_CanRead) {
        return c->m_registers.values[c->m_address];
    } else {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CRTC::WriteData(void *c_,M6502Word a,uint8_t value) {
    auto c=(CRTC *)c_;
    (void)a;

    uint16_t reg=CRTC_REGISTERS[c->m_address];
    if(reg&CrtcRegister_CanWrite) {
        c->m_registers.values[c->m_address]=value&reg;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CRTC::Output CRTC::Update(uint8_t fast_6845) {
    // these just need to go somewhere...
    CHECK_SIZEOF(RegisterBits,18);
    CHECK_SIZEOF(Registers,18);
    CHECK_SIZEOF(R3,1);
    CHECK_SIZEOF(R8,1);
    CHECK_SIZEOF(R10,1);
    CHECK_SIZEOF(Output,4);
    ASSERT(fast_6845==0||fast_6845==1);

    ++m_num_updates;

    // The interlace delay counter appears to be special.
    if(m_interlace_delay_counter>=0) {
        ++m_interlace_delay_counter;
        if(m_interlace_delay_counter!=1+(m_registers.values[0]>>1)) {
            return {};
        }

        TRACEF_IF(m_trace_scanlines,m_trace,"6845 - %u - interlace delay counter done.",m_num_updates);
        m_interlace_delay_counter=-1;
    }

    // Horizontal sync counter.
    if(m_hsync_counter>=0) {
        ++m_hsync_counter;
        if(m_hsync_counter==m_registers.bits.nsw.bits.wh) {
            m_hsync_counter=-1;
        }
    } else {
        if(m_column==m_registers.values[2]) {
            m_hsync_counter=0;
        }
    }

    // Vertical displayed.
    if(m_row==m_registers.values[6]) {
        m_vdisp=false;
    }

    // Handle column 0.
    if(m_column==0) {
        if(m_vsync_counter>=0) {
            ++m_vsync_counter;

            uint8_t wv=m_registers.bits.nsw.bits.wv;
            if(wv==0) {
                wv=16;
            }

            if(m_vsync_counter==wv) {
                m_vsync_counter=-1;
            }
        } else if(m_row==m_registers.values[7]&&m_raster==0) {
            m_vsync_counter=0;
            m_num_updates=0;

            if(m_registers.bits.r8.bits.s) {
                m_interlace_delay_counter=0;
                return {};
            }
        }

        TRACEF_IF(m_trace_scanlines,m_trace,"6845 - %u - start of scanline: CRTC addr: $%04X; raster: %u; row: %u; vsync_counter: %d; adj counter: %d",m_num_updates,m_line_addr.w,m_raster,m_row,m_vsync_counter,m_adj_counter);
    }

    // Produce output.
    Output output={};

    output.vsync=m_vsync_counter>=0;
    output.hsync=m_hsync_counter>=0&&!output.vsync;

    if(m_adj_counter<0) {
        if(m_row==m_registers.values[4]&&m_raster==m_registers.values[9]) {
            m_adj_counter=0;
        }
    }

    // Handle column N-1.
    if(m_column==m_registers.values[0]) {
        m_hdisp=true;

        if(m_raster==m_registers.values[9]) {
        next_row:
            m_raster=0;
            ++m_row;
        } else {
            if(m_registers.bits.r8.bits.v&&m_registers.bits.r8.bits.s) {
                // Interlace sync and video

                // Judging by MODE7:?&FE00=9:?&FE01=17, the real hardware
                // must do something similar?
                if(m_raster+1==m_registers.values[9]) {
                    goto next_row;
                }

                m_raster+=2;
            } else {
                // Interlace sync, no interlace
                ++m_raster;
            }

            m_raster&=RASTER_MASK;
        }

        m_column=0;
        m_char_addr=m_line_addr;

        if(m_adj_counter>=0) {
            if(m_adj_counter==m_registers.values[5]) {
                m_line_addr.b.h=m_registers.values[12];
                m_line_addr.b.l=m_registers.values[13];

                m_char_addr=m_line_addr;

                m_adj_counter=-1;
                m_row=0;
                m_raster=0;
                m_vdisp=true;
                m_hdisp=true;
                m_column=0;
                ++m_num_frames;

                TRACEF(m_trace,"6845 - %u - start of frame. CRTC address: $%04X",m_num_updates,m_line_addr.w);
            } else {
                ++m_adj_counter;
            }
        }

        // Display is never produced in the last column.
    } else {
        if(m_hdisp&&m_vdisp) {
            if(m_registers.bits.r8.bits.d!=3) {
                m_skewed_display|=(2>>fast_6845)<<m_registers.bits.r8.bits.d;
            }

            output.address=m_char_addr.w;

            output.raster=m_raster;
            ASSERT(output.raster==m_raster);

            if(m_char_addr.b.h==m_registers.bits.cursorh&&
               m_char_addr.b.l==m_registers.bits.cursorl&&
               m_raster>=m_registers.bits.ncstart.bits.start&&
               m_raster<m_registers.bits.ncend&&
               m_registers.bits.r8.bits.c!=3)
            {
                switch((CRTCCursorMode)m_registers.bits.ncstart.bits.mode) {
                    case CRTCCursorMode_On:
                        output.cudisp=1;
                        break;

                    case CRTCCursorMode_Off:
                        break;

                    case CRTCCursorMode_Blink16:
                        // 8 frames on, 8 frames off
                        output.cudisp=(uint32_t)m_num_frames>>3;
                        break;

                    case CRTCCursorMode_Blink32:
                        // 16 frames on, 16 frames off
                        output.cudisp=(uint32_t)m_num_frames>>4;
                        break;
                }
            }
        }

        output.display=m_skewed_display&1;
        m_skewed_display>>=1;

        if(output.display) {
            ++m_char_addr.w;
        }

        ++m_column;

        // Horizontal displayed.
        if(m_column==m_registers.values[1]) {
            m_hdisp=false;

            // The 6845 appears to do this at this point - try, e.g.,
            // MODE1:?&FE00=1:?&FE01=128
            if(m_raster==m_registers.values[9]) {
                m_line_addr.w+=m_registers.values[1];
                m_char_addr.w=m_line_addr.w;
            }
        }
    }


    // Vertical.
    if(m_vdisp) {
        if(m_row==m_registers.values[6]) {
            TRACEF(m_trace,"6845 - %u - end of visible display.",m_num_updates);
            m_vdisp=false;
        }
    }

    return output;
}
//        // Skew-related(?) notes that need revisiting once I'm no
//        // longer sick of it:
//        //
//        // - The display skew is set to account for Mode 7, which has
//        //   some kind of 1-char delay between input and output. So
//        //   there should probably also be a 1-byte buffer in BBCMicro
//        //   to simulate this.
//        //
//        //   That would push the screen one to the right. But that
//        //   could be offset by handling the address generation
//        //   properly here - if dispen were delayed, but the address
//        //   weren't, then each row would be positioned where it
//        //   should be
//        //
//        // - The cursor generation ought to be affected by the cursor
//        //   delay, which at the moment it isn't (but since it's
//        //   skewed too, along with the address calculation, it all
//        //   hangs together)
//        //
//        // - On my M128, Mode 7 is still 1 char further to the right
//        //   than Mode 4/5/6 - where does this come from? Is this the
//        //   extra char from the hsync position (which in Mode7 is 51,
//        //   rather than 49, even though the skew is only 1)? Or maybe
//        //   I'm completely wrong?
//        //
//        // - On my M128, Mode 0/1/2/3 are 1 column to the left of Mode
//        //   4/5/6... which I'm just going to pretend I didn't notice
//
//        if(m_column>=delay&&m_column<m_registers.values[1]+delay) {
//            output.display=1;
//
//            output.raster=m_raster;
//            ASSERT(output.raster==m_raster);
//
//            output.address=m_char_addr.w;
//            ASSERT(output.address==m_char_addr.w);
//
//            if(m_char_addr.b.h==m_registers.bits.cursorh&&
//                m_char_addr.b.l==m_registers.bits.cursorl&&
//                output.raster>=m_registers.bits.ncstart.bits.start&&
//                output.raster<m_registers.bits.ncend)
//            {
//                if(m_registers.bits.r8.bits.c==3) {
//                    // No cursor output in this case.
//                } else {
//                    switch((CRTCCursorMode)m_registers.bits.ncstart.bits.mode) {
//                    case CRTCCursorMode_On:
//                        output.cudisp=1;
//                        break;
//
//                    case CRTCCursorMode_Off:
//                        break;
//
//                    case CRTCCursorMode_Blink16:
//                        // 8 frames on, 8 frames off
//                        output.cudisp=(uint32_t)m_num_frames>>3;
//                        break;
//
//                    case CRTCCursorMode_Blink32:
//                        // 16 frames on, 16 frames off
//                        output.cudisp=(uint32_t)m_num_frames>>4;
//                        break;
//                    }
//                }
//            }
//
//            ++m_char_addr.w;
//            m_char_addr.w&=0x3fff;
//        }
//    }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void CRTC::SetTrace(Trace *t,bool trace_scanlines) {
    m_trace=t;
    m_trace_scanlines=trace_scanlines;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
