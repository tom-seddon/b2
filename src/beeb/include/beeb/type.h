#ifndef HEADER_F540ED1CD8194D4CB1143D704E77037D // -*- mode:c++ -*-
#define HEADER_F540ED1CD8194D4CB1143D704E77037D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "type.inl"
#include <shared/enum_end.h>

#include <string>
#include <vector>

class Log;
struct BigPageType;
struct M6502Config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Various bits and pieces to make the difference between BBC types somewhat
// data-driven. Trying to avoid BBCMicroTypeID-specific cases if possible.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Total max addressable memory on the BBC is 468K:
//
// - 64K RAM (main+shadow+ANDY+HAZEL)
// - 256K ROM (16 * 16K)
// - 16K MOS
// - 64K int Tube
// - 2K int Tube ROM
// - 64K ext Tube
// - 2K ext Tube ROM
//
// The paging generally operates at a 4K resolution, so this can be divided into
// 84 4K pages, or (to pick a term) big pages. (1 big page = 16 pages.) The big
// pages are assigned like this:
//
// <pre>
// 0-7     main                     }
// 8       ANDY (M128)/ANDY (B+)    } "RAM"
// 9,10    HAZEL (M128)/ANDY (B+)   }
// 11-15   shadow RAM (M128/B+)     }
//
// 16-19   ROM 0                    }
// 20-23   ROM 1                    } "ROM"
// ...                              }
// 76-79   ROM 15                   }
//
// 80-83   MOS
// 84-99   parasite
// 100     parasite ROM
// 101-116 second parasite [planned]
// 117     second parasite ROM (double up to make 4K) [planned]
// </pre>
//
// (Three additional pages, for FRED/JIM/SHEILA, are planned.)
//
// Big pages 0-15 map directly to the BBCMicro RAM buffer, with each one being
// at offset index<<12. (When the RAM buffer is 32K, big pages 8-15 aren't
// mapped.)
//
// Each big page can be set up once, when the BBCMicro is first created,
// simplifying some of the logic. When switching to ROM 1, for example, the
// buffers can be found by looking at big pages 20-23, rather than having to
// check m_state.sideways_rom_buffers[1] (etc.).
//
// The per-big page struct can also contain some cold info (debug flags, static
// data, etc.), as it's only fetched when the paging registers are changed,
// rather than for every instruction.
//
// The BBC memory map is also divided into big pages, so things match up - the
// terminology is a bit slack but usually a 'big page' refers to one of the big
// pages in the list above, and a 'memory/mem big page' refers to a big page in
// the 6502 address space.
//
// "Second parasite" only applies to the Master, when there's both internal and
// external second processors connected. The external one counts as the second
// one.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Address prefixes:
//
// 0 - sideways ROM 0
// 1 - sideways ROM 1
// 2 - sideways ROM 2
// 3 - sideways ROM 3
// 4 - sideways ROM 4
// 5 - sideways ROM 5
// 6 - sideways ROM 6
// 7 - sideways ROM 7
// 8 - sideways ROM 8
// 9 - sideways ROM 9
// a - sideways ROM a
// b - sideways ROM b
// c - sideways ROM c
// d - sideways ROM d
// e - sideways ROM e
// f - sideways ROM f
// g -
// h - HAZEL
// i - I/O
// j -
// k -
// l -
// m - main RAM
// n - ANDY
// o - OS ROM
// p - parasite RAM
// q -
// r - parasite boot ROM
// s - shadow RAM
// t -
// u -
// v -
// w -
// x -
// y -
// z -

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// See comment for (very similar) CycleCount struct.
struct BigPageIndex {
    typedef uint8_t Type;
    Type i;
};

static constexpr BigPageIndex MAIN_BIG_PAGE_INDEX = {0};
static constexpr BigPageIndex::Type NUM_MAIN_BIG_PAGES = {32 / 4};

static constexpr BigPageIndex ANDY_BIG_PAGE_INDEX = {MAIN_BIG_PAGE_INDEX.i + NUM_MAIN_BIG_PAGES};
static constexpr BigPageIndex::Type NUM_ANDY_BIG_PAGES = {4 / 4};

