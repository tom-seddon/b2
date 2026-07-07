#include <shared/system.h>
#include <beeb/debug_hardware.h>

#if BBCMICRO_DEBUGGER

#include <6502/6502.h>
#include <shared/debug.h>

#include <shared/enum_def.h>
#include <beeb/debug_hardware.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct DebugCommandBuffersStatusBits {
    uint8_t _ : 6;
    uint8_t command_error : 1;
    uint8_t data_available : 1;
};

union DebugCommandBuffersStatus {
    uint8_t value;
    DebugCommandBuffersStatusBits bits;
};
CHECK_SIZEOF(DebugCommandBuffersStatus, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DebugCommandBuffers::DebugCommandBuffers() {
    this->Reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t DebugCommandBuffers::ReadData(void *context, M6502Word) {
    auto buffers = (DebugCommandBuffers *)context;

    if (buffers->m_to_cpu_index < buffers->m_to_cpu.size()) {
        uint8_t value = buffers->m_to_cpu[buffers->m_to_cpu_index];
        ++buffers->m_to_cpu_index;

        return value;
    } else {
        return 0x00;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t DebugCommandBuffers::ReadStatus(void *context, M6502Word) {
    auto buffers = (DebugCommandBuffers *)context;

    DebugCommandBuffersStatus status = {};

    status.bits.command_error = buffers->m_command_error;
    status.bits.data_available = buffers->m_to_cpu_index < buffers->m_to_cpu.size();

    return status.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugCommandBuffers::WriteData(void *context, M6502Word, uint8_t value) {
    auto buffers = (DebugCommandBuffers *)context;

    buffers->m_from_cpu.push_back(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugCommandBuffers::WriteCommand(void *context, M6502Word, uint8_t value) {
    auto buffers = (DebugCommandBuffers *)context;

    std::vector<uint8_t> from_cpu = std::move(buffers->m_from_cpu);
    buffers->m_from_cpu.clear();

    buffers->m_to_cpu.clear();

    buffers->m_command_error = false;

    switch (value) {
    default:
        buffers->CommandError();
        break;

    case DebugCommand_Reset:
        buffers->Reset();
        break;

    case DebugCommand_EnableSymbolGroup:
        buffers->HandleSymbolGroupCommand(std::move(from_cpu), &DebugCommandHandler::EnableSymbolGroup);
        break;

    case DebugCommand_DisableSymbolGroup:
        buffers->HandleSymbolGroupCommand(std::move(from_cpu), &DebugCommandHandler::DisableSymbolGroup);
        break;

    case DebugCommand_DisableAllSymbolGroups:
        if (buffers->m_handler) {
            for (int i = 0; i < 256; ++i) {
                buffers->m_handler->DisableSymbolGroup((uint8_t)i);
            }
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugCommandBuffers::Reset() {
    m_to_cpu.clear();
    m_to_cpu_index = 0;

    m_from_cpu.clear();

    m_command_error = false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugCommandBuffers::CommandError() {
    this->Reset();

    m_command_error = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugCommandBuffers::SetHandler(DebugCommandHandler *handler) {
    m_handler = handler;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugCommandBuffers::HandleSymbolGroupCommand(std::vector<uint8_t> &&args, void (DebugCommandHandler::*handle_command_mfn)(uint8_t)) {
    if (args.size() != 1) {
        this->CommandError();
        return;
    }

    if (m_handler) {
        uint8_t group = args[0];
        (m_handler->*handle_command_mfn)(group);
    }
}

#endif
