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

std::vector<uint8_t> BeebLink::GetErrorResponsePacketData(uint8_t code,
                                                          const char *message)
{
    std::vector<uint8_t> data;

    data.push_back(RESPONSE_ERROR);

    data.push_back(0);
    data.push_back(code);

    for(const char *c=message;*c!=0;++c) {
        data.push_back((uint8_t)*c);
    }

    data.push_back(0);

    return data;
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
            this->WaitForBeebReadyReceive(via,
                                          BeebLinkState_ReceivePacketHeaderFromBeeb_AckAndCheck,
                                          &m_type_byte);
            break;

        case BeebLinkState_ReceivePacketHeaderFromBeeb_AckAndCheck:
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                if(m_type_byte==REQUEST_AVR_PRESENCE) {
                    // AVR presence check. Just ignore.
                    TRACEF(m_trace,"BeebLink: Ignoring REQUEST_AVR_PRESENCE");
                    LOGF(BEEBLINK,"Ignoring REQUEST_AVR_PRESENCE.\n");
                    m_state=BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady;
                } else {
                    LOGF(BEEBLINK,"Command: V=%u, C=%u (0x%02x)\n",
                         m_type_byte&0x80?1:0,m_type_byte&0x7f,m_type_byte&0x7f);
                    TRACEF(m_trace,"BeebLink: Beeb wrote command: V=%u, C=%u (0x%02x)\n",
                           m_type_byte&0x80?1:0,m_type_byte&0x7f,m_type_byte&0x7f);

                    if(m_type_byte&0x80) {
                        m_state=BeebLinkState_ReceiveSize0FromBeeb_WaitForBeebReady;
                    } else {
                        m_data.resize(2);
                        m_data[0]=m_type_byte;
                        m_index=1;
                        m_state=BeebLinkState_ReceivePayloadFromBeeb_WaitForBeebReady;
                    }
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
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                uint32_t size=((uint32_t)m_size_bytes[3]<<24u|
                               (uint32_t)m_size_bytes[2]<<16u|
                               (uint32_t)m_size_bytes[1]<<8u|
                               m_size_bytes[0]);
                m_data.resize(1+size);
                m_data[0]=m_type_byte;

                if(size==0) {
                    this->HandleReceivedData();
                    ASSERT(m_state!=BeebLinkState_None);
                } else {
                    m_index=1;
                    m_state=BeebLinkState_ReceivePayloadFromBeeb_WaitForBeebReady;
                }
            }
            break;

        case BeebLinkState_ReceivePayloadFromBeeb_WaitForBeebReady:
        {
            ASSERT(m_index<m_data.size());
            this->WaitForBeebReadyReceive(via,
                                          BeebLinkState_ReceivePayloadFromBeeb_AckAndCheck,
                                          &m_data[m_index]);
        }
            break;

        case BeebLinkState_ReceivePayloadFromBeeb_AckAndCheck:
            ASSERT(m_index<m_data.size());
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                ++m_index;
                if(m_index==m_data.size()) {
                    this->HandleReceivedData();
                    ASSERT(m_state!=BeebLinkState_None);
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
            ASSERT(!m_data.empty());

            {
                LOGF(BEEBLINK,"To Beeb: ");
                LOGI(BEEBLINK);
                LogDumpBytes(&LOG(BEEBLINK),m_data.data(),m_data.size());
            }

            m_state=BeebLinkState_SendPacketHeaderToBeeb_WaitForBeebReadyLoop;
        }
            // fall through
        case BeebLinkState_SendPacketHeaderToBeeb_WaitForBeebReadyLoop:
        {
            ASSERT(!m_data.empty());

            uint8_t type=m_data[0];
            if(m_data.size()!=2) {
                type|=0x80;
            }

            this->WaitForBeebReadySend(via,BeebLinkState_SendPacketHeaderToBeeb_AckAndCheck,type);
        }
            break;

        case BeebLinkState_SendPacketHeaderToBeeb_AckAndCheck:
        {
            ASSERT(!m_data.empty());

            BeebLinkState ack_state;
            if(m_data.size()==2) {
                m_index=1;
                ack_state=BeebLinkState_SendPayloadToBeeb_WaitForBeebReady;
            } else {
                ASSERT(m_data.size()-1<=UINT32_MAX);
                uint32_t size=(uint32_t)(m_data.size()-1);
                m_size_bytes[0]=(uint8_t)size;
                m_size_bytes[1]=(uint8_t)(size>>8);
                m_size_bytes[2]=(uint8_t)(size>>16);
                m_size_bytes[3]=(uint8_t)(size>>24);
                ack_state=BeebLinkState_SendSize0ToBeeb_WaitForBeebReady;
            }

            this->AckAndCheck(via,ack_state);
        }
            break;

        case BeebLinkState_SendSize0ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,
                                       BeebLinkState_SendSize0ToBeeb_AckAndCheck,
                                       m_size_bytes[0]);
            break;

        case BeebLinkState_SendSize0ToBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_SendSize1ToBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_SendSize1ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,
                                       BeebLinkState_SendSize1ToBeeb_AckAndCheck,
                                       m_size_bytes[1]);
            break;

        case BeebLinkState_SendSize1ToBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_SendSize2ToBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_SendSize2ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,
                                       BeebLinkState_SendSize2ToBeeb_AckAndCheck,
                                       m_size_bytes[2]);
            break;

        case BeebLinkState_SendSize2ToBeeb_AckAndCheck:
            this->AckAndCheck(via,BeebLinkState_SendSize3ToBeeb_WaitForBeebReady);
            break;

        case BeebLinkState_SendSize3ToBeeb_WaitForBeebReady:
            this->WaitForBeebReadySend(via,
                                       BeebLinkState_SendSize3ToBeeb_AckAndCheck,
                                       m_size_bytes[3]);
            break;

        case BeebLinkState_SendSize3ToBeeb_AckAndCheck:
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                ASSERT(!m_data.empty());
                if(m_data.size()==1) {
                    m_state=BeebLinkState_ReceivePacketHeaderFromBeeb_WaitForBeebReady;
                } else {
                    m_index=1;
                    m_state=BeebLinkState_SendPayloadToBeeb_WaitForBeebReady;
                }
            }
            break;

        case BeebLinkState_SendPayloadToBeeb_WaitForBeebReady:
            ASSERT(m_index>0&&m_index<m_data.size());
            if(this->WaitForBeebReadySend(via,
                                          BeebLinkState_SendPayloadToBeeb_AckAndCheck,
                                          m_data[m_index]))
            {
                LOGF(BEEBLINK,"Sent to Beeb: 0x%02x %zu\n",m_data[m_index],m_index);
            }
            break;

        case BeebLinkState_SendPayloadToBeeb_AckAndCheck:
            ASSERT(m_index>0&&m_index<m_data.size());
            if(this->AckAndCheck(via,BeebLinkState_None)) {
                ++m_index;
                if(m_index==m_data.size()) {
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

void BeebLink::SendResponse(std::vector<uint8_t> data) {
    ASSERT(m_state==BeebLinkState_WaitForServerResponse);
    m_data=std::move(data);
    m_state=BeebLinkState_SendPacketHeaderToBeeb_WaitForBeebReady;
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

        LOGF(BEEBLINK,"Server wrote %03u (0x%02x) ('%c'). State now: %s\n",
             value,
             value,
             value>=32&&value<127?(char)value:'?',
             GetBeebLinkStateEnumName(m_state));
        TRACEF(m_trace,"BeebLink: server wrote %03u (0x%02x). State now: %s",
               value,
               value,
               GetBeebLinkStateEnumName(m_state));

        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::HandleReceivedData() {
    {
        LOGF(BEEBLINK,"From Beeb: ");
        LOGI(BEEBLINK);
        LogDumpBytes(&LOG(BEEBLINK),m_data.data(),m_data.size());
    }

    if((m_data[0]&0x7f)==REQUEST_AVR) {
        if(m_data.size()!=2) {
            m_data=GetErrorResponsePacketData(255,
                                              "Invaid REQUEST_AVR payload");
        } else {
            switch(m_data[1]) {
                case REQUEST_AVR_READY:
                    m_data={RESPONSE_YES,AVR_PROTOCOL_VERSION};
                    break;

                case REQUEST_AVR_ERROR:
                    m_data=GetErrorResponsePacketData(255,
                                                      "As requested");
                    break;

                default:
                    m_data=GetErrorResponsePacketData(255,
                                                      "Bad REQUEST_AVR request");
                    return;
            }
        }

        m_state=BeebLinkState_SendPacketHeaderToBeeb_WaitForBeebReady;
    } else {
        if(!m_handler->GotRequestPacket(std::move(m_data))) {
            m_data=GetErrorResponsePacketData(255,"GotRequestPacket failed");
            m_state=BeebLinkState_SendPacketHeaderToBeeb_WaitForBeebReady;
        } else {
            m_state=BeebLinkState_WaitForServerResponse;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::Stall() {
    //ASSERT(false);
}
