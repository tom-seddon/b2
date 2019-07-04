#ifndef HEADER_F540ED1CD8194D4CB1143D704E77037D// -*- mode:c++ -*-
#define HEADER_F540ED1CD8194D4CB1143D704E77037D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "type.inl"
#include <shared/enum_end.h>

#include <string>
#include <vector>

struct BigPageType;
struct M6502Config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Various bits and pieces to make the difference between BBC types somewhat
// data-driven. Trying to avoid BBCMicroTypeID-specific cases if possible.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Total max addressable memory on the BBC is 336K:
//
// - 64K RAM (main+shadow+ANDY+HAZEL)
// - 256K ROM (16 * 16K)
// - 16K MOS
//
// The paging operates at a 4K resolution, so this can be divided into 84
// 4K pages, or (to pick a term) big pages. (1 big page = 16 pages.) The big pages
// are assigned like this:
//
// 0-7     main
// 8       ANDY (M128)/ANDY (B+)
// 9,10    HAZEL (M128)/ANDY (B+)
// 11-15   shadow RAM (M128/B+)
// 16-19   ROM 0
// 20-23   ROM 1
// ...
// 76-79   ROM 15
// 80-83   MOS
//
// (Three additional pages, for FRED/JIM/SHEILA, are planned.)
//
// Big pages 0-15 map directly to the BBCMicro RAM buffer, with each one being
// at offset index<<12. (When the RAM buffer is 32K, big pages 8-15 aren't
// mapped.)
//
// Each big page can be set up once, when the BBCMicro is first created,
// simplifying some of the logic. When switching to ROM 1, for example,
// the buffers can be found by looking at big pages 20-23, rather than having
// to check m_state.sideways_rom_buffers[1] (etc.).
//
// The per-big page struct can also contain some cold info (debug flags, static
// data, etc.), as it's only fetched when the paging registers are changed,
// rather than for every instruction.
//
// The BBC memory map is also divided into big pages, so things match up - the
// terminology is a bit slack but usually a 'big page' refers to one of the
// big pages in the list above, and a 'memory/mem big page' refers to a big
// page in the 6502 address space.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr uint8_t MAIN_BIG_PAGE_INDEX=0;
static constexpr uint8_t NUM_MAIN_BIG_PAGES=32/4;

static constexpr uint8_t ANDY_BIG_PAGE_INDEX=MAIN_BIG_PAGE_INDEX+NUM_MAIN_BIG_PAGES;
static constexpr uint8_t NUM_ANDY_BIG_PAGES=4/4;

static constexpr uint8_t HAZEL_BIG_PAGE_INDEX=ANDY_BIG_PAGE_INDEX+NUM_ANDY_BIG_PAGES;
static constexpr uint8_t NUM_HAZEL_BIG_PAGES=8/4;

static constexpr uint8_t BPLUS_RAM_BIG_PAGE_INDEX=ANDY_BIG_PAGE_INDEX;
static constexpr uint8_t NUM_BPLUS_RAM_BIG_PAGES=12/4;

static constexpr uint8_t SHADOW_BIG_PAGE_INDEX=HAZEL_BIG_PAGE_INDEX+NUM_HAZEL_BIG_PAGES;
static constexpr uint8_t NUM_SHADOW_BIG_PAGES=20/4;

static constexpr uint8_t ROM0_BIG_PAGE_INDEX=SHADOW_BIG_PAGE_INDEX+NUM_SHADOW_BIG_PAGES;
static constexpr uint8_t NUM_ROM_BIG_PAGES=16/4;

static constexpr uint8_t MOS_BIG_PAGE_INDEX=ROM0_BIG_PAGE_INDEX+16*NUM_ROM_BIG_PAGES;
static constexpr uint8_t NUM_MOS_BIG_PAGES=16/4;

static constexpr uint8_t NUM_BIG_PAGES=MOS_BIG_PAGE_INDEX+NUM_MOS_BIG_PAGES;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BROMSELBits {
    uint8_t pr:4,_:4;
};

struct BPlusROMSELBits {
    uint8_t pr:4,_:3,ram:1;
};

struct Master128ROMSELBits {
    uint8_t pm:4,_:3,ram:1;
};

