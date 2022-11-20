#ifndef HEADER_BBA937BCD674470BB9D2DC3558861F5C // -*- mode:c++ -*-
#define HEADER_BBA937BCD674470BB9D2DC3558861F5C

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

static constexpr size_t TUBE_FIFO1_SIZE_BYTES = 24;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

union M6502Word;
#if BBCMICRO_TRACE
class Trace;
class TraceEventType;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TubeFIFOStatusBits {
    uint8_t _ : 6;
    uint8_t not_full : 1;
    uint8_t available : 1;
};

union TubeFIFOStatus {
    TubeFIFOStatusBits bits;
    uint8_t value;
};
static_assert(sizeof(TubeFIFOStatus) == 1, "");

struct TubeStatusBits {
    uint8_t q : 1; //enable HIRQ from register 4
    uint8_t i : 1; //enable PIRQ from register 4
    uint8_t j : 1; //enable PIRQ from register 4
    uint8_t m : 1; //enable PNMI from register 3
    uint8_t v : 1; //2-byte operation of register 3
    uint8_t p : 1; //activate PRST
    uint8_t t : 1; //clear all Tube registers
    uint8_t s : 1; //set/reset flag(s) indicated by mask
};

union TubeStatus {
    TubeStatusBits bits;
    uint8_t value;
};
static_assert(sizeof(TubeStatus) == 1, "");

struct TubeHostIRQBits {
    uint8_t hirq : 1;
};

union TubeHostIRQ {
    uint8_t value;
    TubeHostIRQBits bits;
};

struct TubeParasiteIRQBits {
    uint8_t pirq : 1;
    uint8_t pnmi : 1;
};

union TubeParasiteIRQ {
    uint8_t value;
    TubeParasiteIRQBits bits;
};

struct Tube {
    TubeHostIRQ hirq = {};
    TubeParasiteIRQ pirq = {};

    TubeStatus status = {};

    // FIFO 1
    TubeFIFOStatus hstatus1 = {};
    TubeFIFOStatus pstatus1 = {};
    uint8_t h2p1 = 0;
    uint8_t p2h1[TUBE_FIFO1_SIZE_BYTES] = {};
    uint8_t p2h1_windex = 0;
    uint8_t p2h1_rindex = 0;
    uint8_t p2h1_n = 0;

    // "FIFO" 2
    TubeFIFOStatus hstatus2 = {};
    TubeFIFOStatus pstatus2 = {};
    uint8_t h2p2 = 0;
    uint8_t p2h2 = 0;

    // FIFO 3
    TubeFIFOStatus hstatus3 = {};
    TubeFIFOStatus pstatus3 = {};
    uint8_t p2h3[2] = {};
    uint8_t p2h3_n = 0;
    uint8_t h2p3[2] = {};
    uint8_t h2p3_n = 0;

    // "FIFO" 4
    TubeFIFOStatus hstatus4 = {};
    TubeFIFOStatus pstatus4 = {};
    uint8_t h2p4 = 0;
    uint8_t p2h4 = 0;

#if BBCMICRO_TRACE
    Trace *trace = nullptr;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
extern const TraceEventType TUBE_WRITE_STATUS_EVENT;

extern const TraceEventType TUBE_WRITE_FIFO1_EVENT;
extern const TraceEventType TUBE_READ_FIFO1_EVENT;

extern const TraceEventType TUBE_WRITE_FIFO2_EVENT;
extern const TraceEventType TUBE_READ_FIFO2_EVENT;

extern const TraceEventType TUBE_WRITE_FIFO3_EVENT;
extern const TraceEventType TUBE_READ_FIFO3_EVENT;

extern const TraceEventType TUBE_WRITE_FIFO4_EVENT;
extern const TraceEventType TUBE_READ_FIFO4_EVENT;

#include <shared/pshpack1.h>
struct TubeFIFOEvent {
    // Value read or written.
    uint8_t value;

    // Copy of the appropriate FIFO's status registers.
    uint8_t h_not_full : 1;
    uint8_t h_available : 1;
    uint8_t p_not_full : 1;
    uint8_t p_available : 1;

    // Copy of the interrupt flags.
    uint8_t h_irq : 1;
    uint8_t p_irq : 1;
    uint8_t p_nmi : 1;
};
#include <shared/poppack.h>

#include <shared/pshpack1.h>
struct TubeWriteStatusEvent {
    TubeStatus new_status;

    // Copy of various flags.
    uint8_t h2p3_n : 2;
    uint8_t p2h3_n : 2;

    uint8_t h2p1_available : 1;
    uint8_t h2p4_available : 1;
    uint8_t p2h4_available : 1;

    uint8_t h_irq : 1;
    uint8_t p_irq : 1;
    uint8_t p_nmi : 1;
};
#include <shared/poppack.h>

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ResetTube(Tube *t);
#if BBCMICRO_TRACE
void SetTubeTrace(Tube *t, Trace *trace);
#endif

uint8_t ReadHostTube0(void *tube, M6502Word a);
uint8_t ReadHostTube1(void *tube, M6502Word a);
uint8_t ReadHostTube2(void *tube, M6502Word a);
uint8_t ReadHostTube3(void *tube, M6502Word a);
uint8_t ReadHostTube4(void *tube, M6502Word a);
uint8_t ReadHostTube5(void *tube, M6502Word a);
uint8_t ReadHostTube6(void *tube, M6502Word a);
uint8_t ReadHostTube7(void *tube, M6502Word a);
void WriteHostTube0(void *tube, M6502Word a, uint8_t value);
void WriteHostTube1(void *tube, M6502Word a, uint8_t value);
void WriteHostTube3(void *tube, M6502Word a, uint8_t value);
void WriteHostTube5(void *tube, M6502Word a, uint8_t value);
void WriteHostTube7(void *tube, M6502Word a, uint8_t value);

uint8_t ReadParasiteTube0(void *tube, M6502Word a);
uint8_t ReadParasiteTube1(void *tube, M6502Word a);
uint8_t ReadParasiteTube2(void *tube, M6502Word a);
uint8_t ReadParasiteTube3(void *tube, M6502Word a);
uint8_t ReadParasiteTube4(void *tube, M6502Word a);
uint8_t ReadParasiteTube5(void *tube, M6502Word a);
uint8_t ReadParasiteTube6(void *tube, M6502Word a);
uint8_t ReadParasiteTube7(void *tube, M6502Word a);
void WriteParasiteTube1(void *tube, M6502Word a, uint8_t value);
void WriteParasiteTube3(void *tube, M6502Word a, uint8_t value);
void WriteParasiteTube5(void *tube, M6502Word a, uint8_t value);
void WriteParasiteTube7(void *tube, M6502Word a, uint8_t value);

void WriteTubeDummy(void *tube, M6502Word a, uint8_t value);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
