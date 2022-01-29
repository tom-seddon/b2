#ifndef HEADER_85E7DE82F7DF4FF7AD8A330E6261A2E0 // -*- mode:c++ -*-
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
// Packets are vectors of 1+ byte(s), roughly following the AVR<->PC format:
// first byte is the packet type, then the payload, if any.
//
// Packet type bit 7 may or may not be set - payload size is never included
// in either case, as it can be inferred from the size of the vector.
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
    BeebLinkHandler() = default;
    virtual ~BeebLinkHandler();

    virtual bool GotRequestPacket(std::vector<uint8_t> data) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebLink {
  public:
    static std::vector<uint8_t> GetErrorResponsePacketData(uint8_t code,
                                                           const char *message);

    explicit BeebLink(BeebLinkHandler *handler);
    ~BeebLink();

    void Update(R6522 *via);

    void SendResponse(std::vector<uint8_t> data);

#if BBCMICRO_TRACE
    void SetTrace(Trace *trace);
#endif
  protected:
  private:
    BeebLinkHandler *m_handler = nullptr;

    BeebLinkState m_state;
    uint32_t m_state_counter;

    std::vector<uint8_t> m_data;

    size_t m_index = 0;

    uint8_t m_type_byte = 0;
    uint8_t m_size_bytes[4] = {};

#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
#endif

    bool WaitForBeebReadyReceive(R6522 *via, BeebLinkState ready_state, uint8_t *value);
    bool WaitForBeebReadySend(R6522 *via, BeebLinkState ready_state, uint8_t value);
    bool AckAndCheck(R6522 *via, BeebLinkState ack_state);

    // Return value is first byte of response.
    void HandleReceivedData();
    void Stall();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
