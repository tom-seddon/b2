#ifndef HEADER_368C22BFEF7047F68704FB0A14415EB1 // -*- mode:c++ -*-
#define HEADER_368C22BFEF7047F68704FB0A14415EB1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "ElectronULA.inl"
#include <shared/enum_end.h>

struct VideoDataUnit;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ElectronPagingBits {
    uint8_t rom : 4;
    uint8_t clear_display_end : 1;
    uint8_t clear_rtc : 1;
    uint8_t clear_high_tone : 1;
    uint8_t clear_nmi : 1;
};

union ElectronPaging {
    uint8_t value;
    ElectronPagingBits bits;
};
CHECK_SIZEOF(ElectronPaging, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ElectronIRQFlagBits {
    uint8_t _ : 2;
    uint8_t flags : 5;
    uint8_t nc : 1;
};

struct ElectronIRQBits {
    uint8_t master : 1;
    uint8_t power_on : 1;
    uint8_t display_end : 1;
    uint8_t rtc : 1;
    uint8_t rx_data_full : 1;
    uint8_t tx_data_empty : 1;
    uint8_t high_tone : 1;
    uint8_t nc : 1;
};

union ElectronIRQ {
    uint8_t value;
    ElectronIRQBits bits;
    ElectronIRQFlagBits flag_bits;
};
CHECK_SIZEOF(ElectronIRQ, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ElectronULAMiscBits {
    uint8_t _ : 1;
    ElectronULAMiscMode mode : 2;
    uint8_t display_mode : 3;
    uint8_t motor : 1;
    uint8_t caps_lock : 1;
};

union ElectronULAMisc {
    uint8_t value;
    ElectronULAMiscBits bits;
};
CHECK_SIZEOF(ElectronULAMisc, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ElectronPaletteEntryBits {
    uint8_t r : 1;
    uint8_t g : 1;
    uint8_t b : 1;
};

union ElectronPaletteEntry {
    uint8_t value;
    ElectronPaletteEntryBits bits;
};
CHECK_SIZEOF(ElectronPaletteEntry, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ElectronULA {
    // 4 of the ROM banks get special handling: 2 map to the keyboard, and 2 map to
    // the onboard ROM.
    //
    // ELECTRON_KEYBOARD_ROM_BANK_BASE+0 is considered to be the keyboard's bank,
    // and b2's policy is to treat ELECTRON_KEYBOARD_ROM_BANK_BASE+1 as the alias.
    // See https://www.stardot.org.uk/forums/viewtopic.php?p=461044#p461044
    //
    // ELECTRON_BASIC_ROM_BANK_BASE+1 is considered to be BASIC's bank, and b2's
    // policy is to treat ELECTRON_BASIC_ROM_BANK_BASE+0 as the alias. The rationale
    // is that this matches the OS behaviour (as it scans the ROMs high to low, and
    // so finds +1 first and treats +0 as a duplicate to be ignored.)

    static constexpr uint8_t KEYBOARD_ROM_BANK_BASE = 8;
    static constexpr uint8_t BASIC_ROM_BANK_BASE = 10;

    // All values counted in 2 MHz units, so double the value from the EAUG.
    static constexpr uint8_t NUM_HSYNC_COLUMNS = 8;
    static constexpr uint8_t NUM_BACK_PORCH_COLUMNS = 24;
    static constexpr uint8_t NUM_DISPLAY_COLUMNS = 80;
    static constexpr uint8_t NUM_FRONT_PORCH_COLUMNS = 16;

    static constexpr uint16_t VSYNC_TIME = 2 * 160;
    static constexpr uint16_t VSYNC_SCANLINE = 281;

    // Must add up to 64 microseconds
    static_assert(NUM_DISPLAY_COLUMNS + NUM_BACK_PORCH_COLUMNS + NUM_HSYNC_COLUMNS + NUM_FRONT_PORCH_COLUMNS == 128);

    ElectronIRQ irq = {};
    ElectronIRQ irq_mask = {};
    uint8_t romsel = 0;
    bool nmi = false;

    uint16_t display_start_address = 0;
    ElectronULAMisc misc = {};
    uint8_t sound_frequency = 0;

    // 2-colour modes use entries 0/8; 4-colour modes use entries 0/2/8/10.
    ElectronPaletteEntry palette[16] = {};

    //
    ElectronULADisplayState display_state = ElectronULADisplayState_Display;

    // Storage for last byte fetch from RAM for upcoming display output
    // purposes. Potentially modified while pixels are being emitted.
    uint8_t display_byte = 0;

#if VIDEO_TRACK_METADATA
    // Video metadata stuff. Only modified when byte is fetched from RAM.
    uint8_t display_fetched_byte = 0;
    uint16_t display_byte_address = 0;
#endif

    //
    uint16_t display_row_address = 0;
    uint16_t display_fetch_address = 0;

    // vsync-related counter - either for start of the next vsync, or its
    // duration.
    uint16_t display_vsync_counter = 0;

    // As per the 6845: the row is the character row, and the raster the
    // scanline within that row. Tracked for the Display state only.
    uint16_t display_row = 0;
    uint16_t display_raster = 0;

    // Display column counter.
    uint8_t display_column = 0;

    // Scanline 0 is the first visible one.
    uint16_t display_scanline = 0;

    // Counter for the next RTC interrupt.
    uint16_t rtc_interrupt_timer = 0;

    // Even or odd field.
    bool display_even_field = false;

    // sets the power-on bit to 1.
    ElectronULA();

    typedef void (*EmitPixelsFn)(VideoDataUnit *, ElectronULA *);

    // Display-related tables, indexed by display mode.
    static const EmitPixelsFn EMIT_PIXELS_FNS[8];
    static const bool IS_GRAPHICS_MODE[8];
    static const uint16_t DISPLAY_ROW_STRIDES[8];
    static const uint16_t DISPLAY_START_VALUES[8];

    // Display-related tables, indexed by graphics mode flag.
    static const uint8_t NUM_RASTERS[2];
    static const uint8_t NUM_ROWS[2];
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
