#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include "BeebLink.h"
#include <curl/curl.h>

#include <shared/enum_def.h>
#include "BeebLink.inl"
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

BeebLink::BeebLink() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLink::~BeebLink() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::Reset() {
    m_payload.clear();
    m_state=BeebLinkState_BeebToServerType;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int BeebLink::HandleWrite(uint8_t value) {
    int avr;

    LOGF(BEEBLINK,"BBC wrote: %02x (%03u)",value,value);
    if(value>=32&&value<127) {
        LOGF(BEEBLINK," ('%c')",(char)value);
    }
    LOGF(BEEBLINK,"\n");

    switch(m_state) {
        case BeebLinkState_BeebToServerType:
            if(value==REQUEST_AVR_PRESENCE) {
                // Do nothing.
            } else {
                m_type=value;
                m_payload.clear();
                if(m_type&0x80) {
                    m_type&=0x7f;
                    m_size=0;
                    m_state=BeebLinkState_BeebToServerSize0;
                } else {
                    m_size=1;
                    m_state=BeebLinkState_BeebToServerPayload;
                }
            }
            avr=-1;
            break;

        case BeebLinkState_BeebToServerSize0:
            m_size|=value;
            m_state=BeebLinkState_BeebToServerSize1;
            avr=-1;
            break;

        case BeebLinkState_BeebToServerSize1:
            m_size|=(uint32_t)value<<8;
            m_state=BeebLinkState_BeebToServerSize2;
            avr=-1;
            break;

        case BeebLinkState_BeebToServerSize2:
            m_size|=(uint32_t)value<<16;
            m_state=BeebLinkState_BeebToServerSize3;
            avr=-1;
            break;

        case BeebLinkState_BeebToServerSize3:
            m_size|=(uint32_t)value<<24;
            if(m_size==0) {
                avr=this->HandleBeebToServerPayload();
            } else {
                m_state=BeebLinkState_BeebToServerPayload;
                avr=-1;
            }
            break;

        case BeebLinkState_BeebToServerPayload:
            ASSERT(m_payload.size()<m_size);
            m_payload.push_back(value);
            if(m_payload.size()==m_size) {
                avr=this->HandleBeebToServerPayload();
            } else {
                avr=-1;
            }
            break;

        case BeebLinkState_ServerToBeebSize0:
            avr=m_payload.size()&0xff;
            m_state=BeebLinkState_ServerToBeebSize1;
            break;

        case BeebLinkState_ServerToBeebSize1:
            avr=m_payload.size()>>8&0xff;
            m_state=BeebLinkState_ServerToBeebSize2;
            break;

        case BeebLinkState_ServerToBeebSize2:
            avr=m_payload.size()>>16&0xff;
            m_state=BeebLinkState_ServerToBeebSize3;
            break;

        case BeebLinkState_ServerToBeebSize3:
            avr=m_payload.size()>>24&0xff;
            if(m_payload.empty()) {
                m_state=BeebLinkState_BeebToServerType;
            } else {
                m_state=BeebLinkState_ServerToBeebPayload;
            }
            break;

        case BeebLinkState_ServerToBeebPayload:
            ASSERT(m_index<m_payload.size());
            avr=m_payload[m_index++];
            if(m_index==m_payload.size()) {
                m_state=BeebLinkState_BeebToServerType;
            }
            break;
    }

    return avr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int BeebLink::HandleBeebToServerPayload() {
    if(m_type==REQUEST_AVR) {
        this->HandleRequestAVR();
    } else {
        ASSERT(false);
    }

    ASSERT(!(m_type&0x80));
    m_index=0;

    if(m_payload.size()==1) {
        m_state=BeebLinkState_ServerToBeebPayload;
        return m_type;
    } else {
        m_state=BeebLinkState_ServerToBeebSize0;
        return m_type|0x80;
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
