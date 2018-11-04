#include <shared/system.h>
#include <shared/debug.h>
#include <stdio.h>
#include <beeb/6522.h>
#include <string.h>
#include <beeb/Trace.h>

#include <shared/enum_def.h>
#include <beeb/6522.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// expansion starts with " ".
#define IRQ_FMT "%s%s%s%s%s%s%s%s"
#define IRQ__ARG(IRQ,BIT) (((IRQ).bits.BIT)?(" " #BIT):"")
#define IRQ_ARGS(IRQ) IRQ__ARG(IRQ,t1),IRQ__ARG(IRQ,t2),IRQ__ARG(IRQ,cb1),IRQ__ARG(IRQ,cb2),IRQ__ARG(IRQ,sr),IRQ__ARG(IRQ,ca1),IRQ__ARG(IRQ,ca2),(((IRQ).value&0x7f)==0?" -":"")

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
const TraceEventType R6522::IRQ_EVENT("R6522IRQEvent",sizeof(IRQEvent));
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void R6522::DoPortHandshakingRead(Port *port,uint8_t pcr_bits,uint8_t irqmask2) {
    /* Always clear Cx1 */
    this->ifr.value&=~(irqmask2<<1);

    switch(pcr_bits&7) {
    case R6522Cx2Control_Input_IndIRQNegEdge:
    case R6522Cx2Control_Input_IndIRQPosEdge:
    case R6522Cx2Control_Output_Low:
    case R6522Cx2Control_Output_High:
        /* Leave Cx2 */
        break;

    case R6522Cx2Control_Input_NegEdge:
    case R6522Cx2Control_Input_PosEdge:
    case R6522Cx2Control_Output_Handshake:
        /* Clear Cx2 */
        this->ifr.value&=~irqmask2;
        break;

    case R6522Cx2Control_Output_Pulse:
        /* Clear Cx2 and prepare for the pulse */
        this->ifr.value&=~irqmask2;
        port->pulse=2;
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void R6522::DoPortHandshakingWrite(Port *port,uint8_t pcr_bits,uint8_t irqmask2) {
    switch(pcr_bits&7) {
    case R6522Cx2Control_Output_Handshake:
        this->ifr.value&=~irqmask2;
        break;

    case R6522Cx2Control_Output_Pulse:
        this->ifr.value&=~irqmask2;
        port->pulse=2;
        break;

    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void R6522::UpdatePortPins(Port *port) {
    uint8_t old_p=port->p;

    port->p=(port->p&~port->ddr)|(port->or_&port->ddr);

    if(port->p!=old_p) {
        if(port->fn) {
            (*port->fn)(this,port->p,old_p,port->fn_context);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void R6522::Reset() {
    R6522 old=*this;

    *this=R6522();

    this->a.fn=old.a.fn;
    this->a.fn_context=old.a.fn_context;

    this->b.fn=old.b.fn;
    this->b.fn_context=old.b.fn_context;

    m_id=old.m_id;
    m_name=old.m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void R6522::SetID(uint8_t id,const char *name) {
    m_id=id;
    m_name=name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* IRB */
uint8_t R6522::Read0(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->DoPortHandshakingRead(&via->b,via->m_pcr.value>>4,R6522IRQMask_CB2);

    uint8_t value=via->b.or_&via->b.ddr;

    if(via->m_acr.bits.pb_latching) {
        value|=via->b.p_latch&~via->b.ddr;
    } else {
        value|=via->b.p&~via->b.ddr;
    }

    return value;
}

/* ORB */
void R6522::Write0(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->b.or_=value;

    via->DoPortHandshakingWrite(&via->b,via->m_pcr.value>>4,R6522IRQMask_CB2);

    via->UpdatePortPins(&via->b);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* IRA (no handshaking) */
uint8_t R6522::ReadF(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    if(via->m_acr.bits.pa_latching) {
        return via->a.p_latch;
    } else {
        return via->a.p;
    }
}

/* IRA */
uint8_t R6522::Read1(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->DoPortHandshakingRead(&via->a,via->m_pcr.value>>0,R6522IRQMask_CA2);

    return R6522::ReadF(via,addr);
}

/* ORA (no handshaking) */
void R6522::WriteF(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->a.or_=value;

    via->DoPortHandshakingWrite(&via->a,via->m_pcr.value>>0,R6522IRQMask_CA2);

    via->UpdatePortPins(&via->a);
}

/* ORA */
void R6522::Write1(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    WriteF(via,addr,value);

    via->UpdatePortPins(&via->a);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* DDRB */
uint8_t R6522::Read2(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->b.ddr;
}

void R6522::Write2(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->b.ddr=value;

    via->UpdatePortPins(&via->b);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* DDRA */
uint8_t R6522::Read3(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->a.ddr;
}

void R6522::Write3(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->a.ddr=value;

    via->UpdatePortPins(&via->a);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* T1C-L */
uint8_t R6522::Read4(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->ifr.bits.t1=0;

    return (uint8_t)(via->m_t1);
}

/* T1L-L */
void R6522::Write4(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->m_t1ll=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* T1C-H */
uint8_t R6522::Read5(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return (uint8_t)(via->m_t1>>8);
}

void R6522::Write5(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->ifr.bits.t1=0;
    via->m_t1_irq=1;

    via->m_t1lh=value;

    via->m_t1=(via->m_t1ll|via->m_t1lh<<8)+1;

    if(via->m_acr.bits.t1_output_pb7) {
        via->m_next_pb7=0;
    }

    TRACEF(via->m_trace,"%s - Write T1C-H. T1=%d T1_irq=%d",via->m_name,via->m_t1,via->m_t1_irq);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* T1L-L */
uint8_t R6522::Read6(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->m_t1ll;
}

void R6522::Write6(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->m_t1ll=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* T1L-H */
uint8_t R6522::Read7(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->m_t1lh;
}

void R6522::Write7(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    /* See V.TIMERS. My model-b notes say Skirmish needs this. */
    via->ifr.bits.t1=0;

    TRACEF(via->m_trace,"%s - Write T1L-H. IFR:" IRQ_FMT,via->m_name,IRQ_ARGS(via->ifr));

    via->m_t1lh=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* T2C-L */
uint8_t R6522::Read8(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->ifr.bits.t2=0;

    return (uint8_t)via->m_t2;
}

void R6522::Write8(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->m_t2ll=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* T2C-H */
uint8_t R6522::Read9(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return (uint8_t)(via->m_t2>>8);
}

void R6522::Write9(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->ifr.bits.t2=0;

    via->m_t2=(via->m_t2ll|value<<8)+1;
    via->m_t2_irq=1;

    TRACEF(via->m_trace,"%s - write T2C-H. T2=%d ($%04X)",via->m_name,via->m_t2,via->m_t2);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* SR */
uint8_t R6522::ReadA(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->m_sr;
}

void R6522::WriteA(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->m_sr=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* ACR */
uint8_t R6522::ReadB(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->m_acr.value;
}

void R6522::WriteB(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->m_acr.value=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* PCR */
uint8_t R6522::ReadC(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->m_pcr.value;
}

void R6522::WriteC(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->m_pcr.value=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* IFR */
uint8_t R6522::ReadD(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    uint8_t value=via->ifr.value&0x7f;

    if(via->ier.value&via->ifr.value&0x7f) {
        value|=0x80;
    }

    return value;
}

void R6522::WriteD(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    via->ifr.value&=~value;

    TRACEF(via->m_trace,"%s - Write IFR. IFR:" IRQ_FMT,via->m_name,IRQ_ARGS(via->ifr));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* IER */
uint8_t R6522::ReadE(void *via_,M6502Word addr) {
    auto via=(R6522 *)via_;
    (void)addr;

    return via->ier.value|0x80;
}

void R6522::WriteE(void *via_,M6502Word addr,uint8_t value) {
    auto via=(R6522 *)via_;
    (void)addr;

    if(value&0x80) {
        via->ier.value|=value;
    } else {
        via->ier.value&=~value;
    }

    TRACEF(via->m_trace,"%s - Write IER. IER:" IRQ_FMT,via->m_name,IRQ_ARGS(via->ier));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t R6522::Update() {
    /* CA1/CA2 */
    TickControl(&this->a,m_acr.bits.pa_latching,m_pcr.value>>0,R6522IRQMask_CA2);

    /* CB1/CB2 */
    TickControl(&this->b,m_acr.bits.pb_latching,m_pcr.value>>4,R6522IRQMask_CB2);

    /* Count down T1 */
    {
        if(m_t1--<0) {
            if(m_t1_irq) {
                this->ifr.bits.t1=1;

                if(!m_acr.bits.t1_continuous) {
                    m_t1_irq=0;
                }
            }

            m_t1=m_t1ll|m_t1lh<<8;

            if(m_acr.bits.t1_output_pb7) {
                //if(this->b.ddr&0x80) {
                this->b.p&=0x7f;
                ASSERT(!(m_next_pb7&0x7f));
                this->b.p|=m_next_pb7;
                m_next_pb7^=0x80;
                //}
            }

            TRACEF(m_trace,"%s - T1 timed out (continuous=%d). T1 new value: %d ($%04X)",m_name,m_acr.bits.t1_continuous,m_t1,m_t1);
        }
    }

    /* Count down T2 */
    {
        if(!(m_acr.bits.t2_count_pb6)||
            ((m_old_pb&0x40)&~(this->b.p&0x40)))
        {
            if(m_t2--<0) {
                if(m_t2_irq) {
                    TRACEF(m_trace,"%s - T2 timed out.",m_name);

                    this->ifr.bits.t2=1;
                    m_t2_irq=0;
                }
            }
        }

        m_old_pb=this->b.p;
    }

    uint8_t any_irqs=this->ifr.value&this->ier.value&0x7f;

#if BBCMICRO_TRACE
    if(any_irqs) {
        if(m_trace) {
            auto ev=(IRQEvent *)m_trace->AllocEvent(IRQ_EVENT);
            ev->ifr=this->ifr;
            ev->ier=this->ier;
        }
    }
#endif

    /* Assert IRQ if necessary. */
    return any_irqs;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void R6522::SetTrace(Trace *trace) {
    m_trace=trace;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void R6522::TickControl(Port *port,uint8_t latching,uint8_t pcr_bits,uint8_t cx2_mask) {
    /* Check for Cx1 */
    {
        /* port->old_c1!=port->c1&&port->c1==(pcr_bits&1) */
        uint8_t code=(port->c1|(port->old_c1<<1)|(pcr_bits<<2))&7;

        // PCRb0 oc1 c1   res
        // ----- --- ---  ---
        //   0    0   0    0
        //   0    0   1    0
        //   0    1   0    1
        //   0    1   1    0
        //   1    0   0    0
        //   1    0   1    1
        //   1    1   0    0
        //   1    1   1    0

        if(code==2||code==5) {
            ASSERT(((pcr_bits&1)&&!port->old_c1&&port->c1)||(!(pcr_bits&1)&&port->old_c1&&!port->c1));

            this->ifr.value|=cx2_mask<<1;

            if(latching) {
                port->p_latch=port->p;
            }
        }

        port->old_c1=port->c1;
    }

    /* Do Cx2 */
    {
        auto cx2_control=(R6522Cx2Control)((pcr_bits>>1)&7);
        switch(cx2_control) {
        case R6522Cx2Control_Input_IndIRQNegEdge:
        case R6522Cx2Control_Input_NegEdge:
            if(port->old_c2&&!port->c2) {
                this->ifr.value|=cx2_mask;
            }
            break;

        case R6522Cx2Control_Input_IndIRQPosEdge:
        case R6522Cx2Control_Input_PosEdge:
            if(!port->old_c2&&port->c2) {
                this->ifr.value|=cx2_mask;
            }
            break;

        case R6522Cx2Control_Output_Pulse:
            if(port->pulse>0) {
                --port->pulse;
                if(port->pulse==0) {
                    port->c2=1;
                }
            }
            break;

        case R6522Cx2Control_Output_High:
            port->c2=1;
            break;

        case R6522Cx2Control_Output_Low:
            port->c2=0;
            break;

            /* deliberately leaving this in for the warning, as I
            * haven't done it yet... */
            /* case R6522Cx2Control_Handshake: */
            /*     break; */
        }

        port->old_c2=port->c2;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
