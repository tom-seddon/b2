#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/MC6850.h>
#include <beeb/6502.h>

#include <shared/enum_def.h>
#include <beeb/MC6850.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool Is7Bit(MC6850WordSelect value) {
    return !!(value & 4);
}

static const MC6850Parity PARITY_BY_WORD_SELECT[8] = {
    MC6850Parity_Even,
    MC6850Parity_Odd,
    MC6850Parity_Even,
    MC6850Parity_Odd,
    MC6850Parity_None,
    MC6850Parity_None,
    MC6850Parity_Even,
    MC6850Parity_Odd,
};

static const bool ONE_STOP_BIT_BY_WORD_SELECT[8] = {
    false,
    false,
    true,
    true,
    false,
    true,
    true,
    true,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC6850::WriteDataRegister(void *mc6850_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->m_tdr = value;
    mc6850->m_tx_tdre = 0;
    mc6850->irq.bits.tx = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t MC6850::ReadDataRegister(void *mc6850_, M6502Word addr) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->m_status.bits.rdrf = 0;
    mc6850->m_status.bits.ovrn = 0;
    mc6850->irq.bits.rx = 0;
    mc6850->m_status.bits.not_dcd = mc6850->m_not_dcd;

    return mc6850->m_rdr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC6850::WriteControlRegister(void *mc6850_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->m_control.value = value;

    switch (mc6850->m_control.bits.counter_divide_select) {
    case MC6850CounterDivideSelect_1:
        mc6850->m_clock_mask = 0;
        break;

    case MC6850CounterDivideSelect_16:
        mc6850->m_clock_mask = 15;
        break;

    case MC6850CounterDivideSelect_64:
        mc6850->m_clock_mask = 63;
        break;

    case MC6850CounterDivideSelect_MasterReset:
        mc6850->Reset();
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t MC6850::ReadStatusRegister(void *mc6850_, M6502Word addr) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    StatusRegister status = mc6850->GetStatusRegister();
    return status.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class StateType>
static inline void UpdateDataTransferMask(uint8_t *mask, StateType *state, MC6850::ControlRegister control, StateType parity_state, StateType no_parity_state) {
    *mask <<= 1;

    if (*mask == 0 ||
        (*mask == 0x80 && Is7Bit(control.bits.word_select))) {
        // Reset mask for stop bits transmission.
        *mask = 1;

        if (PARITY_BY_WORD_SELECT[control.bits.word_select] == MC6850Parity_None) {
            *state = no_parity_state;
        } else {
            *state = parity_state;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class StateType>
static inline bool UpdateStopBitsTransferMask(uint8_t *mask, StateType *state, MC6850::ControlRegister control, StateType next_state) {
    *mask <<= 1;

    if (*mask == 4 ||
        (*mask == 2 && ONE_STOP_BIT_BY_WORD_SELECT[control.bits.word_select])) {
        *state = next_state;
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC6850::UpdateReceive(uint8_t bit) {
    // "The Rx CLK must be running for proper /DCD operation" - so presumably
    // the pin is polled as part of the receive processing?
    if (m_control.bits.rx_irq_en) {
        if (!m_old_not_dcd && m_not_dcd) {
            this->irq.bits.rx = 1;
            m_old_not_dcd = m_not_dcd;
            m_status.bits.not_dcd = 1;
        }
    }

    if ((m_rx_clock++ & m_clock_mask) == 0) {
        switch (m_rx_state) {
        default:
            ASSERT(false);
            break;

        case MC6850ReceiveState_Idle:
            break;

        case MC6850ReceiveState_StartBit:
            if (!bit) {
                m_rx_state = MC6850ReceiveState_DataBits;
                m_rx_mask = 1;
                m_rx_data = 0;
                m_rx_parity = 0;
                m_rx_parity_error = false;
                m_rx_framing_error = false;
            }
            break;

        case MC6850ReceiveState_DataBits:
            if (bit) {
                m_rx_data |= m_rx_mask;
                m_rx_parity ^= 1;
            }

            UpdateDataTransferMask(&m_rx_mask, &m_rx_state, m_control, MC6850ReceiveState_ParityBit, MC6850ReceiveState_StopBits);

            break;

        case MC6850ReceiveState_ParityBit:
            {
                ASSERT(m_rx_parity == 0 || m_rx_parity == 1);

                if (PARITY_BY_WORD_SELECT[m_control.bits.word_select] == MC6850Parity_Even) {
                    m_rx_parity ^= 1;
                }

                if (m_rx_parity != (bit != 0)) {
                    m_rx_parity_error = true;
                }

                m_rx_state = MC6850ReceiveState_StopBits;
                m_rx_mask = 1;
            }
            break;

        case MC6850ReceiveState_StopBits:
            {
                if (!bit) {
                    m_rx_framing_error = true;
                }

                if (UpdateStopBitsTransferMask(&m_rx_mask, &m_rx_state, m_control, MC6850ReceiveState_StartBit)) {
                    if (m_status.bits.rdrf) {
                        m_status.bits.ovrn = 1;
                    } else {
                        m_status.bits.rdrf = 1;
                        m_rdr = m_rx_data;
                    }

                    this->irq.bits.rx = m_control.bits.rx_irq_en;
                }
            }
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MC6850::TransmitResult MC6850::UpdateTransmit() {
    if ((m_tx_clock++ & m_clock_mask) == 0) {
        switch (m_tx_state) {
        default:
            ASSERT(false);
            break;

        case MC6850TransmitState_Idle:
            if (m_tx_tdre) {
                break;
            }

            m_tx_data = m_tdr;
            m_tx_state = MC6850TransmitState_StartBit;
            m_tx_tdre = 1;
            [[fallthrough]];
        case MC6850TransmitState_StartBit:
            m_tx_state = MC6850TransmitState_DataBits;
            m_tx_mask = 1;
            m_tx_parity = 0;
            return {0, MC6850BitType_Start};

        case MC6850TransmitState_DataBits:
            {
                TransmitResult result = {!!(m_tx_data & m_tx_mask), MC6850BitType_Data};

                m_tx_parity ^= result.bit;

                UpdateDataTransferMask(&m_tx_mask, &m_tx_state, m_control, MC6850TransmitState_ParityBit, MC6850TransmitState_StopBits);

                return result;
            }

        case MC6850TransmitState_ParityBit:
            {
                ASSERT(m_tx_parity == 0 || m_tx_parity == 1);

                // In theory, you could get here with the control register
                // indicating no parity. I wonder what the actual ACIA does.
                if (PARITY_BY_WORD_SELECT[m_control.bits.word_select] == MC6850Parity_Even) {
                    m_tx_parity ^= 1;
                }

                m_tx_mask = 1;
                m_tx_state = MC6850TransmitState_StopBits;

                return {m_tx_parity, MC6850BitType_Parity};
            }

        case MC6850TransmitState_StopBits:
            {
                if (UpdateStopBitsTransferMask(&m_tx_mask, &m_tx_state, m_control, MC6850TransmitState_Idle)) {
                    this->irq.bits.tx = m_control.bits.transmitter_control == MC6850TransmitterControl_RTS0_TxIRQ;
                }
                return {1, MC6850BitType_Stop};
            }
        }
    }

    // nothing doing.
    return {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC6850::SetNotDCD(bool not_dcd) {
    m_not_dcd = not_dcd;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void MC6850::SetTrace(Trace *t) {
    m_trace = t;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC6850::Reset() {
    m_tx_state = MC6850TransmitState_Idle;
    m_tx_tdre = 1;

    m_rx_state = MC6850ReceiveState_Idle;

    m_status.value = 0;

    m_status.bits.not_dcd = m_not_dcd;
    m_status.bits.not_cts = m_not_cts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MC6850::StatusRegister MC6850::GetStatusRegister() const {
    MC6850::StatusRegister status = m_status;

    if (m_control.bits.counter_divide_select != MC6850CounterDivideSelect_MasterReset) {
        status.bits.tdre = m_tx_tdre && m_not_cts;
        status.bits.irq = this->irq.value != 0;
    }

    return status;
}
