#ifndef HEADER_A9C1EAC5B2784A7D813F621DAEC22558// -*- mode:c++ -*-
#define HEADER_A9C1EAC5B2784A7D813F621DAEC22558

#include "conf.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The TraceUI class exists in some (unused, non-functional) form even
// when tracing is compiled out, so that the TraceUI::Settings stuff
// can get serialized.
//
// This wants revisiting, as it's rather ugly.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebWindow;
class MessageList;
class Trace;

#include "conf.h"

#include <memory>
#include <vector>

#include <shared/enum_decl.h>
#include "TraceUI.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TraceUI {
public:
    struct Settings {
        TraceUIStartCondition start=TraceUIStartCondition_Now;
        TraceUIStopCondition stop=TraceUIStopCondition_ByRequest;
        uint32_t flags=0;
    };

    static Settings GetDefaultSettings();
    static void SetDefaultSettings(const Settings &settings);

#if BBCMICRO_TRACE
    TraceUI(BeebWindow *beeb_window);

    void DoImGui();
    bool DoClose();
protected:
private:
    class SaveTraceJob;
    struct Key;

    BeebWindow *m_beeb_window=nullptr;
    
    std::shared_ptr<SaveTraceJob> m_save_trace_job;
    std::vector<uint8_t> m_keys;

    Settings m_settings;
    bool m_config_changed=false;

    int GetKeyIndex(uint8_t beeb_key) const;
    static bool GetBeebKeyName(void *data,int idx,const char **out_text);
    void SaveButton(const char *label,const std::shared_ptr<Trace> &last_trace,bool cycles);
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
