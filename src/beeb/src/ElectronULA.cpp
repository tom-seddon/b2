#include <shared/system.h>
#include <shared/system.h>
#include <beeb/ElectronULA.h>
#include <beeb/video.h>
#include <shared/debug.h>

#if ENABLE_ELECTRON

#include <shared/enum_def.h>
#include <beeb/ElectronULA.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const VideoDataPixelBits PALETTE_PIXELS[8] = {
    {0, 0, 0, 0},    //0
    {0, 0, 15, 0},   //1=r
    {0, 15, 0, 0},   //2=g
    {0, 15, 15, 0},  //3=g+r
    {15, 0, 0, 0},   //4=b
    {15, 0, 15, 0},  //5=b+r
    {15, 15, 0, 0},  //6=b+g
    {15, 15, 15, 0}, //7=b+g+r
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ElectronULA::ElectronULA() {
    this->irq.bits.power_on = 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// 0 = 0b0000
// 1 = 0b1000

static void Emit2MHz1bpp(VideoDataUnit *unit, ElectronULA *ula) {
    uint8_t byte = ula->display_byte;

    unit->pixels.pixels[0].bits = PALETTE_PIXELS[ula->palette[byte >> 4 & 8].value];
    unit->pixels.pixels[1].bits = PALETTE_PIXELS[ula->palette[byte >> 3 & 8].value];
    unit->pixels.pixels[2].bits = PALETTE_PIXELS[ula->palette[byte >> 2 & 8].value];
    unit->pixels.pixels[3].bits = PALETTE_PIXELS[ula->palette[byte >> 1 & 8].value];
    unit->pixels.pixels[4].bits = PALETTE_PIXELS[ula->palette[byte >> 0 & 8].value];
    unit->pixels.pixels[5].bits = PALETTE_PIXELS[ula->palette[byte << 1 & 8].value];
    unit->pixels.pixels[6].bits = PALETTE_PIXELS[ula->palette[byte << 2 & 8].value];
    unit->pixels.pixels[7].bits = PALETTE_PIXELS[ula->palette[byte << 3 & 8].value];
}

static void Emit1MHz1bpp(VideoDataUnit *unit, ElectronULA *ula) {
    uint8_t byte = ula->display_byte;

    ula->display_byte <<= 4;

    unit->pixels.pixels[1].bits = unit->pixels.pixels[0].bits = PALETTE_PIXELS[ula->palette[byte >> 4 & 8].value];
    unit->pixels.pixels[3].bits = unit->pixels.pixels[2].bits = PALETTE_PIXELS[ula->palette[byte >> 3 & 8].value];
    unit->pixels.pixels[5].bits = unit->pixels.pixels[4].bits = PALETTE_PIXELS[ula->palette[byte >> 2 & 8].value];
    unit->pixels.pixels[7].bits = unit->pixels.pixels[6].bits = PALETTE_PIXELS[ula->palette[byte >> 1 & 8].value];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// 0 = 0b0000
// 1 = 0b0010
// 2 = 0b1000
// 3 = 0b1010

static void Emit2MHz2bpp(VideoDataUnit *unit, ElectronULA *ula) {
    uint8_t byte = ula->display_byte;

    unit->pixels.pixels[1].bits = unit->pixels.pixels[0].bits = PALETTE_PIXELS[ula->palette[byte >> 4 & 8 | byte >> 2 & 2].value];
    unit->pixels.pixels[3].bits = unit->pixels.pixels[2].bits = PALETTE_PIXELS[ula->palette[byte >> 3 & 8 | byte >> 1 & 2].value];
    unit->pixels.pixels[5].bits = unit->pixels.pixels[4].bits = PALETTE_PIXELS[ula->palette[byte >> 2 & 8 | byte & 2].value];
    unit->pixels.pixels[7].bits = unit->pixels.pixels[6].bits = PALETTE_PIXELS[ula->palette[byte >> 1 & 8 | byte << 1 & 2].value];
}

static void Emit1MHz2bpp(VideoDataUnit *unit, ElectronULA *ula) {
    uint8_t byte = ula->display_byte;

    ula->display_byte <<= 2;

    unit->pixels.pixels[3].bits = unit->pixels.pixels[2].bits = unit->pixels.pixels[1].bits = unit->pixels.pixels[0].bits = PALETTE_PIXELS[ula->palette[byte >> 4 & 8 | byte >> 2 & 2].value];
    unit->pixels.pixels[7].bits = unit->pixels.pixels[6].bits = unit->pixels.pixels[5].bits = unit->pixels.pixels[4].bits = PALETTE_PIXELS[ula->palette[byte >> 3 & 8 | byte >> 1 & 2].value];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void Emit2MHz4bpp(VideoDataUnit *unit, ElectronULA *ula) {
    uint8_t byte = ula->display_byte;

    unit->pixels.pixels[3].bits = unit->pixels.pixels[2].bits = unit->pixels.pixels[1].bits = unit->pixels.pixels[0].bits = PALETTE_PIXELS[ula->palette[byte >> 4 & 8 | byte >> 3 & 4 | byte >> 2 & 2 | byte >> 1 & 1].value];
    unit->pixels.pixels[7].bits = unit->pixels.pixels[6].bits = unit->pixels.pixels[5].bits = unit->pixels.pixels[4].bits = PALETTE_PIXELS[ula->palette[byte >> 3 & 8 | byte >> 2 & 4 | byte >> 1 & 2 | byte & 1].value];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ElectronULA::EmitPixelsFn ElectronULA::EMIT_PIXELS_FNS[8] = {
    &Emit2MHz1bpp, //Mode 0
    &Emit2MHz2bpp, //Mode 1
    &Emit2MHz4bpp, //Mode 2
    &Emit2MHz1bpp, //Mode 3
    &Emit1MHz1bpp, //Mode 4
    &Emit1MHz2bpp, //Mode 5
    &Emit1MHz1bpp, //Mode 6
    &Emit1MHz1bpp, //TODO - check what display mode 7 actually does
};

const bool ElectronULA::IS_GRAPHICS_MODE[8] = {
    true,  //Mode 0
    true,  //Mode 1
    true,  //Mode 2
    false, //Mode 3
    true,  //Mode 4
    true,  //Mode 5
    false, //Mode 6
    true,  //Mode 7
};

const uint16_t ElectronULA::DISPLAY_WRAPAROUND_SIZES[8] = {
    20 * 1024, //Mode 0
    20 * 1024, //Mode 1
    20 * 1024, //Mode 2
    16 * 1024, //Mode 3
    10 * 1024, //Mode 4
    10 * 1024, //Mode 5
    8 * 1024,  //Mode 6
    10 * 1024, //Mode 7
};

const uint16_t ElectronULA::DISPLAY_ROW_STRIDES[8] = {
    640, //Mode 0
    640, //Mode 1
    640, //Mode 2
    640, //Mode 3
    320, //Mode 4
    320, //Mode 5
    320, //Mode 6
    320, //Mode 7
};

//const uint8_t ElectronULA::NUM_DISPLAY_ROWS[8] = {
//    32, //Mode 0
//    32, //Mode 1
//    32, //Mode 2
//    25, //Mode 3
//    32, //Mode 4
//    32, //Mode 5
//    25, //Mode 6
//    32, //Mode 7
//};

const uint8_t ElectronULA::NUM_RASTERS[2] = {
    10, //text
    8,  //graphics
};

const uint8_t ElectronULA::NUM_ROWS[2] = {
    25, //text
    32, //graphics
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
