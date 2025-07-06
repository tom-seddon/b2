#ifndef HEADER_CC8787197E6E4552B0F82E55608B2261 // -*- mode:c++ -*-
#define HEADER_CC8787197E6E4552B0F82E55608B2261

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "MC6850.inl"
#include <shared/enum_end.h>

union M6502Word;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MC6850 {
    struct ControlRegisterBits {
        MC6850CounterDivideSelect counter_divide_select : 2;
        MC6850WordSelect word_select : 3;
        MC6850TransmitterControl transmitter_control : 2;
        uint8_t rx_irq_en : 1;
    };

    union ControlRegister {
        ControlRegisterBits bits;
        uint8_t value;
    };
    CHECK_SIZEOF(ControlRegister, 1);

    struct StatusRegisterBits {
        uint8_t rdrf : 1;
        uint8_t tdre : 1;
        uint8_t not_dcd : 1;
        uint8_t not_cts : 1;
        uint8_t fe : 1;
        uint8_t ovrn : 1;
        uint8_t pe : 1;
        uint8_t irq : 1;
    };

    union StatusRegister {
        StatusRegisterBits bits;
        uint8_t value;
    };
    CHECK_SIZEOF(StatusRegister, 1);

    ControlRegister control = {};
    StatusRegister status = {};
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteMC6850DataRegister(void *mc6850, M6502Word addr, uint8_t value);
uint8_t ReadMC6850DataRegister(void *mc6850, M6502Word addr);

void WriteMC6850ControlRegister(void *mc6850, M6502Word addr, uint8_t value);
uint8_t ReadMC6850StatusRegister(void *mc6850, M6502Word addr);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
