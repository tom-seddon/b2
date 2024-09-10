#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/BeebLink.h>
#include <beeb/6522.h>
#include <shared/load_store.h>
#include <6502/6502.h>
#include <inttypes.h>
#include <beeb/Trace.h>

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

BeebLinkHandler::~BeebLinkHandler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> GetBeebLinkErrorResponsePacketData(uint8_t code,
                                                        const char *message) {
    size_t message_len = strlen(message);
    ASSERT(message_len < UINT32_MAX - 2);

    std::vector<unsigned char> data(1 + 4 + 1 + 1 + message_len + 1);

    data[0] = RESPONSE_ERROR;

    Store32LE(&data[1], (uint32_t)(1 + 1 + message_len + 1));

    data[5] = 0;
    data[6] = code;

    // +1 to copy the terminating 0 as well.
    memcpy(&data[7], message, message_len + 1);

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
        LOGF(BEEBLINK, "Presence check.\n");
        // Presence check.
        TRACEF(b->m_trace, "BeebLink: Write Control: presence check");
        b->SendResponse({0xff, 9, 0, 0, 0, 'B', 'e', 'e', 'b', 'L', 'i', 'n', 'k', 0});
        break;

    case 1:
        // Send data.
        LOGF(BEEBLINK, "Send data (reset FIFO).\n");
        TRACEF(b->m_trace, "BeebLink: Write Control: send data (reset FIFO)");
        b->ResetReceiveBuffer();

    begin_send:
        b->m_send.clear();
        b->m_may_send = true;

        break;

    case 2:
        // Send data, keeping input FIFO.
        LOGF(BEEBLINK, "Send data (retain FIFO).\n");
        TRACEF(b->m_trace, "BeebLink: Write Control: send data (retain FIFO)");
        goto begin_send;
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
            bool is_fire_and_forget = false;
            uint8_t c = b->m_send[0] & 0x7f;
            if (c >= 0x60 && c <= 0x6f) {
                is_fire_and_forget = true;
            }

            LOGF(BEEBLINK, "Request: $%02x (%" PRIu32 ")\n", c, payload_size);

            if (!b->m_handler->GotRequestPacket(std::move(b->m_send), is_fire_and_forget)) {
                b->SendResponse(GetBeebLinkErrorResponsePacketData(255, "GotRequestPacket failed"));
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::SendResponse(std::vector<uint8_t> data) {
    LOGF(BEEBLINK, "%zu response bytes:", data.size());
    size_t i = 0;
    const char *sep = " ";
    while (i < data.size()) {
        if (i + 5 > data.size()) {
            LOGF(BEEBLINK, " (header overrun)");
            data = GetBeebLinkErrorResponsePacketData(255, "Bad response (1)");
            break;
        }

        uint8_t c = data[i + 0];
        uint32_t payload_size = Load32LE(&data[i + 1]);
        if (i + 5 + payload_size > data.size()) {
            LOGF(BEEBLINK, " (payload overrun)");
            data = GetBeebLinkErrorResponsePacketData(255, "Bad response (2)");
            break;
        }

        LOGF(BEEBLINK, "%s$%02x (%" PRIu32 ")", sep, c, payload_size);

        i += (size_t)5 + payload_size;
        sep = "; ";
    }

    LOGF(BEEBLINK, "\n");

    m_recv = std::move(data);
    m_recv_index = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebLink::SetTrace(Trace *trace) {
    m_trace = trace;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLink::ResetReceiveBuffer() {
    m_recv.clear();
    m_recv_index = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
