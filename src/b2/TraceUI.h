#ifndef HEADER_A9C1EAC5B2784A7D813F621DAEC22558 // -*- mode:c++ -*-
#define HEADER_A9C1EAC5B2784A7D813F621DAEC22558

#include "conf.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>

#include <beeb/BBCMicro.h>
#include <beeb/SaveTrace.h>

#include <shared/json.h>

#include <shared/enum_decl.h>
#include "TraceUI.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This stuff exists even when tracing is compiled out, so that the
// settings can still be serialized.

struct TraceUISettings {
    // Start condition and any arguments.
    Enum<TraceUIStartCondition> start{TraceUIStartCondition_Now};
    uint16_t start_instruction_address = 0;
    uint16_t start_write_address = 0;

    // Stop condition and any arguments.
    Enum<TraceUIStopCondition> stop{TraceUIStopCondition_ByRequest};
    uint64_t stop_num_2MHz_cycles = 0;
    uint16_t stop_write_address = 0;

    // Other stuff.
    EnumFlags<BBCMicroTraceFlag> flags{0};
    bool unlimited = false;

    // Only relevant on Windows.
    bool unix_line_endings = false;

    EnumFlags<TraceOutputFlags> output_flags{DEFAULT_TRACE_OUTPUT_FLAGS};

    // Auto-save settings.
    bool auto_save = false;
    std::string auto_save_path;

    // UI stuff.
    bool is_other_traces_ui_visible = true;
};
JSON_SERIALIZE(TraceUISettings,
               start, start_instruction_address, start_write_address,
               stop, stop_num_2MHz_cycles, stop_write_address,
               flags, unlimited,
               unix_line_endings,
               output_flags,
               auto_save, auto_save_path,
               is_other_traces_ui_visible);

TraceUISettings GetDefaultTraceUISettings();
void SetDefaultTraceUISettings(const TraceUISettings &settings);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>

class BeebWindow;
class SettingsUI;

// If tracing support is compiled out, returns nullptr.
std::unique_ptr<SettingsUI> CreateTraceUI(BeebWindow *beeb_window);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
