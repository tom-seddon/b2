#ifndef HEADER_1F156F4783FB45E4A3E3A2C50515C8D1// -*- mode:c++ -*-
#define HEADER_1F156F4783FB45E4A3E3A2C50515C8D1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include "type.h"

#include <shared/enum_decl.h>
#include "paging.inl"
#include <shared/enum_end.h>

#include <string>

union M6502Word;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Grab-bag of random paging-related bits.

//
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
// (On the B, big pages 8-15 are the same as big pages 0-7.)
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

struct BigPageType {
    // Single char syntax for use when entering addresses in the debugger.
    char code=0;

    // More elaborate description, printed in UI.
    std::string description;
};

extern const BigPageType MAIN_RAM_BIG_PAGE_TYPE;
extern const BigPageType ANDY_BIG_PAGE_TYPE;
extern const BigPageType HAZEL_BIG_PAGE_TYPE;
extern const BigPageType ROM_BIG_PAGE_TYPES[16];
extern const BigPageType SHADOW_RAM_BIG_PAGE_TYPE;
extern const BigPageType MOS_BIG_PAGE_TYPE;

extern const BigPageType *const BIG_PAGE_TYPES[NUM_BIG_PAGES];

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

// YXE  Usr  MOS
// ---  ---  ---
// 000   M    M
// 001   M    S
// 010   S    M
// 011   S    S
// 100   M    M
// 101   M    M
// 110   S    S
// 111   S    S
//
// Usr Shadow = E
//
// MOS Shadow = (Y AND X) OR (NOT Y AND E)

static inline bool DoesMOSUseShadow(Master128ACCCONBits acccon_m128_bits) {
    if(acccon_m128_bits.y) {
        return acccon_m128_bits.x;
    } else {
        return acccon_m128_bits.e;
    }
}

union ACCCON {
    uint8_t value;
    BPlusACCCONBits bplus_bits;
    Master128ACCCONBits m128_bits;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Helper for figuring out what big page codes to display in debugger and trace.
class PagingAccess {
public:
    // Not very useful...
    PagingAccess()=default;

    explicit PagingAccess(const BBCMicroType *type,
                          ROMSEL romsel,
                          ACCCON acccon);

    void SetROMSEL(ROMSEL romsel);
    void SetACCCON(ACCCON acccon);
    void SetDebugPageOverrides(uint32_t dpo);

    const BigPageType *GetBigPageTypeForAccess(M6502Word pc,M6502Word addr) const;
protected:
private:
    const BBCMicroType *m_type=nullptr;
    uint32_t m_dpo=0;
    ROMSEL m_romsel={};
    ACCCON m_acccon={};

    // m_big_pages[0] is the big pages for use when user code is accessing
    // memory, m_big_pages[1] for when MOS code is accessing memory. Index by
    // top 4 bits of address.
    mutable const BigPageType *m_big_pages[2][16]={};

    // Index by top 4 bits of PC. Each entry is 0 (code in this big page counts
    // as user code) or 1 (code in this big page counts as MOS code) - use to
    // index into m_big_pages.
    mutable uint8_t m_pc_big_pages[16]={};

    // true if $fc00...$feff is I/O area.
    mutable bool m_io=true;

    mutable bool m_dirty=true;
    
    void Update() const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
