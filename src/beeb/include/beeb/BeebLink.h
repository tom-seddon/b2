#ifndef HEADER_85E7DE82F7DF4FF7AD8A330E6261A2E0 // -*- mode:c++ -*-
#define HEADER_85E7DE82F7DF4FF7AD8A330E6261A2E0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <vector>
#include "Trace.h"

class R6522;
union M6502Word;

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

std::vector<uint8_t> GetBeebLinkErrorResponsePacketData(uint8_t code, const char *message);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebLink {
  public:
    explicit BeebLink(BeebLinkHandler *handler);
    ~BeebLink();

    static uint8_t ReadControl(void *context, M6502Word addr);
    static void WriteControl(void *context, M6502Word addr, uint8_t value);
    static uint8_t ReadData(void *context, M6502Word addr);
    static void WriteData(void *context, M6502Word addr, uint8_t value);

    void SendResponse(std::vector<uint8_t> data);

#if BBCMICRO_TRACE
    void SetTrace(Trace *trace);
#endif
  protected:
  private:
    BeebLinkHandler *const m_handler = nullptr;

    std::vector<unsigned char> m_recv;
    uint64_t m_recv_index = 0;

    bool m_may_send = false;
    std::vector<unsigned char> m_send;

#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
