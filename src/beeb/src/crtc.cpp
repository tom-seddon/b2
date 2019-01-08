#include <shared/system.h>
#include <shared/debug.h>
#include <6502/6502.h>
#include <string.h>
#include <beeb/crtc.h>
#include <beeb/Trace.h>

#include <shared/enum_def.h>
#include <beeb/crtc.inl>
#include <shared/enum_end.h>

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

// These delays are in video data half units, assuming 1MHz. The delays are
// halved when the 6845 is running at 2MHz.
static const unsigned DELAYS[]={
    0,//0us
    2,//1us
    4,//2us
    1000000,//i.e., infinity
};

CRTC::Output CRTC::Update(uint8_t fast_6845) {
    // these just need to go somewhere...
    CHECK_SIZEOF(RegisterBits,18);
    CHECK_SIZEOF(Registers,18);
    CHECK_SIZEOF(R3,1);
    CHECK_SIZEOF(R8,1);
    CHECK_SIZEOF(R10,1);
    CHECK_SIZEOF(Output,4);
    ASSERT(fast_6845==0||fast_6845==1);

    Output output={};

    if(m_interlace_delay>0) {
        --m_interlace_delay;
        if(m_interlace_delay==0) {
            output.vsync=1;
        }
        return output;
    }

    const unsigned delay=DELAYS[m_registers.bits.r8.bits.d]>>fast_6845;

    if(m_vsync_left>0) {
        // vysnc line
        ASSERT(m_hsync_left==0);
        output.vsync=1;
    } else if(m_hsync_left>0) {
        // hblank region
        output.hsync=1;
        output.display=m_row<m_registers.values[6]&&!m_adj;
        //m_nadj_left==0;//m_registers.bits.r8.bits.d!=3;
        --m_hsync_left;
    } else if(m_row>=m_registers.values[6]) {
        // Off the bottom of the screen.
    } else {
        // Skew-related(?) notes that need revisiting once I'm no
        // longer sick of it:
        //
        // - The display skew is set to account for Mode 7, which has
        //   some kind of 1-char delay between input and output. So
        //   there should probably also be a 1-byte buffer in BBCMicro
        //   to simulate this.
        // 
        //   That would push the screen one to the right. But that
        //   could be offset by handling the address generation
        //   properly here - if dispen were delayed, but the address
        //   weren't, then each row would be positioned where it
        //   should be
        //
        // - The cursor generation ought to be affected by the cursor
        //   delay, which at the moment it isn't (but since it's
        //   skewed too, along with the address calculation, it all
        //   hangs together)
        // 
        // - On my M128, Mode 7 is still 1 char further to the right
        //   than Mode 4/5/6 - where does this come from? Is this the
        //   extra char from the hsync position (which in Mode7 is 51,
        //   rather than 49, even though the skew is only 1)? Or maybe
        //   I'm completely wrong?
        //
        // - On my M128, Mode 0/1/2/3 are 1 column to the left of Mode
        //   4/5/6... which I'm just going to pretend I didn't notice

        if(m_column>=delay&&m_column<m_registers.values[1]+delay) {
            output.display=1;

            output.raster=m_raster;
            ASSERT(output.raster==m_raster);

            output.address=m_char_addr.w;
            ASSERT(output.address==m_char_addr.w);

            if(m_char_addr.b.h==m_registers.bits.cursorh&&
                m_char_addr.b.l==m_registers.bits.cursorl&&
                output.raster>=m_registers.bits.ncstart.bits.start&&
                output.raster<m_registers.bits.ncend)
            {
                if(m_registers.bits.r8.bits.c==3) {
                    // No cursor output in this case.
                } else {
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

            ++m_char_addr.w;
            m_char_addr.w&=0x3fff;
        }
    }

    if(m_column>=m_registers.values[0]) {
#if BBCMICRO_TRACE
        TRACEF_IF(m_trace_scanlines,m_trace,"6845 - scanline end: %u\n",m_trace_scanline);

        ++m_trace_scanline;
#endif

        // advance to next scanline.
        m_column=0;

        // keep the vsync counter going.
        if(m_vsync_left>0) {
            --m_vsync_left;
        }

        if(m_adj) {
            this->NextRaster();
            
            if(m_raster>=m_registers.values[5]) {
                m_adj=false;
                this->StartOfFrame();
            }
        } else if(m_raster<m_registers.values[9]) {
            this->NextRaster();
        } else {
            m_raster=0;

            ++m_row;

            if(m_row==m_registers.values[7]) {
                m_hsync_left=0;

                if(m_registers.bits.r8.bits.s) {//&&m_odd_frame) {
                    m_interlace_delay=(m_registers.values[0]+1)/2;
                } else {
                    output.vsync=1;
                }

                // TODO - is this the right time to do this?
                ++m_num_frames;

                m_vsync_left=m_registers.bits.nsw.bits.wv;
                if(m_vsync_left==0) {
                    m_vsync_left=16;
                }

                TRACEF_IF(m_trace_scanlines,m_trace,"6845 - vsync begin: %u scanline(s)\n",m_vsync_left);
            }

            // calculate next start address.
            m_line_addr.w+=m_registers.values[1];
            m_line_addr.w&=0x3fff;

            m_char_addr=m_line_addr;

            if(m_row==m_registers.values[4]+1) {
                if(m_registers.values[5]==0) {
                    this->StartOfFrame();
                } else {
                    m_adj=true;
                }
            }
            
            ASSERT(!(m_line_addr.b.h&~0x3f));

        }

        TRACEF_IF(m_trace_scanline,m_trace,"6845 - start of scanline %u: CRTC addr: $%04X; delay: %u; raster: %u; row: %u; vsync_left: %u; adj: %s\n",
                  m_trace_scanline,m_line_addr.w,delay,m_raster,m_row,m_vsync_left,BOOL_STR(m_adj));
    } else {
        // advance to next column in row.
        ++m_column;

        if(m_column==m_registers.values[2]) {
            if(m_vsync_left>0) {
                // ignore the hblank in this situation.
            } else {
                m_hsync_left=m_registers.bits.nsw.bits.wh;

                TRACEF_IF(m_trace_scanlines,m_trace,"6845 - hblank begin.\n");
            }
        }
    }

    return output;
}

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

void CRTC::NextRaster() {
    m_char_addr.w=m_line_addr.w;

    if(m_registers.bits.r8.bits.v&&m_registers.bits.r8.bits.s) {
        // Interlace sync and video
        m_raster+=2;
    } else {
        // Interlace sync, no interlace
        ++m_raster;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CRTC::StartOfFrame() {
    m_line_addr.b.h=m_registers.values[12];
    m_line_addr.b.l=m_registers.values[13];

    m_char_addr=m_line_addr;

    m_row=0;
    m_raster=0;
    
#if BBCMICRO_TRACE
    m_trace_scanline=0;

    TRACEF(m_trace,"6845 - start of frame. CRTC address: $%04X\n",m_line_addr.w);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
