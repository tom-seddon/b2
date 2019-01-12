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
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint16_t SN76489::NOISE0=0x10;
const uint16_t SN76489::NOISE1=0x20;
const uint16_t SN76489::NOISE2=0x40;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SN76489::SN76489() {
    this->Reset(true);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SN76489::Reset(bool tone) {
    m_state=State();
    m_state.noise_seed=1<<14;

    for(size_t i=0;i<4;++i) {
        Channel *c=&m_state.channels[i];

        c->freq=1023;
        c->vol=tone?15:0;

        memset(&c->output,0xff,sizeof c->output);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

#define TRACE_EVENT(REG,VALUE) \
BEGIN_MACRO {\
if(m_trace) {\
auto ev=(WriteEvent *)m_trace->AllocEvent(WRITE_EVENT);\
ev->reg=(REG);\
ev->value=(VALUE);\
}\
} END_MACRO

#else

#define TRACE_EVENT(REG,VALUE) ((void)0)

#endif

SN76489::Output SN76489::Update(bool write,uint8_t value) {
    Output output;

    // Tone channels
    for(size_t i=0;i<3;++i) {
        Channel *channel=&m_state.channels[i];

        if(channel->freq==1) {
            output.ch[i]=(int8_t)channel->vol;
        } else {
            output.ch[i]=channel->vol*channel->output.tone.mul;

            if(channel->counter>0) {
                --channel->counter;
            }

            if(channel->counter==0) {
                channel->output.tone.mul=-channel->output.tone.mul;

                channel->counter=channel->freq;
                if(channel->counter==0) {
                    channel->counter=1024;
                }
            }
        }
        ASSERT(output.ch[i]>=-15&&output.ch[i]<=15);
    }

    // Noise channel
    {
        Channel *channel=&m_state.channels[3];

        if(channel->counter>0) {
            --channel->counter;
        }

        if(channel->counter==0) {
            channel->output.noise.toggle=!channel->output.noise.toggle;

            if(channel->output.noise.toggle) {
                if(channel->freq&4) {
                    // White noise
                    channel->output.noise.value=this->NextWhiteNoiseBit();
                } else {
                    // Periodic noise
                    channel->output.noise.value=this->NextPeriodicNoiseBit();
                }
            }

            channel->counter=*m_noise_pointers[channel->freq&3];
            if(channel->counter==0) {
                channel->counter=1024;
            }
        }

        output.ch[3]=channel->vol*(int8_t)channel->output.noise.value;
        ASSERT(output.ch[3]>=-15&&output.ch[3]<=15);
    }

    if(write) {
        if(m_state.write_delay>0) {
            // write is lost, presumably?
        } else {
            m_state.write_delay=2;

            uint8_t latch_data=value&0x80;

            if(latch_data) {
                m_state.reg=value>>5&3;
            }

            if(value&0x80) {
                // Latch/data byte

                m_state.reg=value>>4&7;
                uint8_t v=value&0xf;

                Channel *channel=&m_state.channels[m_state.reg>>1];

                if(m_state.reg&1) {
                    // volume
                    channel->vol=v^0xf;
                    TRACE_EVENT(m_state.reg,channel->vol);
                } else {
                    // data
                    channel->freq&=~0xf;
                    channel->freq|=v;
                    TRACE_EVENT(m_state.reg,channel->freq);
                }
            } else {
                Channel *channel=&m_state.channels[m_state.reg>>1];
                uint8_t v=value&0x3f;

                // Data byte
                if(m_state.reg&1) {
                    // volume
                    channel->vol=(v&0xf)^0xf;
                    TRACE_EVENT(m_state.reg,channel->vol);
                } else if(m_state.reg==3<<1) {
                    // noise data
                    channel->freq=v;
                    m_state.noise_seed=1<<14;
                    TRACE_EVENT(m_state.reg,channel->freq);
                } else {
                    // tone data
                    channel->freq&=0xf;
                    channel->freq|=v<<4;
                    TRACE_EVENT(m_state.reg,channel->freq);
                }
            }
        }
    }

    if(m_state.write_delay>0) {
        --m_state.write_delay;
    }

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

// http://www.zeridajh.org/articles/various/sn76489/index.htm
uint8_t SN76489::NextWhiteNoiseBit() {
    uint8_t feed_bit=((m_state.noise_seed>>1)^m_state.noise_seed)&1;
    m_state.noise_seed=(m_state.noise_seed&32767)>>1|feed_bit<<14;
    return (m_state.noise_seed&1)^1;
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
