#ifndef HEADER_AAA66CF42C6646DBAF095E42C8601BC4 // -*- mode:c++ -*-
#define HEADER_AAA66CF42C6646DBAF095E42C8601BC4

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef BUILD_TYPE_Debug
// Add in some slightly ugly MOS 5.10-specific debug stuff (that gets its
// tentacles elsewhere). Deliberately only included in the cmake Debug
// configuration, as it's not general-purpose.
#define PCD8572_MOS510_DEBUG 0
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Trace;
#if PCD8572_MOS510_DEBUG
struct M6502; //temp
#endif

#include "conf.h"

#include <shared/enum_decl.h>
#include "PCD8572.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct PCD8572 {
    struct AddressBits {
        uint8_t w : 1;
        uint8_t a : 7;
    };

    union Address {
        uint8_t value;
        struct AddressBits bits;
    };

#if BBCMICRO_TRACE
    Trace *t = nullptr;
#endif

    bool oclk = false;
    bool odata = false;
    uint8_t value = 0;
    uint8_t value_mask = 0;
    bool data_output = true;

    bool read = false;
    uint8_t addr = 0;

    PCD8572State state = PCD8572State_Idle;
    PCD8572State next_state = PCD8572State_Idle;

#if PCD8572_MOS510_DEBUG
    const M6502 *cpu = nullptr;
#endif

    // "RAM"? Is that really the best name for this?
    uint8_t ram[128] = {};
};

#if BBCMICRO_TRACE
void SetPCD8572Trace(PCD8572 *p, Trace *t);
#endif
void UpdatePCD8572(PCD8572 *p, bool clk, bool data);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
