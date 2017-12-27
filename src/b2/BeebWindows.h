#ifndef HEADER_62439EA1AE6F4CC49C648342D7F81AD6 // -*- mode:c++ -*-
#define HEADER_62439EA1AE6F4CC49C648342D7F81AD6

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <vector>
#include <mutex>
#include <string>
#include <memory>
#include <beeb/conf.h>
#include "BeebConfig.h"
#include "JobQueue.h"
#include "BeebKeymap.h"
#include <functional>

class BeebState;
class VBlankMonitor;
class BeebWindow;
class BBCMicro;
class DiscImage;
struct BeebWindowInitArguments;
struct SDL_WindowEvent;
class BeebConfig;
class BeebLoadedConfig;
struct SDL_KeyboardEvent;
struct BeebWindowSettings;
struct SDL_MouseMotionEvent;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Look after the list of windows, and things that are shared between
// them: keymaps, configs, some shared settings.
//
// Configs and keymaps are copyable, but once added must be referred
// to by the pointer returned from the Add function. This points to
// the canonical editable copy.
//
// Keymaps are referred to by reference; to edit one, use the Keymap
// member functions directly.
//
// BeebConfigs are referred to by value. After editing one, call the
// ConfigDidChange function to update things.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace BeebWindows {
    // an accessor doesn't really buy much here...
    extern BeebWindowSettings defaults;

    bool Init();
    void Shutdown();

    // (This was supposed to be "CreateWindow", but it had to avoid
    // using the Windows CreateWindow define.)
    //
    // There isn't all that much you can do with the result except
    // check if it is nullptr - in which case the creation failed.
    BeebWindow *CreateBeebWindow(BeebWindowInitArguments init_arguments);

    size_t GetNumWindows();
    BeebWindow *GetWindowByIndex(size_t index);

    void HandleSDLWindowEvent(const SDL_WindowEvent &event);
    void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);
    void SetSDLMouseWheelState(uint32_t sdl_window_id,int x,int y);
    void HandleSDLTextInput(uint32_t sdl_window_id,const char *text);
    void HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event);

    void HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks);

    void ThreadFillAudioBuffer(uint32_t audio_device_id,float *mix_buffer,size_t mix_buffer_size);

    void UpdateWindowTitles();

    // If the keymap is in use by any windows, they'll be reset to use
    // the default keymap.
    void RemoveBeebKeymap(BeebKeymap *keymap);

    // When necessary, name will be adjusted to make it unique.
    //
    // When other_keymap is not null, this will take other_keymap's
    // place in the list (other_keymap and those past it will be
    // shuffled up one); otherwise, it will be added to the end.
    //
    // Returns the pointer to the editable keymap in the list.
    BeebKeymap *AddBeebKeymap(BeebKeymap keymap,BeebKeymap *other_keymap=nullptr);

    // When necessary, name will be adjusted to make it unique.
    void SetBeebKeymapName(BeebKeymap *keymap,std::string name);

    // For each stock keymap k, calls func(k,nullptr); for each keymap
    // in the keymap list, calls func (k,k). If func returns false,
    // stop iteration and return the keymap it returned false for.
    //
    // Adding/removing keymaps in the loop is safe, but keymaps may be
    // missed or seen multiple times and/or the ForEach return value
    // might be wrong.
    const BeebKeymap *ForEachBeebKeymap(const std::function<bool(const BeebKeymap *,BeebKeymap *)> &func);

    // Keymaps are not optimised for retrieval by name.
    const BeebKeymap *FindBeebKeymapByName(const std::string &name);

    // Alterations take effect the next time this config is selected.
    //void SetConfig(BeebConfig *config,BeebConfig new_config);

    // Fill out a BeebLoadedConfig for the given BeebConfig.
    //
    // If it's a stock config, hand out the stock loaded config and
    // return true.
    //
    // Otherwise, if there's a BeebLoadedConfig ready for it, hand
    // that out and return true.
    //
    // Otherwise, try to initialize a BeebLoadedConfig, returning
    // false if there was a problem and printing messages out to *msg.
    bool GetLoadedConfigForConfig(BeebLoadedConfig *loaded_config,const BeebConfig *config,Messages *msg);

    // 
    BeebConfig *AddConfig(BeebConfig config,BeebConfig *other_config=nullptr);
    void RemoveConfig(BeebConfig *config);
    void ConfigDidChange(BeebConfig *config);

    const BeebConfig *GetDefaultConfig();
    void SetDefaultConfig(const std::string &default_config_name);

    // For each stock loaded config l, calls func(nullptr,l).
    //
    // Then for each config in the config list, calls func(c,l), where
    // c points to its canonical BeebConfig and l its BeebLoadedConfig
    // (if loaded) or null (if not). If func returns false, stop
    // iteration.
    void ForEachConfig(const std::function<bool(BeebConfig *,const BeebLoadedConfig *)> &func);

    // When necessary, name will be adjusted to make it unique.
    void SetBeebWindowName(BeebWindow *window,std::string name);

    // Add job to the job queue.
    void AddJob(std::shared_ptr<JobQueue::Job> job);

    // Get the job queue's list of jobs.
    std::vector<std::shared_ptr<JobQueue::Job>> GetJobs();

    BeebWindow *FindBeebWindowBySDLWindowID(uint32_t sdl_window_id);

    const std::vector<uint8_t> &GetLastWindowPlacementData();
    void SetLastWindowPlacementData(std::vector<uint8_t> placement_data);

    const BeebKeymap *GetDefaultBeebKeymap();
    void SetDefaultBeebKeymap(const BeebKeymap *keymap);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
