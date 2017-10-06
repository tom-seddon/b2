#include <shared/system.h>
#include <6502/6502.h>
#include <string.h>
#include <beeb/VideoULA.h>
#include <beeb/video.h>
#include <shared/debug.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint16_t COLOURS[8]={
    0x0000,
    0x0f00,
    0x00f0,
    0x0ff0,
    0x000f,
    0x0f0f,
    0x00ff,
    0x0fff,
};

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

void VideoULA::Byte(uint8_t byte) {
    m_byte=byte;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t FLASH_XOR_VALUES[2][2]={
    {0,8,},
    {0,15,},
};

// TODO need to work through this again, post-NuLA - the flashing
// stuff can be done directly on the 12bpp values, surely??
uint16_t VideoULA::Shift() {
    uint8_t index=m_byte;
    index=((index>>1)&1)|((index>>2)&2)|((index>>3)&4)|((index>>4)&8);

    m_byte<<=1;
    m_byte|=1;

    uint8_t value=m_palette[index];

    value^=FLASH_XOR_VALUES[this->control.bits.flash][value>>3];

    ASSERT(value>=0&&value<8);
    return COLOURS[value];
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
