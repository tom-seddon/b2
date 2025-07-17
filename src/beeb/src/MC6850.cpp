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

void WriteMC6850DataRegister(void *mc6850_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->tdr = value;
    mc6850->tx_tdre = 0;
    mc6850->irq.bits.tx = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t ReadMC6850DataRegister(void *mc6850_, M6502Word addr) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->status.bits.rdrf = 0;
    mc6850->status.bits.ovrn = 0;
    mc6850->irq.bits.rx = 0;
    mc6850->status.bits.not_dcd = mc6850->not_dcd;

    return mc6850->rdr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void Reset(MC6850 *mc6850) {
    mc6850->tx_state = MC6850TransmitState_Idle;
    mc6850->tx_tdre = 1;

    mc6850->rx_state = MC6850ReceiveState_Idle;

    mc6850->status.value = 0;

    mc6850->status.bits.not_dcd = mc6850->not_dcd;
    mc6850->status.bits.not_cts = mc6850->not_cts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteMC6850ControlRegister(void *mc6850_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->control.value = value;

    switch (mc6850->control.bits.counter_divide_select) {
    case MC6850CounterDivideSelect_1:
        mc6850->clock_mask = 0;
        break;

    case MC6850CounterDivideSelect_16:
        mc6850->clock_mask = 15;
        break;

    case MC6850CounterDivideSelect_64:
        mc6850->clock_mask = 63;
        break;

    case MC6850CounterDivideSelect_MasterReset:
        Reset(mc6850);
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static MC6850::StatusRegister ReadStatusRegister(const MC6850 *mc6850) {
    MC6850::StatusRegister status = mc6850->status;

    if (mc6850->control.bits.counter_divide_select != MC6850CounterDivideSelect_MasterReset) {
        status.bits.tdre = mc6850->tx_tdre && mc6850->not_cts;
        status.bits.irq = mc6850->irq.value != 0;
    }

    return status;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
MC6850::StatusRegister DebugReadMC6850StatusRegister(const MC6850 *mc6850) {
    return ReadStatusRegister(mc6850);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t ReadMC6850StatusRegister(void *mc6850_, M6502Word addr) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    MC6850::StatusRegister status = ReadStatusRegister(mc6850);
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

void UpdateMC6850Receive(MC6850 *mc6850, uint8_t bit) {
    // "The Rx CLK must be running for proper /DCD operation" - so presumably
    // the pin is polled as part of the receive processing?
    if (mc6850->control.bits.rx_irq_en) {
        if (!mc6850->old_not_dcd && mc6850->not_dcd) {
            mc6850->irq.bits.rx = 1;
            mc6850->old_not_dcd = mc6850->not_dcd;
            mc6850->status.bits.not_dcd = 1;
        }
    }

    if ((mc6850->rx_clock++ & mc6850->clock_mask) == 0) {
        switch (mc6850->rx_state) {
        default:
            ASSERT(false);
            break;

        case MC6850ReceiveState_Idle:
            break;

        case MC6850ReceiveState_StartBit:
            if (!bit) {
                mc6850->rx_state = MC6850ReceiveState_DataBits;
                mc6850->rx_mask = 1;
                mc6850->rx_data = 0;
                mc6850->rx_parity = 0;
                mc6850->rx_parity_error = false;
                mc6850->rx_framing_error = false;
            }
            break;

        case MC6850ReceiveState_DataBits:
            if (bit) {
                mc6850->rx_data |= mc6850->rx_mask;
                mc6850->rx_parity ^= 1;
            }

            UpdateDataTransferMask(&mc6850->rx_mask, &mc6850->rx_state, mc6850->control, MC6850ReceiveState_ParityBit, MC6850ReceiveState_StopBits);

            break;

        case MC6850ReceiveState_ParityBit:
            {
                ASSERT(mc6850->rx_parity == 0 || mc6850->rx_parity == 1);

                if (PARITY_BY_WORD_SELECT[mc6850->control.bits.word_select] == MC6850Parity_Even) {
                    mc6850->rx_parity ^= 1;
                }

                if (mc6850->rx_parity != (bit != 0)) {
                    mc6850->rx_parity_error = true;
                }

                mc6850->rx_state = MC6850ReceiveState_StopBits;
                mc6850->rx_mask = 1;
            }
            break;

        case MC6850ReceiveState_StopBits:
            {
                if (!bit) {
                    mc6850->rx_framing_error = true;
                }

                if (UpdateStopBitsTransferMask(&mc6850->rx_mask, &mc6850->rx_state, mc6850->control, MC6850ReceiveState_StartBit)) {
                    if (mc6850->status.bits.rdrf) {
                        mc6850->status.bits.ovrn = 1;
                    } else {
                        mc6850->status.bits.rdrf = 1;
                        mc6850->rdr = mc6850->rx_data;
                    }

                    mc6850->irq.bits.rx = mc6850->control.bits.rx_irq_en;
                }
            }
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MC6850TransmitResult UpdateMC6850Transmit(MC6850 *mc6850) {
    if ((mc6850->tx_clock++ & mc6850->clock_mask) == 0) {
        switch (mc6850->tx_state) {
        default:
            ASSERT(false);
            break;

        case MC6850TransmitState_Idle:
            if (mc6850->tx_tdre) {
                break;
            }

            mc6850->tx_data = mc6850->tdr;
            mc6850->tx_state = MC6850TransmitState_StartBit;
            mc6850->tx_tdre = 1;
            [[fallthrough]];
        case MC6850TransmitState_StartBit:
            mc6850->tx_state = MC6850TransmitState_DataBits;
            mc6850->tx_mask = 1;
            mc6850->tx_parity = 0;
            return {0, MC6850BitType_Start};

        case MC6850TransmitState_DataBits:
            {
                MC6850TransmitResult result = {!!(mc6850->tx_data & mc6850->tx_mask), MC6850BitType_Data};

                mc6850->tx_parity ^= result.bit;

                UpdateDataTransferMask(&mc6850->tx_mask, &mc6850->tx_state, mc6850->control, MC6850TransmitState_ParityBit, MC6850TransmitState_StopBits);

                return result;
            }

        case MC6850TransmitState_ParityBit:
            {
                ASSERT(mc6850->tx_parity == 0 || mc6850->tx_parity == 1);

                // In theory, you could get here with the control register
                // indicating no parity. I wonder what the actual ACIA does.
                if (PARITY_BY_WORD_SELECT[mc6850->control.bits.word_select] == MC6850Parity_Even) {
                    mc6850->tx_parity ^= 1;
                }

                mc6850->tx_mask = 1;
                mc6850->tx_state = MC6850TransmitState_StopBits;

                return {mc6850->tx_parity, MC6850BitType_Parity};
            }

        case MC6850TransmitState_StopBits:
            {
                if (UpdateStopBitsTransferMask(&mc6850->tx_mask, &mc6850->tx_state, mc6850->control, MC6850TransmitState_Idle)) {
                    mc6850->irq.bits.tx = mc6850->control.bits.transmitter_control == MC6850TransmitterControl_RTS0_TxIRQ;
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
