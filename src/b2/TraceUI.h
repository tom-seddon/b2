#ifndef HEADER_A9C1EAC5B2784A7D813F621DAEC22558 // -*- mode:c++ -*-
#define HEADER_A9C1EAC5B2784A7D813F621DAEC22558

#include "conf.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#include <beeb/Trace.h>
#include <beeb/SaveTrace.h>

#include <shared/enum_decl.h>
#include "TraceUI.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This stuff exists even when tracing is compiled out, so that the
// settings can still be serialized.

struct TraceUISettings {
    // Start condition and any arguments.
    TraceUIStartCondition start = TraceUIStartCondition_Now;
    uint16_t start_instruction_address = 0;
    uint16_t start_write_address = 0;

    // Stop condition and any arguments.
    TraceUIStopCondition stop = TraceUIStopCondition_ByRequest;
    CycleCount stop_num_cycles = {0};
    uint16_t stop_write_address = 0;

    // Other stuff.
    uint32_t flags = 0;
    bool unlimited = false;
    TraceCyclesOutput cycles_output = TraceCyclesOutput_Relative;
};

TraceUISettings GetDefaultTraceUISettings();
void SetDefaultTraceUISettings(const TraceUISettings &settings);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

#include <memory>

class BeebWindow;
class SettingsUI;

std::unique_ptr<SettingsUI> CreateTraceUI(BeebWindow *beeb_window);

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
