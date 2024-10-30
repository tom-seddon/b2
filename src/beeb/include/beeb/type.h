#ifndef HEADER_F540ED1CD8194D4CB1143D704E77037D // -*- mode:c++ -*-
#define HEADER_F540ED1CD8194D4CB1143D704E77037D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include "roms.h"
#include <string>
#include <vector>
#include <memory>

#include <shared/enum_decl.h>
#include "type.inl"
#include <shared/enum_end.h>

class Log;
struct BigPageType;
struct M6502Config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Various paging-related bits and pieces that need a bit of a tidy up.
//
// At least some of this stuff could go into BBCMicro

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

char GetROMBankCode(uint32_t bank);
char GetMapperRegionCode(uint32_t region);

// BIG_PAGE_SIZE_BYTES fits into a uint16_t.
static constexpr size_t BIG_PAGE_SIZE_BYTES = 4096;
static constexpr size_t BIG_PAGE_OFFSET_MASK = 4095;

// See comment for (very similar) CycleCount struct.
struct BigPageIndex {
    typedef uint16_t Type;
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
static constexpr BigPageIndex::Type NUM_ROM_BIG_PAGES = {128 / 4};

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
static_assert(NUM_BIG_PAGES <= (BigPageIndex::Type)~0xf, "too many big pages");

static constexpr BigPageIndex INVALID_BIG_PAGE_INDEX = {(BigPageIndex::Type) ~(BigPageIndex::Type)0};

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

struct PagingState {
    // Value of ROMSEL.
    ROMSEL romsel = {};

    // Value of ACCCON.
    ACCCON acccon = {};

    // Current ROM mapper region for each ROM.
    uint8_t rom_regions[16] = {};

    // ROM type for each ROM.
    ROMType rom_types[16] = {};
};

static_assert(sizeof(PagingState) == 34);

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

#if BBCMICRO_DEBUGGER
    // index of the debug flags for this big page.
    BigPageIndex debug_flags_index = INVALID_BIG_PAGE_INDEX;
#endif

    // Page override char(s) to display in the debugger. (At the moment, only 2
    // are required.)
    //
    // If only 1 code applies, the second char of aligned_codes is a space.
    char aligned_codes[3] = {};
    char minimal_codes[3] = {};

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
    BBCMicroTypeID type_id;

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
                                       const PagingState &paging);

#if BBCMICRO_DEBUGGER
    void (*apply_dso_fn)(PagingState *paging,
                         uint32_t dso);
#endif

#if BBCMICRO_DEBUGGER
    uint32_t (*get_dso_fn)(const PagingState &paging);
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
    bool (*parse_suffix_char_fn)(uint32_t *dso, char c);
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetROMOffset(ROMType rom_type, uint32_t relative_big_page_index, uint32_t region);

std::shared_ptr<const BBCMicroType> CreateBBCMicroType(BBCMicroTypeID type_id, const ROMType *rom_types);

// a few per-type ID fixed properties.
bool HasNVRAM(BBCMicroTypeID type_id);
bool CanDisplayTeletextAt3C00(BBCMicroTypeID type_id);
bool HasNumericKeypad(BBCMicroTypeID type_id);
bool HasSpeech(BBCMicroTypeID type_id);
bool HasTube(BBCMicroTypeID type_id);
bool HasCartridges(BBCMicroTypeID type_id);
bool HasUserPort(BBCMicroTypeID type_id);
bool Has1MHzBus(BBCMicroTypeID type_id);
bool HasADC(BBCMicroTypeID type_id);
bool HasIndependentMOSView(BBCMicroTypeID type_id);
const char *GetModelName(BBCMicroTypeID type_id);

#if BBCMICRO_DEBUGGER
// Parse address suffix and add additional flags to *dso_ptr.
//
// Returns true if OK. Any bits set or cleared in *dso_ptr will have their
// corresponding DebugStateOverride_OverrideXXX flag set too.
//
// Returns false if not ok (*dso_ptr unmodified), and prints error messages on
// *log if not NULL.
bool ParseAddressSuffix(uint32_t *dso_ptr,
                        const std::shared_ptr<const BBCMicroType> &type,
                        const char *suffix,
                        Log *log);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
