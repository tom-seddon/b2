#include <shared/system.h>
#include <beeb/serproc.h>
#include <beeb/6502.h>
#include <beeb/MC6850.h>
#include <beeb/conf.h>

#include <shared/enum_def.h>
#include <beeb/serproc.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static_assert(CYCLES_PER_SECOND == 4000000, "BBCMicro::Update needs updating");

template <unsigned DIV>
static constexpr uint8_t GetSERPROCClockMask() {
    static_assert((DIV & (DIV - 1)) == 0);
    if constexpr (1.23e6 / DIV > CYCLES_PER_SECOND / 13) {
        // TODO: This case isn't emulated properly. But the data rate is too
        // high for the poor old 6502 anyway.
        return 0;
    } else {
        return DIV - 1;
    }
}

static constexpr uint8_t SERPROC_CLOCK_MASKS[8] = {
    GetSERPROCClockMask<1>(),   // 19200: 1.23e6/64/1=19219
    GetSERPROCClockMask<2>(),   // 9600: 1.23e6/64/2=9609
    GetSERPROCClockMask<4>(),   // 4800: 1.23e6/64/4=4805
    GetSERPROCClockMask<8>(),   // 2400: 1.23e6/64/8=2402
    GetSERPROCClockMask<16>(),  // 1200: 1.23e6/64/16=1201
    GetSERPROCClockMask<64>(),  // 300: 1.23e6/64/64=300
    GetSERPROCClockMask<128>(), // 150: 1.23e6/64/128=150
    GetSERPROCClockMask<256>(), // 75: 1.23e6/64/256=75
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const unsigned SERPROC_BAUD_RATES[8] = {
    19200, // 000
    1200,  // 001
    4800,  // 010
    150,   // 011
    9600,  // 100
    300,   // 101
    2400,  // 110
    75,    // 111
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SerialDataSource::~SerialDataSource() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SerialDataSink::~SerialDataSink() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteSERPROC(void *serproc_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto serproc = (SERPROC *)serproc_;

    serproc->control.value = value;

    serproc->tx_clock_mask = SERPROC_CLOCK_MASKS[serproc->control.bits.tx_baud];
    serproc->rx_clock_mask = SERPROC_CLOCK_MASKS[serproc->control.bits.rx_baud];

    if (serproc->control.bits.rs423) {
        serproc->acia->not_dcd = 0;
    } else {
        serproc->acia->not_dcd = 1;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void UpdateSERPROC(SERPROC *serproc, MC6850 *mc6850) {
    if ((serproc->clock & serproc->tx_clock_mask) == 0) {
        MC6850TransmitResult result = UpdateMC6850Transmit(mc6850);

        // TODO: should really ignore the result entirely when there's no
        // sink...
        switch (result.type) {
        case MC6850BitType_Start:
            serproc->tx_byte = 0;
            break;

        case MC6850BitType_Data:
            serproc->tx_byte <<= 1;
            serproc->tx_byte |= result.bit;
            break;

        case MC6850BitType_Stop:
            if (!!serproc->sink) {
                serproc->sink->AddByte(serproc->tx_byte);
            }
            break;
        }
    }

    if ((serproc->clock & serproc->rx_clock_mask) == 0) {
        UpdateMC6850Receive(mc6850, false);
    }

    //MaybeSetMC6850Idle(mc6850, !!serproc->source);

    ++serproc->clock;
}
