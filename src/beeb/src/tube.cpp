#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/tube.h>
#include <6502/6502.h>
#include <beeb/Trace.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Notes:
//
// - Tube enters boot mode on reset, whether the reset was power on, BREAK or
//   the PRST bit
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
const TraceEventType TUBE_WRITE_STATUS_EVENT("Write Tube status", sizeof(TubeWriteStatusEvent), TraceEventSource_Host);

const TraceEventType TUBE_WRITE_FIFO1_EVENT("Write Tube FIFO1", sizeof(TubeFIFOEvent), TraceEventSource_None);
const TraceEventType TUBE_READ_FIFO1_EVENT("Read Tube FIFO1", sizeof(TubeFIFOEvent), TraceEventSource_None);

const TraceEventType TUBE_WRITE_FIFO2_EVENT("Write Tube FIFO2", sizeof(TubeFIFOEvent), TraceEventSource_None);
const TraceEventType TUBE_READ_FIFO2_EVENT("Read Tube FIFO2", sizeof(TubeFIFOEvent), TraceEventSource_None);

const TraceEventType TUBE_WRITE_FIFO3_EVENT("Write Tube FIFO3", sizeof(TubeFIFOEvent), TraceEventSource_None);
const TraceEventType TUBE_READ_FIFO3_EVENT("Read Tube FIFO3", sizeof(TubeFIFOEvent), TraceEventSource_None);

