#include <shared/system.h>
#include <6502/6502.h>
#include <string.h>
#include <beeb/VideoULA.h>
#include <beeb/video.h>

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

uint8_t VideoULA::Shift() {
    uint8_t index=m_byte;
    index=((index>>1)&1)|((index>>2)&2)|((index>>3)&4)|((index>>4)&8);

    m_byte<<=1;
    m_byte|=1;

    uint8_t value=m_palette[index];

    value^=FLASH_XOR_VALUES[this->control.bits.flash][value>>3];

    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void VideoULA::Emit2MHz(VideoDataHalfUnit *hu) {
    uint8_t value=this->Shift();
    hu->pixels[0]=value;
    hu->pixels[1]=value;
    hu->pixels[2]=value;
    hu->pixels[3]=value;
    hu->pixels[4]=value;
    hu->pixels[5]=value;
    hu->pixels[6]=value;
    hu->pixels[7]=value;
}

void VideoULA::Emit4MHz(VideoDataHalfUnit *hu) {
    uint8_t value;

    value=this->Shift();
    hu->pixels[0]=value;
    hu->pixels[1]=value;
    hu->pixels[2]=value;
    hu->pixels[3]=value;

    value=this->Shift();
    hu->pixels[4]=value;
    hu->pixels[5]=value;
    hu->pixels[6]=value;
    hu->pixels[7]=value;
}

void VideoULA::Emit8MHz(VideoDataHalfUnit *hu) {
    uint8_t value;

    value=this->Shift();
    hu->pixels[0]=value;
    hu->pixels[1]=value;

    value=this->Shift();
    hu->pixels[2]=value;
    hu->pixels[3]=value;

    value=this->Shift();
    hu->pixels[4]=value;
    hu->pixels[5]=value;

    value=this->Shift();
    hu->pixels[6]=value;
    hu->pixels[7]=value;
}

void VideoULA::Emit16MHz(VideoDataHalfUnit *hu) {
    uint8_t value;

    value=this->Shift();
    hu->pixels[0]=value;

    value=this->Shift();
    hu->pixels[1]=value;

    value=this->Shift();
    hu->pixels[2]=value;

    value=this->Shift();
    hu->pixels[3]=value;

    value=this->Shift();
    hu->pixels[4]=value;

    value=this->Shift();
    hu->pixels[5]=value;

    value=this->Shift();
    hu->pixels[6]=value;

    value=this->Shift();
    hu->pixels[7]=value;
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
