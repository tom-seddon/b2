#include <shared/system.h>
#include <6502/6502.h>
#include <string.h>
#include <beeb/VideoULA.h>
#include <beeb/video.h>
#include <shared/debug.h>
#include <shared/log.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(VU,"VIDULA",&log_printer_stdout_and_debugger,false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::WriteControlRegister(void *ula_,M6502Word a,uint8_t value) {
    auto ula=(VideoULA *)ula_;
    (void)a;

    if(value!=ula->control.value) {
        ula->control.value=value;

        LOGF(VU,"ULA Control: Flash=%s Teletext=%s Line Width=%d Fast6845=%s Cursor=%d\n",BOOL_STR(ula->control.bits.flash),BOOL_STR(ula->control.bits.teletext),ula->control.bits.line_width,BOOL_STR(ula->control.bits.fast_6845),ula->control.bits.cursor);
    }
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


        switch(code) {
        case 1:
            // Toggle direct palette mode.
            ula->m_direct_palette=param;
            LOGF(VU,"NuLA Control: Direct Palette=%s\n",BOOL_STR(ula->m_direct_palette));
            break;

        case 4:
            // Reset NuLA state.
            ula->ResetNuLAState();
            LOGF(VU,"NuLA Control: Reset NuLA state\n");
            break;

        case 5:
            // Disable A1.
            ula->m_disable_a1=1;
            LOGF(VU,"NuLA Control: Disable A1\n");
            break;

        case 6:
            // Attribute modes on/off.
            ula->m_attribute_mode.bits.enabled=!!param;
            LOGF(VU,"NuLA Control: Attribute Mode=%s\n",BOOL_STR(ula->m_attribute_mode.bits.enabled));
            break;

        case 7:
            // Text attribute modes on/off.
            ula->m_attribute_mode.bits.text=!!param;
            LOGF(VU,"NuLA Control: Text Attribute Mode=%s\n",BOOL_STR(ula->m_attribute_mode.bits.text));
            break;

        case 8:
            // Set flashing flags for logical colours 8-11.
            ula->m_flash[8]=param&0x08;
            ula->m_flash[9]=param&0x04;
            ula->m_flash[10]=param&0x02;
            ula->m_flash[11]=param&0x01;
            LOGF(VU,"NuLA Control: Flash: 8=%s 9=%s 10=%s 11=%s\n",BOOL_STR(ula->m_flash[8]),BOOL_STR(ula->m_flash[9]),BOOL_STR(ula->m_flash[10]),BOOL_STR(ula->m_flash[11]));
            break;

        case 9:
            // Set flashing flags for logical colours 12-15.
            ula->m_flash[12]=param&0x08;
            ula->m_flash[13]=param&0x04;
            ula->m_flash[14]=param&0x02;
            ula->m_flash[15]=param&0x01;
            LOGF(VU,"NuLA Control: Flash: 12=%s 13=%s 14=%s 15=%s\n",BOOL_STR(ula->m_flash[12]),BOOL_STR(ula->m_flash[13]),BOOL_STR(ula->m_flash[14]),BOOL_STR(ula->m_flash[15]));
            break;

        default:
            // Ignore...
            LOGF(VU,"NuLA Control: code=%u, param=%u\n",code,param);
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
            VideoDataBitmapPixel *entry=&ula->m_output_palette[index];

            entry->r=ula->m_nula_palette_write_buffer&0xf;
            entry->g=value>>4;
            entry->b=value&0xf;

            LOGF(VU,"NuLA Palette: index=%u, rgb=0x%x%x%x\n",index,entry->r,entry->g,entry->b);
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
    m_original_byte=m_byte=byte;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitPixels(union VideoDataHalfUnit *hu) {
    (this->*EMIT_MFNS[this->m_attribute_mode.value][this->control.bits.fast_6845][this->control.bits.line_width])(hu);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::ResetNuLAState() {
    // Reset output palette.
    for(size_t i=0;i<16;++i) {
        VideoDataBitmapPixel *pixel=&m_output_palette[i];

        pixel->x=0;
        pixel->r=i&1?15:0;
        pixel->g=i&2?15:0;
        pixel->b=i&4?15:0;
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

VideoDataBitmapPixel VideoULA::GetPalette(uint8_t index) {

    if(!m_direct_palette) {
        index=m_palette[index];

        if(m_flash[index]) {
            if(this->control.bits.flash) {
                index^=7;
            }
        }
    }

    return m_output_palette[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataBitmapPixel VideoULA::Shift() {
    uint8_t index=m_byte;
    index=((index>>1)&1)|((index>>2)&2)|((index>>3)&4)|((index>>4)&8);

    m_byte<<=1;
    m_byte|=1;

    return this->GetPalette(index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataBitmapPixel VideoULA::ShiftAttributeMode0() {
    uint8_t attribute=m_original_byte&0x03;

    uint8_t index=m_byte>>7|attribute<<2;

    m_byte<<=1;
    m_byte|=1;

    return this->GetPalette(index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataBitmapPixel VideoULA::ShiftAttributeMode1() {
    uint8_t index=(m_original_byte>>1)&8|(m_original_byte<<2)&4|(m_byte>>6)&2|(m_byte>>3)&1;

    m_byte<<=1;
    m_byte|=1;

    return this->GetPalette(index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoDataBitmapPixel VideoULA::ShiftAttributeText() {
    uint8_t index=(m_byte>>7|m_original_byte<<1)&0xf;

    m_byte<<=1;
    m_byte&=0xf0;

    return this->GetPalette(index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Emit2MHz(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel=this->Shift();

    hu->bitmap.pixels[0]=pixel;
    hu->bitmap.pixels[1]=pixel;
    hu->bitmap.pixels[2]=pixel;
    hu->bitmap.pixels[3]=pixel;
    hu->bitmap.pixels[4]=pixel;
    hu->bitmap.pixels[5]=pixel;
    hu->bitmap.pixels[6]=pixel;
    hu->bitmap.pixels[7]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Emit4MHz(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[0]=pixel;
    hu->bitmap.pixels[1]=pixel;
    hu->bitmap.pixels[2]=pixel;
    hu->bitmap.pixels[3]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[4]=pixel;
    hu->bitmap.pixels[5]=pixel;
    hu->bitmap.pixels[6]=pixel;
    hu->bitmap.pixels[7]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Emit8MHz(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[0]=pixel;
    hu->bitmap.pixels[1]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[2]=pixel;
    hu->bitmap.pixels[3]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[4]=pixel;
    hu->bitmap.pixels[5]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[6]=pixel;
    hu->bitmap.pixels[7]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Emit16MHz(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[0]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[1]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[2]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[3]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[4]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[5]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[6]=pixel;

    pixel=this->Shift();
    hu->bitmap.pixels[7]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeMode0(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    hu->type.x=BeebControlPixel_NuLAAttribute;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[1]=pixel;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[2]=pixel;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[3]=pixel;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[4]=pixel;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[5]=pixel;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[6]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeMode1(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    hu->type.x=BeebControlPixel_NuLAAttribute;

    pixel=this->ShiftAttributeMode1();
    hu->bitmap.pixels[1]=pixel;
    hu->bitmap.pixels[2]=pixel;

    pixel=this->ShiftAttributeMode1();
    hu->bitmap.pixels[3]=pixel;
    hu->bitmap.pixels[4]=pixel;

    pixel=this->ShiftAttributeMode1();
    hu->bitmap.pixels[5]=pixel;
    hu->bitmap.pixels[6]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeMode4(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    hu->type.x=BeebControlPixel_NuLAAttribute;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[1]=pixel;
    hu->bitmap.pixels[2]=pixel;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[3]=pixel;
    hu->bitmap.pixels[4]=pixel;

    pixel=this->ShiftAttributeMode0();
    hu->bitmap.pixels[5]=pixel;
    hu->bitmap.pixels[6]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeTextMode4(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    hu->type.x=BeebControlPixel_NuLAAttribute;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[1]=pixel;
    hu->bitmap.pixels[2]=pixel;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[3]=pixel;
    hu->bitmap.pixels[4]=pixel;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[5]=pixel;
    hu->bitmap.pixels[6]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNuLAAttributeTextMode0(VideoDataHalfUnit *hu) {
    VideoDataBitmapPixel pixel;

    hu->type.x=BeebControlPixel_NuLAAttribute;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[1]=pixel;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[2]=pixel;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[3]=pixel;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[4]=pixel;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[5]=pixel;

    pixel=this->ShiftAttributeText();
    hu->bitmap.pixels[6]=pixel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::EmitNothing(VideoDataHalfUnit *hu) {
    hu->type.x=BeebControlPixel_Nothing;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoULA::EmitMFn VideoULA::EMIT_MFNS[4][2][4]={
    // Not attribute mode, not text attribute mode
    {
        // Slow 6845
        {
            &VideoULA::Emit2MHz,
            &VideoULA::Emit4MHz,
            &VideoULA::Emit8MHz,
            &VideoULA::Emit16MHz,
        },

        // Fast 6845
        {
            &VideoULA::Emit2MHz,
            &VideoULA::Emit4MHz,
            &VideoULA::Emit8MHz,
            &VideoULA::Emit16MHz,
        },
    },

    // Attribute mode, not text attribute mode
    {
        // Slow 6845
        {
            &VideoULA::EmitNothing,
            &VideoULA::EmitNothing,
            &VideoULA::EmitNuLAAttributeMode4,
            &VideoULA::EmitNothing,
        },

        // Fast 6845
        {
            &VideoULA::EmitNothing,
            &VideoULA::EmitNothing,
            &VideoULA::EmitNuLAAttributeMode1,
            &VideoULA::EmitNuLAAttributeMode0,
        },
    },

    // Not attribute mode, text attribute mode
    {
        // Slow 6845
        {
            &VideoULA::Emit2MHz,
            &VideoULA::Emit4MHz,
            &VideoULA::Emit8MHz,
            &VideoULA::Emit16MHz,
        },

        // Fast 6845
        {
            &VideoULA::Emit2MHz,
            &VideoULA::Emit4MHz,
            &VideoULA::Emit8MHz,
            &VideoULA::Emit16MHz,
        },
    },

    // Attribute mode, text attribute mode
    {
        // Slow 6845
        {
            &VideoULA::EmitNothing,
            &VideoULA::EmitNothing,
            &VideoULA::EmitNuLAAttributeTextMode4,
            &VideoULA::EmitNothing,
        },

        // Fast 6845
        {
            &VideoULA::EmitNothing,
            &VideoULA::EmitNothing,
            &VideoULA::EmitNothing,
            &VideoULA::EmitNuLAAttributeTextMode0,
        },
    },
};
