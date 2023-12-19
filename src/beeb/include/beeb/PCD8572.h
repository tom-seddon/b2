#ifndef HEADER_AAA66CF42C6646DBAF095E42C8601BC4 // -*- mode:c++ -*-
#define HEADER_AAA66CF42C6646DBAF095E42C8601BC4

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Trace;

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
};

#if BBCMICRO_TRACE
void SetPCD8572Trace(PCD8572 *p, Trace *t);
#endif
bool UpdatePCD8572(PCD8572 *p, bool clk, bool data);

//protected:
//private:
//bool m_odata = false;
//bool m_oclk = false;
//uint8_t m_ram[128] = {};
//uint8_t m_addr = 0;
//uint8_t m_value = 0;
//uint8_t m_num_bits = 0;
//}
//;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
