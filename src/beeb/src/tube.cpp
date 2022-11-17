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

void ResetTube(Tube *t) {
    TubeStatus old_status = t->status;

    TubeFIFOStatus empty = {};
    empty.bits.not_full = 1;
    empty.bits.available = 0;

    *t = Tube();

    t->status = old_status;

    t->hstatus1 = empty;
    t->hstatus2 = empty;
    t->hstatus4 = empty;

    t->pstatus1 = empty;
    t->pstatus2 = empty;
    t->pstatus4 = empty;

    t->hstatus3.bits.not_full = 0;
    t->hstatus3.bits.available = 1;

    t->pstatus3.bits.not_full = 0;
    t->pstatus3.bits.available = 0;
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
    if (t->status.bits.m && t->status.bits.v) {
        t->pirq.bits.pnmi = t->h2p3_n == 2 || t->p2h3_n == 0;
    } else if (t->status.bits.m && !t->status.bits.v) {
        t->pirq.bits.pnmi = t->h2p3_n > 0 || t->p2h3_n == 0;
    } else {
        t->pirq.bits.pnmi = 0;
    }
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
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// FIFO 1
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

// Want to have this stuff global, like BINARY_STRS?
static char g_dec_strs[256][5];
static char g_hex_strs[256][5];
static char g_char_strs[256][5];

static bool InitializeStrs() {
    for (unsigned i = 0; i < 256; ++i) {
        snprintf(g_dec_strs[i], sizeof g_dec_strs[i], "%3u ", i);
        ASSERT(strlen(g_dec_strs[i]) == 4);

        snprintf(g_hex_strs[i], sizeof g_hex_strs[i], "$%02x ", i);
        ASSERT(strlen(g_hex_strs[i]) == 4);

        if (i >= 32 && i <= 126) {
            snprintf(g_char_strs[i], sizeof g_char_strs[i], "%c   ", (char)i);
        } else {
            const char *str;
            switch (i) {
            case 9:
                str = "\\t";
                break;

            case 10:
                str = "\\n";
                break;

            case 13:
                str = "\\r";
                break;

            default:
                str = "";
                break;
            }

            snprintf(g_char_strs[i], sizeof g_char_strs[i], "%-4s", str);
        }
        ASSERT(strlen(g_char_strs[i]) == 4);
    }

    return true;
}

static const bool g_got_strs = InitializeStrs();

static void TraceParasiteToHostFIFO1(Tube *t, TraceEventSource source, const char *op) {
    if (t->trace) {
        static constexpr size_t STR_SIZE = sizeof t->p2h1 * 4 + 1;
        char dec[STR_SIZE], hex[STR_SIZE], chars[STR_SIZE], index[STR_SIZE];
        size_t src = t->p2h1_rindex, dest = 0;
        for (uint8_t i = 0; i < t->p2h1_n; ++i) {
            uint8_t value = t->p2h1[src];
            memcpy(&index[dest], g_dec_strs[i], 4);
            memcpy(&dec[dest], g_dec_strs[value], 4);
            memcpy(&hex[dest], g_hex_strs[value], 4);
            memcpy(&chars[dest], g_char_strs[value], 4);

            dest += 4;

            src = (src + 1) % sizeof t->p2h1;
        }

        index[dest] = 0;
        dec[dest] = 0;
        hex[dest] = 0;
        chars[dest] = 0;

        t->trace->AllocStringf(source, "%s FIFO1", op);
        t->trace->AllocStringf(source, "Index: %s", index);
        t->trace->AllocStringf(source, "Dec  : %s", dec);
        t->trace->AllocStringf(source, "Hex  : %s", hex);
        t->trace->AllocStringf(source, "ASCII: %s", chars);
    }
}

#else

#define TraceParasiteToHostFIFO1(T) ((void)0)

#endif

// Read Tube control register + FIFO 1 host status
uint8_t ReadHostTube0(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->hstatus1.value | (t->status.value & 0x3f);
}

// Read FIFO 1 parasite->host
uint8_t ReadHostTube1(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    uint8_t value = 0;
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

        TraceParasiteToHostFIFO1(t, TraceEventSource_Host, "read");
    }

    return value;
}

