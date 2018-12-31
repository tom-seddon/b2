#ifndef HEADER_85E7DE82F7DF4FF7AD8A330E6261A2E0// -*- mode:c++ -*-
#define HEADER_85E7DE82F7DF4FF7AD8A330E6261A2E0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <vector>
#include "Trace.h"

class R6522;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// "BeebLink" is not two words.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "BeebLink.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebLinkHandler {
public:
    BeebLinkHandler()=default;
    virtual ~BeebLinkHandler();

    virtual void GotRequestPacket(uint8_t type,
                                  const std::vector<uint8_t> &payload)=0;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebLink {
public:
    explicit BeebLink(BeebLinkHandler *handler);
    ~BeebLink();

    void Update(R6522 *via);

#if BBCMICRO_TRACE
    void SetTrace(Trace *trace);
#endif
protected:
private:
    BeebLinkHandler *m_handler=nullptr;

    BeebLinkState m_state;
    BeebLinkState m_next_state;
    uint32_t m_state_counter;

    uint8_t m_type;
    uint8_t m_size_bytes[4];
    uint32_t m_size;
    std::vector<uint8_t> m_payload;
    size_t m_index;

#if BBCMICRO_TRACE
    Trace *m_trace=nullptr;
#endif

    bool WaitForBeebReadyReceive(R6522 *via,BeebLinkState ready_state,uint8_t *value);
    bool WaitForBeebReadySend(R6522 *via,BeebLinkState ready_state,uint8_t value);
    bool AckAndCheck(R6522 *via,BeebLinkState ack_state);

    // Return value is first byte of response.
    uint8_t HandleBeebToServerPayload();
    void HandleRequestAVR();
    void ErrorResponse(uint8_t code,const char *message);
    void Stall();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
