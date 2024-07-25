#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/BeebLink.h>
#include <beeb/6522.h>
#include <shared/load_store.h>
#include <6502/6502.h>
#include <inttypes.h>

#include <shared/enum_def.h>
#include <beeb/BeebLink.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t RESPONSE_ERROR = 4;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(BEEBLINK, "beeblink", "BEEBLINK", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The requests and responses have the size in bytes 1/2/3/4, which is the
// format for the planned version 2 HTTP API but currently requires a bit of
// fiddling around.
//
// This will get tidied up!

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLinkHandler::~BeebLinkHandler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> GetBeebLinkErrorResponsePacketData(uint8_t code,
                                                        const char *message) {
    std::vector<uint8_t> data;

    data.push_back(RESPONSE_ERROR);

    data.push_back(0);
    data.push_back(code);

    for (const char *c = message; *c != 0; ++c) {
        data.push_back((uint8_t)*c);
    }

    data.push_back(0);

    return data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLink::BeebLink(BeebLinkHandler *handler)
    : m_handler(handler) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLink::~BeebLink() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BeebLink::ReadControl(void *context, M6502Word addr) {
    (void)addr;
    const auto b = (BeebLink *)context;

    TRACEF(b->m_trace, "BeebLink: Read Control: m_recv_index=%" PRId64, b->m_recv_index);
    if (b->m_recv_index < b->m_recv.size()) {
        return 0x80;
    } else {
        ASSERT(b->m_recv_index == b->m_recv.size());
        return 0x00;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::WriteControl(void *context, M6502Word addr, uint8_t value) {
    (void)addr;
    const auto b = (BeebLink *)context;

    switch (value) {
    case 0:
        // Presence check.
        TRACEF(b->m_trace, "BeebLink: Write Control: presence check");
        b->SendResponse({0xff, 'B', 'e', 'e', 'b', 'L', 'i', 'n', 'k', 0});
        break;

    case 1:
        // Send data.
        TRACEF(b->m_trace, "BeebLink: Write Control: send data");
        b->m_may_send = true;

        // always book space for the request type
        b->m_send.clear();
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BeebLink::ReadData(void *context, M6502Word addr) {
    (void)addr;
    const auto b = (BeebLink *)context;

    TRACEF(b->m_trace, "BeebLink: Read Data: m_recv_index=%" PRId64 ", m_recv.size()=%zu", b->m_recv_index, b->m_recv.size());
    if (b->m_recv_index < b->m_recv.size()) {
        return b->m_recv[b->m_recv_index++];
    } else {
        ASSERT(b->m_recv_index == b->m_recv.size());
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::WriteData(void *context, M6502Word addr, uint8_t value) {
    (void)addr;
    const auto b = (BeebLink *)context;

    if (!b->m_may_send) {
        return;
    }

    b->m_send.push_back(value);
    if (b->m_send.size() >= 5) {
        uint32_t payload_size = Load32LE(&b->m_send[1]);

        ASSERT(b->m_send.size() - 5 <= payload_size);
        if (b->m_send.size() - 5 == payload_size) {
            b->m_send.erase(b->m_send.begin() + 1, b->m_send.begin() + 5);

            if (!b->m_handler->GotRequestPacket(std::move(b->m_send))) {
                b->SendResponse(GetBeebLinkErrorResponsePacketData(255, "GotRequestPacket failed"));
            } else {
                b->m_recv.clear();
                b->m_recv_index = 0;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::SendResponse(std::vector<uint8_t> data) {
    if (data.size() - 1 > UINT32_MAX) {
        data = GetBeebLinkErrorResponsePacketData(255, "Response too large");
    }

    m_recv.resize(5);
    m_recv[0] = data[0];
    Store32LE(&m_recv[1], (uint32_t)(data.size() - 1));
    m_recv.insert(m_recv.end(), data.begin() + 1, data.end());
    m_recv_index = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebLink::SetTrace(Trace *trace) {
    m_trace = trace;
}
#endif