// Read FIFO 1 host->parasite
uint8_t ReadParasiteTube1(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    UpdateLatchForRead(&t->pstatus1, &t->hstatus1);
    UpdatePIRQ(t);

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

    TRACEF(t->trace, "h->p FIFO1=%u (0x%02x, '%c') H not full=%u, P available=%u, PIRQ=%u",
           t->h2p1,
           t->h2p1,
           CH(t->h2p1),
           t->hstatus1.bits.not_full,
           t->pstatus1.bits.available,
           t->pirq.bits.pirq);
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

        TraceParasiteToHostFIFO1(t, TraceEventSource_Parasite, "write");
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

    return t->p2h2;
}

// Write FIFO 2 host->parasite
void WriteHostTube3(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->h2p2 = value;

    UpdateLatchForWrite(&t->hstatus2, &t->pstatus2);
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

    return t->h2p2;
}

// Write FIFO 2 parasite->host
void WriteParasiteTube3(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->p2h2 = value;

    UpdateLatchForWrite(&t->pstatus2, &t->hstatus2);
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
                         uint8_t *fifo_n) {
    uint8_t value = fifo[0];
    fifo[0] = fifo[1];
    if (*fifo_n > 0) {
        --*fifo_n;
    }

    if (*fifo_n == 0 || !t->status.bits.v) {
        this_status->bits.not_full = 1;
        other_status->bits.available = 0;
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
    if (t->status.bits.v) {
        // 2-byte mode
        if (*fifo_n < 2) {
            fifo[*fifo_n] = value;
            ++*fifo_n;
        }
        if (*fifo_n == 2) {
            this_status->bits.not_full = 0;
            other_status->bits.available = 1;
        }
    } else {
        // 1-byte mode
        fifo[0] = value;
        *fifo_n = 1;
        this_status->bits.not_full = 0;
        other_status->bits.available = 1;
    }

    UpdatePNMI(t);
}

// Read FIFO 3 host status
uint8_t ReadHostTube4(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->hstatus3.value;
}

// Read FIFO 3 parasite->Host
uint8_t ReadHostTube5(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    uint8_t value = ReadFIFO3(t, &t->hstatus3, &t->pstatus3, t->p2h3, &t->p2h3_n);
    return value;
}

// Write FIFO3 host->parasite
void WriteHostTube5(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    WriteFIFO3(t, &t->hstatus3, &t->pstatus3, t->h2p3, &t->h2p3_n, value);
}

// Read FIFO 3 parasite status
uint8_t ReadParasiteTube4(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    return t->pstatus3.value;
}

// Read FIFO 3 host->parasite
uint8_t ReadParasiteTube5(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    uint8_t value = ReadFIFO3(t, &t->pstatus3, &t->hstatus3, t->h2p3, &t->h2p3_n);
    return value;
}

// Write FIFO 3 parasite->host
void WriteParasiteTube5(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    WriteFIFO3(t, &t->pstatus3, &t->hstatus3, t->p2h3, &t->p2h3_n, value);
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

    return t->p2h4;
}

// Write FIFO4 host->parasite
void WriteHostTube7(void *tube_, M6502Word, uint8_t value) {
    auto t = (Tube *)tube_;

    t->h2p4 = value;

    UpdateLatchForWrite(&t->hstatus4, &t->pstatus4);
    UpdatePIRQ(t);
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
}

// Read FIFO 4 host->parasite
uint8_t ReadParasiteTube7(void *tube_, M6502Word) {
    auto t = (Tube *)tube_;

    UpdateLatchForRead(&t->pstatus4, &t->hstatus4);
    UpdatePIRQ(t);

    return t->h2p4;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteTubeDummy(void *, M6502Word, uint8_t) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
