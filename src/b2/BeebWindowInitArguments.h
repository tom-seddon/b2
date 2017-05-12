#ifndef HEADER_C20C56AFA58B4B8EB96BE8857C1AACAE// -*- mode:c++ -*-
#define HEADER_C20C56AFA58B4B8EB96BE8857C1AACAE

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebState;
class MessageList;
class Keymap;

#include <string>
#include <memory>
#include <vector>
#include <SDL.h>
#include "BeebConfig.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebWindowInitArguments {
public:
    // Index of SDL render driver, as supplied to SDL_CreateRenderer.
    int render_driver_index=-1;

    // SDL pixel format to use.
    uint32_t pixel_format=0;

    // SDL_AudioDeviceID of device to play to. The window doesn't
    // create the audio device, but it needs to know which ID to use
    // so it can respond to the appropriate callback.
    uint32_t sound_device=0;

    // Sound playback details.
    SDL_AudioSpec sound_spec;

    // Name for the window.
    std::string name;

    // Placement data to use for this window. Empty if the default
    // position is OK.
    std::vector<uint8_t> placement_data;
    
#if SYSTEM_OSX

    // Frame name to load window frame from, or empty if the default
    // is OK.
    std::string frame_name;
    
#endif

    // If set, the emulator is initially paused.
    //
    // (This isn't actually terribly useful, because there's no way to
    // then unpause it from the public API - but the flag has to go
    // somewhere, and it has to funnel through CreateBeebWindow, so...
    // here it is.)
    bool initially_paused=false;

    // When INITIAL_STATE is non-null, it is used as the initial state
    // for the window, and PARENT_TIMELINE_EVENT_ID is the parent
    // event id. (This is used to split the timeline at a non-start
    // event, without having to introduce a new start event.)
    //
    // When INITIAL_STATE is null, but PARENT_TIMELINE_EVENT_ID is
    // non-zero, PARENT_TIMELINE_EVENT_ID is the event to load the
    // state from. (It must be a start event.)
    //
    // Otherwise (INITIAL_STATE==null, PARENT_TIMELINE_EVENT_ID==0),
    // DEFAULT_CONFIG holds the config to start with, and a new root
    // node will be created to start the timeline.
    //
    // DEFAULT_CONFIG must always be valid, even when INITIAL_STATE
    // and PARENT_TIMELINE_EVENT_ID will be used, as it is the config
    // used for new windows created by Window|New.
    std::shared_ptr<BeebState> initial_state;
    uint64_t parent_timeline_event_id=0;
    BeebLoadedConfig default_config;

    // Message list to be used to populate the window's message list.
    // This will almost always be null; it's used to collect messages
    // from the initial startup that are printed before there's any
    // windows to display them on.
    //
    // If the window init fails, and preinit_messages!=nullptr, any
    // additional init messages will be added here too.
    std::shared_ptr<MessageList> preinit_message_list;

    // If preinit_messages is null, this should be non-zero. It's the
    // ID of the window to send init messages to if the init fails.
    //
    // (If the initiating window has gone away, messages will be
    // printed to Messages::stdio.)
    uint32_t initiating_window_id=0;

    // Initial keymap to select.
    const Keymap *keymap=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
