#include <shared/system.h>
#include <6502/6502.h>
#include <string.h>
#include <beeb/VideoULA.h>
#include <beeb/video.h>
#include <shared/debug.h>
#include <shared/log.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(VU,"VIDULA",&log_printer_stdout_and_debugger);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WriteControlRegister(void *ula_,M6502Word a,uint8_t value) {
    auto ula=(VideoULA *)ula_;
    (void)a;

    ula->control.value=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WritePalette(void *ula_,M6502Word a,uint8_t value) {
    auto ula=(VideoULA *)ula_;
    (void)a;

    uint8_t phy=(value&0x0f)^7;
    uint8_t log=value>>4;

    ula->m_palette[log]=phy;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WriteNuLAControlRegister(void *ula_,M6502Word a,uint8_t value) {
    auto ula=(VideoULA *)ula_;

    if(ula->m_disable_a1) {
        WriteControlRegister(ula_,a,value);
    } else {
        uint8_t code=value>>4,param=value&0xf;

        LOGF(VU,"NuLA Control: code=%u, param=%u\n",code,param);

        switch(code) {
        case 1:
            // Toggle direct palette mode.
            ula->m_direct_palette=param;
            break;

        case 4:
            // Reset NuLA state.
            ula->ResetNuLAState();
            break;

        case 5:
            // Disable A1.
            ula->m_disable_a1=1;
            break;

        case 8:
            // Set flashing flags for logical colours 8-11.
            ula->m_flash[8]=param&0x08;
            ula->m_flash[9]=param&0x04;
            ula->m_flash[10]=param&0x02;
            ula->m_flash[11]=param&0x01;
            break;

        case 9:
            // Set flashing flags for logical colours 12-15.
            ula->m_flash[12]=param&0x08;
            ula->m_flash[13]=param&0x04;
            ula->m_flash[14]=param&0x02;
            ula->m_flash[15]=param&0x01;
            break;

        default:
            // Ignore...
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WriteNuLAPalette(void *ula_,M6502Word a,uint8_t value) {
    auto ula=(VideoULA *)ula_;

    if(ula->m_disable_a1) {
        WritePalette(ula_,a,value);
    } else {
        if(ula->m_nula_palette_write_state) {
            uint8_t index=ula->m_nula_palette_write_buffer>>4;
            ula->m_output_palette[index]=(ula->m_nula_palette_write_buffer&0xf)<<8|value;

            LOGF(VU,"NuLA Palette: index=%u, value=0x%06X\n",index,ula->m_output_palette[index]&0xfff);
        } else {
            ula->m_nula_palette_write_buffer=value;
        }

        ula->m_nula_palette_write_state=!ula->m_nula_palette_write_state;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoULA::VideoULA() {
    this->ResetNuLAState();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Byte(uint8_t byte) {
    m_byte=byte;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::ResetNuLAState() {
    // Reset output palette.
    for(size_t i=0;i<16;++i) {
        if(i&1) {
            m_output_palette[i]|=0xf00;
        }

        if(i&2) {
            m_output_palette[i]|=0x0f0;
        }

        if(i&4) {
            m_output_palette[i]|=0x00f;
        }
    }

    // Reset flash flags.
    for(size_t i=0;i<8;++i) {
        m_flash[8+i]=1;
    }

    // Don't use direct palette.
    m_direct_palette=0;

    // Reset palette write state.
    m_nula_palette_write_state=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint16_t VideoULA::Shift() {
    uint8_t index=m_byte;
    index=((index>>1)&1)|((index>>2)&2)|((index>>3)&4)|((index>>4)&8);

    m_byte<<=1;
    m_byte|=1;

    if(!m_direct_palette) {
        index=m_palette[index];

        if(m_flash[index]) {
            if(this->control.bits.flash) {
                index^=7;
            }
        }
    }

    uint16_t value=m_output_palette[index];
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Emit2MHz(VideoDataHalfUnit *hu) {
    uint16_t value=this->Shift();

    hu->bitmap.pixels[0]=value;
    hu->bitmap.pixels[1]=value;
    hu->bitmap.pixels[2]=value;
    hu->bitmap.pixels[3]=value;
    hu->bitmap.pixels[4]=value;
    hu->bitmap.pixels[5]=value;
    hu->bitmap.pixels[6]=value;
    hu->bitmap.pixels[7]=value;
}

void VideoULA::Emit4MHz(VideoDataHalfUnit *hu) {
    uint16_t value;

    value=this->Shift();
    hu->bitmap.pixels[0]=value;
    hu->bitmap.pixels[1]=value;
    hu->bitmap.pixels[2]=value;
    hu->bitmap.pixels[3]=value;

    value=this->Shift();
    hu->bitmap.pixels[4]=value;
    hu->bitmap.pixels[5]=value;
    hu->bitmap.pixels[6]=value;
    hu->bitmap.pixels[7]=value;
}

void VideoULA::Emit8MHz(VideoDataHalfUnit *hu) {
    uint16_t value;

    value=this->Shift();
    hu->bitmap.pixels[0]=value;
    hu->bitmap.pixels[1]=value;

    value=this->Shift();
    hu->bitmap.pixels[2]=value;
    hu->bitmap.pixels[3]=value;

    value=this->Shift();
    hu->bitmap.pixels[4]=value;
    hu->bitmap.pixels[5]=value;

    value=this->Shift();
    hu->bitmap.pixels[6]=value;
    hu->bitmap.pixels[7]=value;
}

void VideoULA::Emit16MHz(VideoDataHalfUnit *hu) {
    uint16_t value;

    value=this->Shift();
    hu->bitmap.pixels[0]=value;

    value=this->Shift();
    hu->bitmap.pixels[1]=value;

    value=this->Shift();
    hu->bitmap.pixels[2]=value;

    value=this->Shift();
    hu->bitmap.pixels[3]=value;

    value=this->Shift();
    hu->bitmap.pixels[4]=value;

    value=this->Shift();
    hu->bitmap.pixels[5]=value;

    value=this->Shift();
    hu->bitmap.pixels[6]=value;

    value=this->Shift();
    hu->bitmap.pixels[7]=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoULA::EmitMFn VideoULA::EMIT_MFNS[4]={
    &VideoULA::Emit2MHz,
    &VideoULA::Emit4MHz,
    &VideoULA::Emit8MHz,
    &VideoULA::Emit16MHz,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
