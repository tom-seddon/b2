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

// This struct isn't yet as descriptive, nor as comprehensively used, as it
// could/should be. But this will probably improve.

struct BBCMicroType {
    // Switch-friendly identifier.
    const BBCMicroTypeID type_id;

    const M6502Config *m6502_config;

    size_t ram_buffer_size;

    DiscDriveType default_disc_drive_type;

    uint32_t dpo_mask;

    // indexed by big page index. These don't vary too much from model to
    // model - the tables are mainly separate so that B+ ANDY and M128
    // ANDY/HAZEL are correctly catogorized.
    std::vector<const BigPageType *> big_page_types;

    void (*get_mem_big_page_tables_fn)(uint8_t *usr,
                                       uint8_t *mos,
                                       uint8_t *mos_pc_mem_big_pages,
                                       bool *io,
                                       ROMSEL romsel,
                                       ACCCON acccon);

    void (*apply_dpo_fn)(ROMSEL *romsel,
                         ACCCON *acccon,
                         uint32_t dpo);
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