union ROMSEL {
    uint8_t value;
    BROMSELBits b_bits;
    BPlusROMSELBits bplus_bits;
    Master128ROMSELBits m128_bits;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BPlusACCCONBits {
    uint8_t _:7,shadow:1;
};

struct Master128ACCCONBits {
    uint8_t d:1,e:1,x:1,y:1,itu:1,ifj:1,tst:1,irr:1;
};

union ACCCON {
    uint8_t value;
    BPlusACCCONBits bplus_bits;
    Master128ACCCONBits m128_bits;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MemoryBigPageTables {
    // [0][i] is the big page to use when user code accesses memory big page i;
    // [1][i] likewise for MOS code.
    uint8_t mem_big_pages[2][16];

    // [i] is 0 if memory big page i counts as user code, or 1 if it counts as
    // MOS code. Can use as index into mem_big_pages, hence the name.
    uint8_t pc_mem_big_pages_set[16];
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO think of a better name for this!
struct BigPageMetadata {
    // index of this big page.
    uint8_t index=0xff;

    // Single char syntax for use when entering addresses in the debugger.
    char code=0;

    // More elaborate description, printed in UI.
    std::string description;

    // dpo mask/value that must be applied to have this big page mapped in.
    uint32_t dpo_mask=~(uint32_t)0;
    uint32_t dpo_value=0;

    // where this big page will appear in the address space when mapped in.
    uint16_t addr=0xffff;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BBCMicroType {
    // Switch-friendly identifier.
    const BBCMicroTypeID type_id;

    const M6502Config *m6502_config;

    size_t ram_buffer_size;

    DiscDriveType default_disc_drive_type;

#if BBCMICRO_DEBUGGER
    uint32_t dpo_mask;
#endif

//    // indexed by big page index. These don't actually vary much from model to
//    // model - the tables are separate mainly so that B+ ANDY and M128
//    // ANDY/HAZEL are correctly catogorized.
//    std::vector<const BigPageType *> big_page_types;

    // indexed by big page index.
    //
    // Info about where a given big page will appear in the 6502 memory map.
    //
    // If addr==0xffff, this big page isn't relevant for this model.
    std::vector<BigPageMetadata> big_pages_metadata;

    // usr, mos and mos_pc_mem_big_pages should point to 16-byte tables.
    //
    // usr[i] is the big page to use when user code accesses memory big page i,
    // and mos[i] likewise for MOS code.
    //
    // mos_pc_mem_big_pages[i] is 0 if memory big page i counts as user code,
    // or 1 if it counts as MOS code. This is indexed by the program counter
    // to figure out whether to use the usr or mos table.
    //
    // *io corresponds to the Master's TST bit - true if I/O mapped at
    // $fc00...$feff, or false if reads there access ROM.
    //
    // *crt_shadow is set if the CRT should read from shadow RAM rather than
    // main RAM.
    //
    // (The naming of these isn't the best.)
    void (*get_mem_big_page_tables_fn)(MemoryBigPageTables *tables,
                                       bool *io,
                                       bool *crt_shadow,
                                       ROMSEL romsel,
                                       ACCCON acccon);

#if BBCMICRO_DEBUGGER
    void (*apply_dpo_fn)(ROMSEL *romsel,
                         ACCCON *acccon,
                         uint32_t dpo);
#endif

#if BBCMICRO_DEBUGGER
    uint32_t (*get_dpo_fn)(ROMSEL romsel,
                           ACCCON accon);
#endif

    // Mask for ROMSEL bits.
    uint8_t romsel_mask;

    // Mask for ACCCON bits. If 0x00, the system has no ACCCON register.
    uint8_t acccon_mask;

    // combination of BBCMicroTypeFlag
    uint32_t flags;

    struct SHEILACycleStretchRegion {
        uint8_t first,last;//both inclusive
    };
    std::vector<SHEILACycleStretchRegion> sheila_cycle_stretch_regions;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const BBCMicroType BBC_MICRO_TYPE_B;
extern const BBCMicroType BBC_MICRO_TYPE_B_PLUS;
extern const BBCMicroType BBC_MICRO_TYPE_MASTER;

// returns a pointer to one of the global BBCMicroType objects.
const BBCMicroType *GetBBCMicroTypeForTypeID(BBCMicroTypeID type_id);

size_t GetNumBBCMicroTypes();
const BBCMicroType *GetBBCMicroTypeByIndex(size_t index);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