const TraceEventType TUBE_WRITE_FIFO4_EVENT("Write Tube FIFO4", sizeof(TubeFIFOEvent), TraceEventSource_None);
const TraceEventType TUBE_READ_FIFO4_EVENT("Read Tube FIFO4", sizeof(TubeFIFOEvent), TraceEventSource_None);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr uint8_t H2P_FIFO3_DUMMY_VALUE = 0xe4;
static constexpr uint8_t P2H_FIFO3_DUMMY_VALUE = 0x96;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define CH(X) ((X) >= 32 && (X) <= 126 ? (X) : '?')

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Call when reading 1-byte FIFO.
static void UpdateLatchForRead(TubeFIFOStatus *this_status, TubeFIFOStatus *other_status) {
    if (this_status->bits.available) {
        this_status->bits.available = 0;
        other_status->bits.not_full = 1;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Call when writing 1-byte FIFO.
static void UpdateLatchForWrite(TubeFIFOStatus *this_status, TubeFIFOStatus *other_status) {
    this_status->bits.not_full = 0;
    other_status->bits.available = 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void SetTubeTrace(Tube *t, Trace *trace) {
    t->trace = trace;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
static inline void SetFIFOEvent(TubeFIFOEvent *ev, const Tube *t, uint8_t value, TubeFIFOStatus hstatus, TubeFIFOStatus pstatus) {
    ev->value = value;

    ev->h_available = hstatus.bits.available;
    ev->h_not_full = hstatus.bits.not_full;
    ev->p_available = pstatus.bits.available;
    ev->p_not_full = pstatus.bits.not_full;
    ev->p_irq = t->pirq.bits.pirq;
    ev->p_nmi = t->pirq.bits.pnmi;
    ev->h_irq = t->hirq.bits.hirq;
}

#define TRACE_FIFO(INDEX, OPERATION, SOURCE, VALUE)                                                                             \
    BEGIN_MACRO {                                                                                                               \
        if (t->trace) {                                                                                                         \
            auto ev = (TubeFIFOEvent *)t->trace->AllocEvent(TUBE_##OPERATION##_FIFO##INDEX##_EVENT, TraceEventSource_##SOURCE); \
            SetFIFOEvent(ev, t, (VALUE), t->hstatus##INDEX, t->pstatus##INDEX);                                                 \
        }                                                                                                                       \
    }                                                                                                                           \
    END_MACRO

#else

#define TRACE_FIFO(...) ((void)0)

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void UpdateHIRQ(Tube *t) {
    t->hirq.bits.hirq = t->status.bits.q && t->hstatus4.bits.available;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void UpdatePIRQ(Tube *t) {
    t->pirq.bits.pirq = ((t->status.bits.i && t->pstatus1.bits.available) ||
                         (t->status.bits.j && t->pstatus4.bits.available));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void UpdatePNMI(Tube *t) {
    bool pnmi;
    if (t->status.bits.v) {
        pnmi = t->h2p3_n == 2 || t->p2h3_n == 0;
    } else {
        pnmi = t->h2p3_n > 0 || t->p2h3_n == 0;
    }

    t->pstatus3.bits.available = pnmi;

    if (t->status.bits.m) {
        t->pirq.bits.pnmi = pnmi;
    } else {
        t->pirq.bits.pnmi = 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ResetTube(Tube *t) {
    TubeStatus old_status = t->status;

    TubeFIFOStatus empty = {};
    empty.bits.not_full = 1;
    empty.bits.available = 0;

    *t = Tube();

    t->status = old_status;

    t->hstatus1 = empty;
    t->pstatus1 = empty;
    t->p2h1_windex = 0;
    t->p2h1_rindex = 0;
    t->p2h1_n = 0;

    t->hstatus2 = empty;
    t->pstatus2 = empty;

    t->hstatus3.bits.not_full = 1;
    t->hstatus3.bits.available = 1;

    t->pstatus3.bits.not_full = 0;
    t->pstatus3.bits.available = 0;

    t->p2h3_n = 1;
    t->p2h3[1] = t->p2h3[0] = P2H_FIFO3_DUMMY_VALUE;

    t->h2p3_n = 0;
    t->h2p3[1] = t->h2p3[0] = H2P_FIFO3_DUMMY_VALUE;

    t->hstatus4 = empty;
    t->pstatus4 = empty;

    UpdatePNMI(t);
    UpdatePIRQ(t);
    UpdateHIRQ(t);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Status
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Write status
void WriteHostTube0(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    if (value & 0x80) {
        t->status.value |= value;
    } else {
        t->status.value &= ~value;
    }

    UpdateHIRQ(t);
    UpdatePIRQ(t);
    UpdatePNMI(t);

#if BBCMICRO_TRACE
    if (t->trace) {
        auto ev = (TubeWriteStatusEvent *)t->trace->AllocEvent(TUBE_WRITE_STATUS_EVENT);
        ev->new_status = t->status;

        ev->h_irq = t->hirq.bits.hirq;
        ev->p_irq = t->pirq.bits.pirq;
        ev->p_nmi = t->pirq.bits.pnmi;

        ev->h2p1_available = t->pstatus1.bits.available;

        ev->h2p3_n = t->h2p3_n;
        ev->p2h3_n = t->p2h3_n;

        ev->h2p4_available = t->pstatus4.bits.available;
        ev->p2h4_available = t->hstatus4.bits.available;
    }
#endif
}

// Read Tube control register + FIFO 1 host status
uint8_t ReadHostTube0(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    ASSERT(t->hstatus1.bits._ == 0);
    return t->hstatus1.value | (t->status.value & 0x3f);
}

// Read FIFO 1 parasite->host
uint8_t ReadHostTube1(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    uint8_t value = 1;
    if (t->p2h1_n > 0) {
        value = t->p2h1[t->p2h1_rindex];

        ++t->p2h1_rindex;
        if (t->p2h1_rindex == sizeof t->p2h1) {
            t->p2h1_rindex = 0;
        }

        --t->p2h1_n;
        t->pstatus1.bits.not_full = 1;
        if (t->p2h1_n == 0) {
            t->hstatus1.bits.available = 0;
        }

        TRACE_FIFO(1, READ, Host, value);
    }

    return value;
}

// Read FIFO 1 host->parasite
uint8_t ReadParasiteTube1(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    UpdateLatchForRead(&t->pstatus1, &t->hstatus1);
    UpdatePIRQ(t);
    TRACE_FIFO(1, READ, Parasite, t->h2p1);

    return t->h2p1;
}

// Read Tube control register + FIFO 1 parasite status
uint8_t ReadParasiteTube0(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->pstatus1.value | (t->status.value & 0x3f);
}

// Write FIFO 1 host->parasite
void WriteHostTube1(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->h2p1 = value;

    UpdateLatchForWrite(&t->hstatus1, &t->pstatus1);

    UpdatePIRQ(t);

    TRACE_FIFO(1, WRITE, Host, value);
}

// Write FIFO 1 parasite->host
void WriteParasiteTube1(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    if (t->p2h1_n < sizeof t->p2h1) {
        t->p2h1[t->p2h1_windex] = value;

        ++t->p2h1_windex;
        if (t->p2h1_windex == sizeof t->p2h1) {
            t->p2h1_windex = 0;
        }

        ++t->p2h1_n;
        if (t->p2h1_n == sizeof t->p2h1) {
            t->pstatus1.bits.not_full = 0;
        }

        t->hstatus1.bits.available = 1;

        TRACE_FIFO(1, WRITE, Parasite, value);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// FIFO 2
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Read FIFO 2 host status
uint8_t ReadHostTube2(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    // A2 F2 1 1 1 1 1 1
    return t->hstatus2.value;
}

// Read FIFO 2 parasite->host
uint8_t ReadHostTube3(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    UpdateLatchForRead(&t->hstatus2, &t->pstatus2);

    TRACE_FIFO(2, READ, Host, t->p2h2);

    return t->p2h2;
}

// Write FIFO 2 host->parasite
void WriteHostTube3(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->h2p2 = value;

    UpdateLatchForWrite(&t->hstatus2, &t->pstatus2);

    TRACE_FIFO(2, WRITE, Host, value);
}

// Read FIFO 2 parasite status
uint8_t ReadParasiteTube2(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->pstatus2.value;
}

// Read FIFO 2 host->parasite
uint8_t ReadParasiteTube3(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    UpdateLatchForRead(&t->pstatus2, &t->hstatus2);

    TRACE_FIFO(2, READ, Parasite, t->h2p2);

    return t->h2p2;
}

// Write FIFO 2 parasite->host
void WriteParasiteTube3(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->p2h2 = value;

    UpdateLatchForWrite(&t->pstatus2, &t->hstatus2);

    TRACE_FIFO(2, WRITE, Parasite, value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// FIFO 3
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t ReadFIFO3(Tube *t,
                         TubeFIFOStatus *this_status,
                         TubeFIFOStatus *other_status,
                         uint8_t *fifo,
                         uint8_t *fifo_n,
                         uint8_t dummy) {
    uint8_t value;
    if (*fifo_n > 0) {
        value = fifo[0];
        fifo[0] = fifo[1];
        --*fifo_n;
    } else {
        value = dummy;
    }

    if (*fifo_n == 0) {
        this_status->bits.available = 0;
        other_status->bits.not_full = 1;
    }

    UpdatePNMI(t);

    return value;
}

static void WriteFIFO3(Tube *t,
                       TubeFIFOStatus *this_status,
                       TubeFIFOStatus *other_status,
                       uint8_t *fifo,
                       uint8_t *fifo_n,
                       uint8_t value) {
    if (*fifo_n < 2) {
        fifo[*fifo_n] = value;
        ++*fifo_n;
    }

    if (t->status.bits.v) {
        // 2-byte mode
        if (*fifo_n == 2) {
            this_status->bits.not_full = 0;
            other_status->bits.available = 1;
        }
    } else {
        // 1-byte mode
        UpdateLatchForWrite(this_status, other_status);
    }

    UpdatePNMI(t);
}

// Read FIFO 3 host status
uint8_t ReadHostTube4(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->hstatus3.value;
}

// Read FIFO 3 parasite->host
uint8_t ReadHostTube5(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    uint8_t value = ReadFIFO3(t, &t->hstatus3, &t->pstatus3, t->p2h3, &t->p2h3_n, P2H_FIFO3_DUMMY_VALUE);
    TRACE_FIFO(3, READ, Host, value);
    return value;
}

// Write FIFO3 host->parasite
void WriteHostTube5(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    WriteFIFO3(t, &t->hstatus3, &t->pstatus3, t->h2p3, &t->h2p3_n, value);
    TRACE_FIFO(3, WRITE, Host, value);
}

// Read FIFO 3 parasite status
uint8_t ReadParasiteTube4(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    TubeFIFOStatus status = t->pstatus3;

    return status.value;
}

// Read FIFO 3 host->parasite
uint8_t ReadParasiteTube5(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    uint8_t value = ReadFIFO3(t, &t->pstatus3, &t->hstatus3, t->h2p3, &t->h2p3_n, H2P_FIFO3_DUMMY_VALUE);
    TRACE_FIFO(3, READ, Parasite, value);
    return value;
}

// Write FIFO 3 parasite->host
void WriteParasiteTube5(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    WriteFIFO3(t, &t->pstatus3, &t->hstatus3, t->p2h3, &t->p2h3_n, value);
    TRACE_FIFO(3, WRITE, Parasite, value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// FIFO 4
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Read FIFO 4 host status
uint8_t ReadHostTube6(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->hstatus4.value;
}

// Read FIFO 4 parasite->host
uint8_t ReadHostTube7(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    UpdateLatchForRead(&t->hstatus4, &t->pstatus4);
    UpdateHIRQ(t);
    TRACE_FIFO(4, READ, Host, t->p2h4);

    return t->p2h4;
}

// Write FIFO4 host->parasite
void WriteHostTube7(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->h2p4 = value;

    UpdateLatchForWrite(&t->hstatus4, &t->pstatus4);
    UpdatePIRQ(t);
    TRACE_FIFO(4, WRITE, Host, value);
}

// Read FIFO 4 parasite status
uint8_t ReadParasiteTube6(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->pstatus4.value;
}

// Write FIFO 4 parasite->host
void WriteParasiteTube7(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->p2h4 = value;

    UpdateLatchForWrite(&t->pstatus4, &t->hstatus4);
    UpdateHIRQ(t);
    TRACE_FIFO(4, WRITE, Parasite, value);
}

// Read FIFO 4 host->parasite
uint8_t ReadParasiteTube7(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    UpdateLatchForRead(&t->pstatus4, &t->hstatus4);
    UpdatePIRQ(t);
    TRACE_FIFO(4, READ, Parasite, t->h2p4);

    return t->h2p4;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteTubeDummy(void *, M6502Word, uint8_t) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
