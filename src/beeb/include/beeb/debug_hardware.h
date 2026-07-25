#ifndef HEADER_A22E0D2888D540F4BD00CA717BCB348D // -*- mode:c++ -*-
#define HEADER_A22E0D2888D540F4BD00CA717BCB348D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include <vector>

union M6502Word;
class BBCMicro;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

#include <shared/enum_decl.h>
#include "debug_hardware.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Actually handling the commands is the caller's responsibility.
class DebugCommandHandler {
  public:
    DebugCommandHandler() = default;
    virtual ~DebugCommandHandler() = default;

    virtual void SetSymbolGroupEnabled(uint8_t group, bool enabled) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DebugCommandBuffers {
  public:
    DebugCommandBuffers();

    static uint8_t ReadData(void *context, M6502Word addr);
    static uint8_t ReadStatus(void *context, M6502Word addr);
    static void WriteData(void *context, M6502Word addr, uint8_t value);
    static void WriteCommand(void *context, M6502Word addr, uint8_t value);

    void Reset();
    void CommandError();

    void SetHandler(DebugCommandHandler *handler);

  protected:
  private:
    std::vector<uint8_t> m_to_cpu;
    size_t m_to_cpu_index = 0;
    std::vector<uint8_t> m_from_cpu;
    bool m_command_error = false;
    DebugCommandHandler *m_handler = nullptr;

    void HandleSymbolGroupCommand(std::vector<uint8_t> &&args, bool enabled);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