static constexpr BigPageIndex HAZEL_BIG_PAGE_INDEX = {ANDY_BIG_PAGE_INDEX.i + NUM_ANDY_BIG_PAGES};
static constexpr BigPageIndex::Type NUM_HAZEL_BIG_PAGES = {8 / 4};

static constexpr BigPageIndex BPLUS_RAM_BIG_PAGE_INDEX = {ANDY_BIG_PAGE_INDEX.i};
static constexpr BigPageIndex::Type NUM_BPLUS_RAM_BIG_PAGES = {12 / 4};

static constexpr BigPageIndex SHADOW_BIG_PAGE_INDEX = {HAZEL_BIG_PAGE_INDEX.i + NUM_HAZEL_BIG_PAGES};
static constexpr BigPageIndex::Type NUM_SHADOW_BIG_PAGES = {20 / 4};

static constexpr BigPageIndex ROM0_BIG_PAGE_INDEX = {SHADOW_BIG_PAGE_INDEX.i + NUM_SHADOW_BIG_PAGES};
static constexpr BigPageIndex::Type NUM_ROM_BIG_PAGES = {16 / 4};

static constexpr BigPageIndex MOS_BIG_PAGE_INDEX = {ROM0_BIG_PAGE_INDEX.i + 16 * NUM_ROM_BIG_PAGES};
static constexpr BigPageIndex::Type NUM_MOS_BIG_PAGES = {16 / 4};

static constexpr BigPageIndex PARASITE_BIG_PAGE_INDEX = {MOS_BIG_PAGE_INDEX.i + NUM_MOS_BIG_PAGES};
static constexpr BigPageIndex::Type NUM_PARASITE_BIG_PAGES = {64 / 4};

static constexpr BigPageIndex PARASITE_ROM_BIG_PAGE_INDEX = {PARASITE_BIG_PAGE_INDEX.i + NUM_PARASITE_BIG_PAGES};
static constexpr BigPageIndex::Type NUM_PARASITE_ROM_BIG_PAGES = {1};

//static constexpr uint8_t SECOND_PARASITE_BIG_PAGE_INDEX = {PARASITE_ROM_BIG_PAGE_INDEX.i + NUM_PARASITE_ROM_BIG_PAGES.i};
//static constexpr uint8_t NUM_SECOND_PARASITE_BIG_PAGES = {64 / 4};
//
//static constexpr uint8_t SECOND_PARASITE_ROM_BIG_PAGE_INDEX = {SECOND_PARASITE_BIG_PAGE_INDEX.i + NUM_SECOND_PARASITE_BIG_PAGES.i};
//static constexpr uint8_t NUM_SECOND_PARASITE_ROM_BIG_PAGES = {1};

static constexpr BigPageIndex::Type NUM_BIG_PAGES = MOS_BIG_PAGE_INDEX.i + NUM_MOS_BIG_PAGES + NUM_PARASITE_BIG_PAGES + NUM_PARASITE_ROM_BIG_PAGES;

// A few big page indexes from NUM_BIG_PAGES onwards will never be valid, so
// they can be used for other purposes.
static_assert(NUM_BIG_PAGES <= 0xf0, "too many big pages");

static constexpr BigPageIndex INVALID_BIG_PAGE_INDEX = {0xff};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BROMSELBits {
    uint8_t pr : 4, _ : 4;
};

struct BPlusROMSELBits {
    uint8_t pr : 4, _ : 3, ram : 1;
};

struct Master128ROMSELBits {
    uint8_t pm : 4, _ : 3, ram : 1;
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
    uint8_t _ : 7, shadow : 1;
};

struct Master128ACCCONBits {
    uint8_t d : 1, e : 1, x : 1, y : 1, itu : 1, ifj : 1, tst : 1, irr : 1;
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
    BigPageIndex mem_big_pages[2][16];

    // [i] is 0 if memory big page i counts as user code, or 1 if it counts as
    // MOS code. Can use as index into mem_big_pages, hence the name.
    uint8_t pc_mem_big_pages_set[16];
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO think of a better name for this!
struct BigPageMetadata {
    // index of this big page.
    BigPageIndex index = INVALID_BIG_PAGE_INDEX;

