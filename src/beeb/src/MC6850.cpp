#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/MC6850.h>
#include <beeb/6502.h>

#include <shared/enum_def.h>
#include <beeb/MC6850.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteMC6850DataRegister(void *mc6850_, M6502Word addr, uint8_t value) {
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->tdr = value;
    mc6850->status.bits.tdre = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t ReadMC6850DataRegister(void *mc6850, M6502Word addr) {
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteMC6850ControlRegister(void *mc6850_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    mc6850->control.value = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t ReadMC6850StatusRegister(void *mc6850_, M6502Word addr) {
    (void)addr;
    auto mc6850 = (MC6850 *)mc6850_;

    return mc6850->status.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Start bit, data bits, parity bit, stop bits.

static const uint8_t POPCNT[16] = {
    0, //%0000
    1, //%0001
    1, //%0010
    2, //%0011
    1, //%0100
    2, //%0101
    2, //%0110
    3, //%0111
    1, //%1000
    2, //%1001
    2, //%1010
    3, //%1011
    2, //%1100
    3, //%1101
    3, //%1110
    4, //%1111
};

template <unsigned NUM_DATA_BITS, unsigned NUM_STOP_BITS>
struct TransmitDataBitsWithParity {
    uint16_t start : 1;
    uint16_t data : NUM_DATA_BITS;
    uint16_t parity : 1;
    uint16_t stop : NUM_STOP_BITS;
};

template <unsigned NUM_DATA_BITS, unsigned NUM_STOP_BITS>
struct TransmitDataBitsWithoutParity {
    uint16_t start : 1;
    uint16_t data : NUM_DATA_BITS;
    uint16_t stop : NUM_STOP_BITS;
};

template <uint8_t NUM_DATA_BITS, MC6850Parity PARITY, uint8_t NUM_STOP_BITS>
static uint16_t GetTransmitData(uint8_t data) {
    static_assert(NUM_STOP_BITS == 1 || NUM_STOP_BITS == 2);
    static_assert(NUM_DATA_BITS == 7 || NUM_DATA_BITS == 8);
    static_assert(PARITY == MC6850Parity_None || PARITY == MC6850Parity_Odd || PARITY == MC6850Parity_Even);

    static constexpr uint16_t STOP_BITS_VALUE = (1 << NUM_STOP_BITS) - 1;

    if constexpr (PARITY == MC6850Parity_None) {
        union {
            TransmitDataBitsWithoutParity<NUM_DATA_BITS, NUM_STOP_BITS> bits;
            uint16_t value;
        } u;
        static_assert(sizeof u == 2);
        u.value = 0;
        u.bits.start = 1;
        u.bits.data = data;
        u.bits.stop = STOP_BITS_VALUE;
        return u.value;
    } else {
        union {
            TransmitDataBitsWithParity<NUM_DATA_BITS, NUM_STOP_BITS> bits;
            uint16_t value;
        } u;
        static_assert(sizeof u == 2);
        u.value = 0;
        u.bits.start = 1;
        u.bits.data = data;
        if constexpr (NUM_DATA_BITS == 7) {
            u.bits.parity = POPCNT[data >> 4 & 7] + POPCNT[data & 0xf];
        } else {
            u.bits.parity = POPCNT[data >> 4] + POPCNT[data & 0xf];
        }
        if constexpr (PARITY == MC6850Parity_Even) {
            u.bits.parity ^= 1;
        }
        u.bits.stop = STOP_BITS_VALUE;
        return u.value;
    }
}

bool UpdateMC6850Transmit(MC6850 *mc6850) {
    switch (mc6850->transmit_state) {
    default:
        ASSERT(false);
        break;

    case MC6850TransmitState_Idle:
        if (mc6850->status.bits.tdre) {
            break;
        }

        switch (mc6850->control.bits.word_select) {
        default:
            ASSERT(false);
            [[fallthrough]];
        case MC6850WordSelect_7e2:
            mc6850->transmit_data = GetTransmitData<7, MC6850Parity_Even, 2>(mc6850->tdr);
            break;

        case MC6850WordSelect_7o2:
            mc6850->transmit_data = GetTransmitData<7, MC6850Parity_Odd, 2>(mc6850->tdr);
            break;

        case MC6850WordSelect_7e1:
            mc6850->transmit_data = GetTransmitData<7, MC6850Parity_Even, 1>(mc6850->tdr);
            break;

        case MC6850WordSelect_7o1:
            mc6850->transmit_data = GetTransmitData<7, MC6850Parity_Odd, 1>(mc6850->tdr);
            break;

        case MC6850WordSelect_8_2:
            mc6850->transmit_data = GetTransmitData<8, MC6850Parity_None, 2>(mc6850->tdr);
            break;

        case MC6850WordSelect_8_1:
            mc6850->transmit_data = GetTransmitData<8, MC6850Parity_None, 1>(mc6850->tdr);
            break;

        case MC6850WordSelect_8e1:
            mc6850->transmit_data = GetTransmitData<8, MC6850Parity_Even, 1>(mc6850->tdr);
            break;

        case MC6850WordSelect_8o1:
            mc6850->transmit_data = GetTransmitData<8, MC6850Parity_Odd, 1>(mc6850->tdr);
            break;
        }

        mc6850->transmit_state = MC6850TransmitState_Bits;
        [[fallthrough]];
    case MC6850TransmitState_Bits:
        {
            bool bit = (mc6850->transmit_data & 1) != 0;

            mc6850->transmit_data >>= 1;

            if (mc6850->transmit_data == 0) {
                mc6850->transmit_state = MC6850TransmitState_Idle;
            }

            return bit;
        }
        break;
    }

    // nothing doing.
    return false;
}
