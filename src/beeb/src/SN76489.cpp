#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/SN76489.h>
#include <string.h>
#include <stdio.h>
#include <beeb/Trace.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
const TraceEventType SN76489::WRITE_EVENT("SN76489WriteEvent",sizeof(WriteEvent));
const TraceEventType SN76489::UPDATE_EVENT("SN76489UpdateEvent",sizeof(UpdateEvent));
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SN76489::SN76489() {
    this->Reset(true);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SN76489::Reset(bool tone) {
    m_state=State();

    if(!tone) {
        for(size_t i=0;i<4;++i) {
            m_state.channels[i].values.vol=0;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

#define TRACE_EVENT(VALUE) \
BEGIN_MACRO {\
if(m_trace) {\
auto ev=(WriteEvent *)m_trace->AllocEvent(WRITE_EVENT);\
ev->write_value=value;\
ev->reg=m_state.reg;\
ev->reg_value=(VALUE);\
}\
} END_MACRO

#else

#define TRACE_EVENT(VALUE) ((void)0)

#endif

SN76489::Output SN76489::Update(bool write,uint8_t value) {
    Output output;

    // Tone channels
    for(size_t i=0;i<3;++i) {
        Channel *channel=&m_state.channels[i];

        output.ch[i]=channel->values.vol&channel->mask;

        if(channel->counter>0) {
            --channel->counter;
        }

        if(channel->counter==0) {
            channel->mask=~channel->mask;

            channel->counter=channel->values.freq;
            if(channel->counter==0) {
                channel->counter=1024;
            }
        }

        ASSERT(output.ch[i]<=15);
    }

    // Noise channel
    {
        Channel *channel=&m_state.channels[3];

        if(channel->counter>0) {
            --channel->counter;
        }

        if(channel->counter==0) {
            m_state.noise_toggle=!m_state.noise_toggle;

            if(m_state.noise_toggle) {
                if(channel->values.freq&4) {
                    // White noise
                    channel->mask=this->NextWhiteNoiseBit()?0xff:0x00;
                } else {
                    // Periodic noise
                    channel->mask=this->NextPeriodicNoiseBit()?0xff:0x00;
                }
            }

            switch(channel->values.freq&3) {
            case 3:
                channel->counter=m_state.channels[2].values.freq;

                if(channel->counter==0) {
                    channel->counter=1024;
                }
                break;

            case 2:
                channel->counter=0x40;
                break;

            case 1:
                channel->counter=0x20;
                break;

            case 0:
                channel->counter=0x10;
                break;
            }
        }

        output.ch[3]=channel->values.vol&channel->mask;
        ASSERT(output.ch[3]<=15);
    }

    if(write) {
        if(value&0x80) {
            // Latch/data byte

            m_state.reg=value>>4&7;
            uint8_t v=value&0xf;

            Channel *channel=&m_state.channels[m_state.reg>>1];

            if(m_state.reg&1) {
                // volume
                channel->values.vol=v^0xf;
                TRACE_EVENT(channel->values.vol);
            } else {
                // data
                channel->values.freq&=~0xf;
                channel->values.freq|=v;
                TRACE_EVENT(channel->values.freq);

                if(m_state.reg==3<<1) {
                    // noise data
                    m_state.noise_seed=1<<14;
                }
            }
        } else {
            Channel *channel=&m_state.channels[m_state.reg>>1];
            uint8_t v=value&0x3f;

            // Data byte
            if(m_state.reg&1) {
                // volume
                channel->values.vol=(v&0xf)^0xf;
                TRACE_EVENT(channel->values.vol);
            } else if(m_state.reg==3<<1) {
                // noise data
                channel->values.freq=v;
                m_state.noise_seed=1<<14;
                TRACE_EVENT(channel->values.freq);
            } else {
                // tone data
                channel->values.freq&=0xf;
                channel->values.freq|=v<<4;
                TRACE_EVENT(channel->values.freq);
            }
        }
    }

#if BBCMICRO_TRACE

    // Not generally advisable, but it's here when I need it.

//    if(m_trace) {
//        auto ev=(UpdateEvent *)m_trace->AllocEvent(UPDATE_EVENT);
//
//        ev->state=m_state;
//        ev->output=output;
//    }

#endif

    return output;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void SN76489::SetTrace(Trace *t) {
    m_trace=t;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SN76489::GetState(ChannelValues *channels,uint16_t *noise_seed) const {
    channels[0]=m_state.channels[0].values;
    channels[1]=m_state.channels[1].values;
    channels[2]=m_state.channels[2].values;
    channels[3]=m_state.channels[3].values;
    *noise_seed=m_state.noise_seed;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://www.zeridajh.org/articles/various/sn76489/index.htm
uint8_t SN76489::NextWhiteNoiseBit() {
    uint8_t feed_bit=((m_state.noise_seed>>1)^m_state.noise_seed)&1;
    m_state.noise_seed=(m_state.noise_seed&32767)>>1|feed_bit<<14;
    return m_state.noise_seed&1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t SN76489::NextPeriodicNoiseBit() {
    uint8_t result=m_state.noise_seed&1;
    m_state.noise_seed=((m_state.noise_seed>>1)|(m_state.noise_seed<<14))&0x7fff;
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
