#ifndef HEADER_F94E88F4203549FF9348C20B1A8E40B3
#define HEADER_F94E88F4203549FF9348C20B1A8E40B3

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include <6502/6502.h>

#if BBCMICRO_TRACE
class Trace;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "6522.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class R6522 {
public:
#include <shared/pushwarn_bitfields.h>
    struct PCRBits {
        uint8_t ca1_pos_irq:1;
        uint8_t ca2_mode:3;
        uint8_t cb1_pos_irq:1;
        uint8_t cb2_mode:3;

    };
#include <shared/popwarn.h>

    union PCR {
        PCRBits bits;
        uint8_t value;
    };

#include <shared/pushwarn_bitfields.h>
    struct ACRBits {
        uint8_t pa_latching:1;
        uint8_t pb_latching:1;
        uint8_t sr:3;
        uint8_t t2_count_pb6:1;
        uint8_t t1_continuous:1;
        uint8_t t1_output_pb7:1;
    };
#include <shared/popwarn.h>

    union ACR {
        ACRBits bits;
        uint8_t value;
    };
    typedef union R6522ACR R6522ACR;

#include <shared/pushwarn_bitfields.h>
    struct IRQBits {
        uint8_t ca2:1;
        uint8_t ca1:1;
        uint8_t sr:1;
        uint8_t cb2:1;
        uint8_t cb1:1;
        uint8_t t2:1;
        uint8_t t1:1;
        uint8_t _:1;
    };
#include <shared/popwarn.h>

    union IRQ {
        IRQBits bits;
        uint8_t value;
    };

    /* Cx1 and Cx2 */
#include <shared/pushwarn_bitfields.h>
    struct Port {
    private:
        /* ORx */
        /* (`or' conflicts with iso646) */
        uint8_t or_;

        /* DDRx */
        uint8_t ddr;

        /* Px */
    public:
        uint8_t p;
    private:
        uint8_t p_latch;

        /* Cx1 */
    public:
        uint8_t c1;
    private:
        uint8_t old_c1;

        /* Cx2 */
    public:
        uint8_t c2;
    private:
        uint8_t old_c2;
        uint8_t pulse;
    public:
        /* Callback */
        typedef void (*ChangeFn)(R6522 *via,uint8_t value,uint8_t old_value,void *context);
        ChangeFn fn;
        void *fn_context;

        friend class R6522;
#if BBCMICRO_DEBUGGER
        friend class R6522DebugWindow;
#endif
    };
#include <shared/popwarn.h>

    Port a={};
    Port b={};

    /* for me, in the debugger... */
    const char *tag=nullptr;

    void Reset();

    static void Write0(void *via,M6502Word addr,uint8_t value);
    static void Write1(void *via,M6502Word addr,uint8_t value);
    static void Write2(void *via,M6502Word addr,uint8_t value);
    static void Write3(void *via,M6502Word addr,uint8_t value);
    static void Write4(void *via,M6502Word addr,uint8_t value);
    static void Write5(void *via,M6502Word addr,uint8_t value);
    static void Write6(void *via,M6502Word addr,uint8_t value);
    static void Write7(void *via,M6502Word addr,uint8_t value);
    static void Write8(void *via,M6502Word addr,uint8_t value);
    static void Write9(void *via,M6502Word addr,uint8_t value);
    static void WriteA(void *via,M6502Word addr,uint8_t value);
    static void WriteB(void *via,M6502Word addr,uint8_t value);
    static void WriteC(void *via,M6502Word addr,uint8_t value);
    static void WriteD(void *via,M6502Word addr,uint8_t value);
    static void WriteE(void *via,M6502Word addr,uint8_t value);
    static void WriteF(void *via,M6502Word addr,uint8_t value);

    static uint8_t Read0(void *via,M6502Word addr);
    static uint8_t Read1(void *via,M6502Word addr);
    static uint8_t Read2(void *via,M6502Word addr);
    static uint8_t Read3(void *via,M6502Word addr);
    static uint8_t Read4(void *via,M6502Word addr);
    static uint8_t Read5(void *via,M6502Word addr);
    static uint8_t Read6(void *via,M6502Word addr);
    static uint8_t Read7(void *via,M6502Word addr);
    static uint8_t Read8(void *via,M6502Word addr);
    static uint8_t Read9(void *via,M6502Word addr);
    static uint8_t ReadA(void *via,M6502Word addr);
    static uint8_t ReadB(void *via,M6502Word addr);
    static uint8_t ReadC(void *via,M6502Word addr);
    static uint8_t ReadD(void *via,M6502Word addr);
    static uint8_t ReadE(void *via,M6502Word addr);
    static uint8_t ReadF(void *via,M6502Word addr);

    // returns IRQ flag: true = IRQ, false = no IRQ
    uint8_t Update();

#if BBCMICRO_TRACE
    void SetTrace(Trace *t);
#endif
protected:
private:
    /* uint8_t pa; */
    /* uint8_t pa_latch; */
    /* uint8_t pb,old_pb; */
    /* uint8_t pb_latch; */

    uint8_t m_t1ll=250;
    uint8_t m_t1lh=202;
    uint8_t m_t2ll=0;
    uint8_t m_sr=0;
    ACR m_acr={};
    PCR m_pcr={};
    IRQ m_ifr={};
    IRQ m_ier={};

    int32_t m_t1=0;
    int32_t m_t2=0;

    //uint8_t m_irq_output=0;

    /* next value for pb7 when in T1 output toggle mode */
    uint8_t m_next_pb7=0;

    /* whether to generate an IRQ on next T2 timeout */
    uint8_t m_t2_irq=0;

    /* when not in continuous mode, whether to generate an IRQ on next
     * T1 timeout */
    uint8_t m_t1_irq=0;

    /* old value of port B, for use when counting PB6 pulses. */
    uint8_t m_old_pb=0;

#if BBCMICRO_TRACE
    Trace *m_trace=nullptr;
#endif

    void TickControl(Port *port,uint8_t latching,uint8_t pcr_bits,uint8_t cx2_mask);
    void DoPortHandshakingRead(Port *port,uint8_t pcr_bits,uint8_t irqmask2);
    void DoPortHandshakingWrite(Port *port,uint8_t pcr_bits,uint8_t irqmask2);
    void UpdatePortPins(Port *port);

#if BBCMICRO_DEBUGGER
    friend class R6522DebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CHECK_SIZEOF(R6522::PCR,1);
CHECK_SIZEOF(R6522::ACR,1);
CHECK_SIZEOF(R6522::IRQ,1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
