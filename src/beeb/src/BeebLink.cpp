#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/BeebLink.h>
#include <curl/curl.h>
#include <beeb/6522.h>

#include <shared/enum_def.h>
#include <beeb/BeebLink.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t AVR_PROTOCOL_VERSION=1;

static const uint8_t REQUEST_AVR_PRESENCE=0;
static const uint8_t REQUEST_AVR=1;

static const uint8_t REQUEST_AVR_READY=0;
static const uint8_t REQUEST_AVR_ERROR=1;

//static const uint8_t RESPONSE_NO=1;
static const uint8_t RESPONSE_YES=2;
static const uint8_t RESPONSE_ERROR=4;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(BEEBLINK,"beeblink","BEEBLINK",&log_printer_stdout_and_debugger,false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLinkHandler::~BeebLinkHandler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLink::BeebLink(BeebLinkHandler *handler):
m_handler(handler),
m_state(BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLink::~BeebLink() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::Update(R6522 *via) {
    const BeebLinkState old_state=m_state;

    switch(m_state) {
        case BeebLinkState_None:
            ASSERT(false);
            // fall through
        case BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady:
            this->WaitForBeebReadyReceive(via,BeebLinkState_ReceivePacketHeaderFromBeeb_AckAndCheck,&m_type);
            break;

        case BeebLinkState_ReceivePacketHeaderFromBeeb_AckAndCheck:
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                if(m_type==REQUEST_AVR_PRESENCE) {
                    // AVR presence check. Just ignore.
                    TRACEF(m_trace,"BeebLink: Ignoring REQUEST_AVR_PRESENCE");
                    LOGF(BEEBLINK,"Ignoring REQUEST_AVR_PRESENCE.\n");
                    m_state=BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady;
                } else {
                    LOGF(BEEBLINK,"Command: V=%u, C=%u (0x%02x)\n",
                         m_type&0x80?1:0,m_type&0x7f,m_type&0x7f);
                    TRACEF(m_trace,"BeebLink: Beeb wrote command: V=%u, C=%u (0x%02x)\n",
                           m_type&0x80?1:0,m_type&0x7f,m_type&0x7f);
                    if(m_type&0x80) {
                        m_type&=0x7f;
                        m_state=BeebLinkState_ReceiveSize0FromBeeb_WaitForBeebReady;
                    } else {
                        m_size=1;
                        m_state=BeebLinkState_ReceivePayloadFromBeeb_WaitForBeebReady;
                    }

                    m_payload.clear();
                }
            }
            break;

        case BeebLinkState_ReceiveSize0FromBeeb_WaitForBeebReady:
            this->WaitForBeebReadyReceive(via,BeebLinkState_ReceiveSize0FromBeeb_AckAndCheck,&m_size_bytes[0]);
            break;

        case BeebLinkState_ReceiveSize0FromBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_ReceiveSize1FromBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_ReceiveSize1FromBeeb_WaitForBeebReady:
            this->WaitForBeebReadyReceive(via,BeebLinkState_ReceiveSize1FromBeeb_AckAndCheck,&m_size_bytes[1]);
            break;

        case BeebLinkState_ReceiveSize1FromBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_ReceiveSize2FromBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_ReceiveSize2FromBeeb_WaitForBeebReady:
            this->WaitForBeebReadyReceive(via,BeebLinkState_ReceiveSize2FromBeeb_AckAndCheck,&m_size_bytes[2]);
            break;

        case BeebLinkState_ReceiveSize2FromBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_ReceiveSize3FromBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_ReceiveSize3FromBeeb_WaitForBeebReady:
            this->WaitForBeebReadyReceive(via,BeebLinkState_ReceiveSize3FromBeeb_AckAndCheck,&m_size_bytes[3]);
            break;

        case BeebLinkState_ReceiveSize3FromBeeb_AckAndCheck:
            if(this->AckAndCheck(via,BeebLinkState_ReceivePayloadFromBeeb_WaitForBeebReady)) {
                m_size=((uint32_t)m_size_bytes[3]<<24u|
                        (uint32_t)m_size_bytes[2]<<16u|
                        (uint32_t)m_size_bytes[1]<<8u|
                        m_size_bytes[0]);
            }
            break;

        case BeebLinkState_ReceivePayloadFromBeeb_WaitForBeebReady:
        {
            uint8_t tmp;
            if(this->WaitForBeebReadyReceive(via,BeebLinkState_ReceivePayloadFromBeeb_AckAndCheck,&tmp)) {
                m_payload.push_back(tmp);
            }
        }
            break;

        case BeebLinkState_ReceivePayloadFromBeeb_AckAndCheck:
            ASSERT(m_payload.size()<=m_size);
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                if(m_payload.size()==m_size) {
                    if(m_type==REQUEST_AVR) {
                        this->HandleRequestAVR();
                        m_state=BeebLinkState_SendPacketHeaderToBeeb_WaitForBeebReady;
                    } else {
                        m_state=BeebLinkState_WaitForServerResponse;
                    }
                } else {
                    m_state=BeebLinkState_ReceivePayloadFromBeeb_WaitForBeebReady;
                }
            }
            break;

        case BeebLinkState_WaitForServerResponse:
            // Wait for it...
            break;

        case BeebLinkState_SendPacketHeaderToBeeb_WaitForBeebReady:
        {
            uint8_t type=m_type;
            if(m_payload.size()!=1) {
                type|=0x80;
            }

            this->WaitForBeebReadySend(via,BeebLinkState_SendPacketHeaderToBeeb_AckAndCheck,type);
        }
            break;

        case BeebLinkState_SendPacketHeaderToBeeb_AckAndCheck:
        {
            BeebLinkState ack_state;
            if(m_payload.size()==1) {
                m_index=0;
                ack_state=BeebLinkState_SendPayloadToBeeb_WaitForBeebReady;
            } else {
                m_size_bytes[0]=(uint8_t)m_payload.size();
                m_size_bytes[1]=(uint8_t)(m_payload.size()>>8);
                m_size_bytes[2]=(uint8_t)(m_payload.size()>>16);
                m_size_bytes[3]=(uint8_t)(m_payload.size()>>24);
                ack_state=BeebLinkState_SendSize0ToBeeb_WaitForBeebReady;
            }

            this->AckAndCheck(via,ack_state);
        }
            break;

        case BeebLinkState_SendSize0ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,BeebLinkState_SendSize0ToBeeb_AckAndCheck,(uint8_t)m_payload.size());
            break;

        case BeebLinkState_SendSize0ToBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_SendSize1ToBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_SendSize1ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,BeebLinkState_SendSize0ToBeeb_AckAndCheck,(uint8_t)(m_payload.size()>>8));
            break;

        case BeebLinkState_SendSize1ToBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_SendSize2ToBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_SendSize2ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,BeebLinkState_SendSize0ToBeeb_AckAndCheck,(uint8_t)(m_payload.size()>>16));
            break;

        case BeebLinkState_SendSize2ToBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_SendSize3ToBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_SendSize3ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,BeebLinkState_SendSize0ToBeeb_AckAndCheck,(uint8_t)(m_payload.size()>>24));
            break;

        case BeebLinkState_SendSize3ToBeeb_AckAndCheck:
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                if(m_payload.empty()) {
                    m_state=BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady;
                } else {
                    m_index=0;
                    m_state=BeebLinkState_SendPayloadToBeeb_WaitForBeebReady;
                }
            }
            break;

        case BeebLinkState_SendPayloadToBeeb_WaitForBeebReady:
            ASSERT(m_index<=m_payload.size());
            this->WaitForBeebReadySend(via,BeebLinkState_SendPayloadToBeeb_AckAndCheck,m_payload[m_index]);
            break;

        case BeebLinkState_SendPayloadToBeeb_AckAndCheck:
            ASSERT(m_index<=m_payload.size());
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                ++m_index;
                if(m_index==m_payload.size()) {
                    m_state=BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady;
                } else {
                    m_state=BeebLinkState_SendPayloadToBeeb_WaitForBeebReady;
                }
            }
            break;

        case BeebLinkState_BeebDidNotAck:
            if(via->b.c2) {
                this->Stall();
                m_state=BeebLinkState_Reset;
            }
            break;

        case BeebLinkState_Reset:
            m_state=BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady;
            break;
    }

    if(m_state!=old_state) {
        LOGF(BEEBLINK,"%s -> %s\n",
             GetBeebLinkStateEnumName(old_state),
             GetBeebLinkStateEnumName(m_state));
    }

    TRACEF_IF(m_state!=old_state,m_trace,
              "BeebLink: %s -> %s",
              GetBeebLinkStateEnumName(old_state),
              GetBeebLinkStateEnumName(m_state));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebLink::SetTrace(Trace *trace) {
    m_trace=trace;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLink::WaitForBeebReadyReceive(R6522 *via,
                                       BeebLinkState ready_state,
                                       uint8_t *value)
{
    if(via->b.c2) {
//        if(++m_state_counter==0) {
//            m_state=BeebLinkState_BeebDidNotBecomeReady;
//        }

        // Some kind of timeout message?

        return false;
    } else {
        *value=via->b.p;

        TRACEF(m_trace,"BeebLink: Beeb wrote %03u (0x%02x). CB1=0 (was %u). State now: %s",
               *value,
               *value,
               via->b.c1,
               GetBeebLinkStateEnumName(ready_state));

        via->b.c1=0;//part of ACK_AND_CHECK...

        m_state_counter=0;
        m_state=ready_state;

        LOGF(BEEBLINK,"Received: %03u (0x%02x).\n",*value,*value);
        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLink::AckAndCheck(R6522 *via,BeebLinkState ack_state) {
    if(!via->b.c2) {
        // Some kind of timeout message?

        if(++m_state_counter>255) {
            // Beeb didn't acknowledge in time.
            m_state=BeebLinkState_BeebDidNotAck;

            TRACEF(m_trace,"BeebLink: Beeb didn't acknowledgein time. CB1=1 (was %u). State now: %s",
                   via->b.c1,
                   GetBeebLinkStateEnumName(m_state));

            via->b.c1=1;
        }

        return false;
    } else {
        m_state=ack_state;

        TRACEF(m_trace,"BeebLink: Beeb acknowledged. CB1=1 (was %u). State now: %s",
               via->b.c1,
               GetBeebLinkStateEnumName(m_state));

        via->b.c1=1;

        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLink::WaitForBeebReadySend(R6522 *via,
                                    BeebLinkState ready_state,
                                    uint8_t value)
{
    if(via->b.c2) {
        // Some kind of timeout message?

        return false;
    } else {
        via->b.p=value;

        via->b.c1=0;//part of ACK_AND_CHECK...

        m_state_counter=0;
        m_state=ready_state;

        TRACEF(m_trace,"BeebLink: server wrote %03u (0x%02x). State now: %s",
               value,
               value,
               GetBeebLinkStateEnumName(m_state));

        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::HandleRequestAVR() {
    if(m_payload.size()<1) {
        ErrorResponse(255,"Missing REQUEST_AVR payload");
        return;
    }

    switch(m_payload[0]) {
        case REQUEST_AVR_READY:
            m_type=RESPONSE_YES;
            m_payload.clear();
            m_payload.push_back(AVR_PROTOCOL_VERSION);
            break;

        case REQUEST_AVR_ERROR:
            ErrorResponse(255,"As requested");
            break;

        default:
            ErrorResponse(255,"Bad REQUEST_AVR request");
            return;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::ErrorResponse(uint8_t code,const char *message) {
    m_type=RESPONSE_ERROR;

    m_payload.clear();

    m_payload.push_back(0);
    m_payload.push_back(code);

    for(const char *c=message;*c!=0;++c) {
        m_payload.push_back((uint8_t)*c);
    }

    m_payload.push_back(0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::Stall() {
    //ASSERT(false);
}
