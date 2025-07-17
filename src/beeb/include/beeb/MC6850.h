#ifndef HEADER_CC8787197E6E4552B0F82E55608B2261 // -*- mode:c++ -*-
#define HEADER_CC8787197E6E4552B0F82E55608B2261

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#include <shared/enum_decl.h>
#include "MC6850.inl"
#include <shared/enum_end.h>

union M6502Word;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Transmit = ACIA->Device

// Receive = Device->ACIA

// TODO: there's enough logic in this that it probably actually wants to be a
// class with private stuff.

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

    struct IRQBits {
        uint8_t tx : 1;
        uint8_t rx : 1;
    };

    union IRQ {
        IRQBits bits;
        uint8_t value;
    };
    CHECK_SIZEOF(IRQ, 1);

    bool not_dcd = false;
    bool not_cts = false;
    ControlRegister control = {};
    StatusRegister status = {};
    uint8_t tdr = 0;
    uint8_t rdr = 0;
    IRQ irq = {};
    bool old_not_dcd = false;

    uint8_t rx_clock = 0;
    uint8_t rx_parity = 0;
    uint8_t rx_data = 0;
    uint8_t rx_mask = 0;
    bool rx_parity_error = false;
    bool rx_framing_error = false;
    MC6850ReceiveState rx_state = MC6850ReceiveState_Idle;

    uint8_t tx_clock = 0;
    uint8_t tx_data = 0;
    uint8_t tx_mask = 0;
    uint8_t tx_parity = 0;
    uint8_t tx_tdre = 1;
    MC6850TransmitState tx_state = MC6850TransmitState_Idle;

    uint8_t clock_mask = 0;
};

struct MC6850TransmitResult {
    uint8_t bit : 1;
    MC6850BitType type : 3;
};
CHECK_SIZEOF(MC6850TransmitResult, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteMC6850DataRegister(void *mc6850, M6502Word addr, uint8_t value);
uint8_t ReadMC6850DataRegister(void *mc6850, M6502Word addr);

void WriteMC6850ControlRegister(void *mc6850, M6502Word addr, uint8_t value);
#if BBCMICRO_DEBUGGER
MC6850::StatusRegister DebugReadMC6850StatusRegister(const MC6850 *mc6850);
#endif
uint8_t ReadMC6850StatusRegister(void *mc6850, M6502Word addr);

void UpdateMC6850Receive(MC6850 *mc6850, uint8_t bit);
MC6850TransmitResult UpdateMC6850Transmit(MC6850 *mc6850);

//void MaybeSetMC6850Idle(MC6850 *mc6850, bool has_input_source);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