    // Single char syntax for use when entering addresses in the debugger.
    char code = 0;

    // More elaborate description, printed in UI.
    std::string description;

#if BBCMICRO_DEBUGGER
    // dso mask/value that must be applied to have this big page mapped in.
    uint32_t dso_mask = ~(uint32_t)0;
    uint32_t dso_value = 0;
#endif

    // where this big page will appear in the address space when mapped in.
    uint16_t addr = 0xffff;

    // Set if this big page is in the parasite address space.
    //
    // (This mechanism could be tidier. But it should hang together for now...)
    bool is_parasite = false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BBCMicroType {
    // Switch-friendly identifier.
    const BBCMicroTypeID type_id;

    // Display name suitable for UI or whatever.
    const char *model_name;

    const M6502Config *m6502_config;

    size_t ram_buffer_size;

    DiscDriveType default_disc_drive_type;

#if BBCMICRO_DEBUGGER
    uint32_t dso_mask;
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
    // mos_pc_mem_big_pages[i] is 0 if memory big page i counts as user code, or
    // 1 if it counts as MOS code. This is indexed by the program counter to
    // figure out whether to use the usr or mos table.
    //
    // *paging_flags is set to a combination of PagingFlags bits.
    //
    // (The naming of these isn't the best.)
    void (*get_mem_big_page_tables_fn)(MemoryBigPageTables *tables,
                                       uint32_t *paging_flags,
                                       ROMSEL romsel,
                                       ACCCON acccon);

#if BBCMICRO_DEBUGGER
    void (*apply_dso_fn)(ROMSEL *romsel,
                         ACCCON *acccon,
                         uint32_t dso);
#endif

#if BBCMICRO_DEBUGGER
    uint32_t (*get_dso_fn)(ROMSEL romsel,
                           ACCCON accon);
#endif

    // Mask for ROMSEL bits.
    uint8_t romsel_mask;

    // Mask for ACCCON bits. If 0x00, the system has no ACCCON register.
    uint8_t acccon_mask;

    struct SHEILACycleStretchRegion {
        uint8_t first, last; //both inclusive
    };
    std::vector<SHEILACycleStretchRegion> sheila_cycle_stretch_regions;

    // Where the ADC lives, and how many addresses it occupies.
    uint16_t adc_addr = 0;
    uint16_t adc_count = 0;

#if BBCMICRO_DEBUGGER
    bool (*parse_prefix_lower_case_char_fn)(uint32_t *dso, char c);
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const BBCMicroType BBC_MICRO_TYPE_B;
extern const BBCMicroType BBC_MICRO_TYPE_B_PLUS;
extern const BBCMicroType BBC_MICRO_TYPE_MASTER_128;
extern const BBCMicroType BBC_MICRO_TYPE_MASTER_COMPACT;

// returns a pointer to one of the global BBCMicroType objects.
const BBCMicroType *GetBBCMicroTypeForTypeID(BBCMicroTypeID type_id);

// a few per-type ID fixed properties.
bool HasNVRAM(const BBCMicroType *type);
bool CanDisplayTeletextAt3C00(const BBCMicroType *type);
bool HasNumericKeypad(const BBCMicroType *type);
bool HasSpeech(const BBCMicroType *type);
bool HasTube(const BBCMicroType *type);
bool HasCartridges(const BBCMicroType *type);
bool HasUserPort(const BBCMicroType *type);
bool Has1MHzBus(const BBCMicroType *type);
bool HasADC(const BBCMicroType *type);
bool HasIndependentMOSView(const BBCMicroType *type);

#if BBCMICRO_DEBUGGER
// Parse address prefix and add additional flags to *dso_ptr.
//
// Returns true if OK. Any bits set or cleared in *dso_ptr will have their
// corresponding DebugStateOverride_OverrideXXX flag set too.
//
// Returns false if not ok (*dso_ptr unmodified), and prints error messages on
// *log if not NULL.
bool ParseAddressPrefix(uint32_t *dso_ptr,
                        const BBCMicroType *type,
                        const char *prefix_begin,
                        const char *prefix_end,
                        Log *log);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
