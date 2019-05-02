#ifndef HEADER_F540ED1CD8194D4CB1143D704E77037D// -*- mode:c++ -*-
#define HEADER_F540ED1CD8194D4CB1143D704E77037D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "type.inl"
#include <shared/enum_end.h>

#include <vector>

struct BigPageType;
struct M6502Config;
union ACCCON;
union ROMSEL;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Just whatever data is needed to avoid a few switches on BBCMicroTypeID.

struct BBCMicroType {
    // Switch-friendly identifier.
    const BBCMicroTypeID type_id;

    const M6502Config *m6502_config;

    size_t ram_buffer_size;

    DiscDriveType default_disc_drive_type;

    uint32_t dpo_mask;

    // indexed by big page index. These don't actually vary much from model to
    // model - the tables are separate mainly so that B+ ANDY and M128
    // ANDY/HAZEL are correctly catogorized.
    std::vector<const BigPageType *> big_page_types;

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
    void (*get_mem_big_page_tables_fn)(uint8_t *usr,
                                       uint8_t *mos,
                                       uint8_t *mos_pc_mem_big_pages,
                                       bool *io,
                                       bool *crt_shadow,
                                       ROMSEL romsel,
                                       ACCCON acccon);

    void (*apply_dpo_fn)(ROMSEL *romsel,
                         ACCCON *acccon,
                         uint32_t dpo);

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
