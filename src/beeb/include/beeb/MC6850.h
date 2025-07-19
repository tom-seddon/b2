#ifndef HEADER_CC8787197E6E4552B0F82E55608B2261 // -*- mode:c++ -*-
#define HEADER_CC8787197E6E4552B0F82E55608B2261

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#include <shared/enum_decl.h>
#include "MC6850.inl"
#include <shared/enum_end.h>

union M6502Word;
class Trace;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Transmit = ACIA->Device

// Receive = Device->ACIA

// TODO: there's enough logic in this that it probably actually wants to be a
// class with private stuff.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MC6850 {
  public:
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

    struct TransmitResult {
        uint8_t bit : 1;
        MC6850BitType type : 3;
    };
    CHECK_SIZEOF(TransmitResult, 1);

    IRQ irq = {};

    static void WriteDataRegister(void *mc6850, M6502Word addr, uint8_t value);
    static uint8_t ReadDataRegister(void *mc6850, M6502Word addr);

    static void WriteControlRegister(void *mc6850, M6502Word addr, uint8_t value);

    static uint8_t ReadStatusRegister(void *mc6850, M6502Word addr);

    void UpdateReceive(uint8_t bit);
    TransmitResult UpdateTransmit();

    void SetNotDCD(bool not_dcd);

#if BBCMICRO_TRACE
    void SetTrace(Trace *t);
#endif

  protected:
  private:
    bool m_not_dcd = false;
    bool m_not_cts = false;
    ControlRegister m_control = {};
    StatusRegister m_status = {};
    uint8_t m_tdr = 0;
    uint8_t m_rdr = 0;
    bool m_old_not_dcd = false;

    uint8_t m_rx_clock = 0;
    uint8_t m_rx_parity = 0;
    uint8_t m_rx_data = 0;
    uint8_t m_rx_mask = 0;
    bool m_rx_parity_error = false;
    bool m_rx_framing_error = false;
    MC6850ReceiveState m_rx_state = MC6850ReceiveState_Idle;

    uint8_t m_tx_clock = 0;
    uint8_t m_tx_data = 0;
    uint8_t m_tx_mask = 0;
    uint8_t m_tx_parity = 0;
    uint8_t m_tx_tdre = 1;
    MC6850TransmitState m_tx_state = MC6850TransmitState_Idle;

    uint8_t m_clock_mask = 0;

#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
#endif

    void Reset();
    StatusRegister GetStatusRegister() const;

    friend class SerialDebugWindow;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
