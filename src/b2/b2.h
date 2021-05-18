#ifndef HEADER_6CE0510D80AC4A17BD606A68EAA242EC// -*- mode:c++ -*-
#define HEADER_6CE0510D80AC4A17BD606A68EAA242EC

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#include <string>
#include <vector>
#include <functional>

#include <shared/enum_decl.h>
#include "b2.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Call the given function next time round the loop.
void PushFunctionMessage(std::function<void()> fun);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Global settings, so the load/save stuff can find them.
//
// Some better mechanism to follow, perhaps...

extern bool g_option_vsync;

struct GlobalStats {
    struct VBlankStats {
        uint64_t production_ticks=0;
        uint64_t production_delta_ticks=0;
        
        uint64_t consumption_ticks=0;
        uint64_t consumption_delta_ticks=0;
    };
    
    struct DisplayStats {
        uint32_t display_id=0;
        int x=0,y=0,w=0,h=0;
        VBlankStats vblank_stats_base;
        std::vector<VBlankStats> vblank_stats;
        size_t vblank_stats_head=0;
    };

    struct UpdateStats {
        uint64_t update_ticks=0;
    };
    
    struct EventStats {
        uint64_t num_vblank_messages=0;
        uint64_t num_mouse_motion_events=0;
        uint64_t num_mouse_wheel_events=0;
        uint64_t num_text_input_events=0;
        uint64_t num_key_events=0;
    };
    
    EventStats total;
    EventStats window;

    std::vector<DisplayStats> display_stats;

    size_t update_stats_head=0;
    std::vector<UpdateStats> update_stats;
};

const GlobalStats *GetGlobalStats();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
