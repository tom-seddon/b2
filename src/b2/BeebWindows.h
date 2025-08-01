#ifndef HEADER_62439EA1AE6F4CC49C648342D7F81AD6 // -*- mode:c++ -*-
#define HEADER_62439EA1AE6F4CC49C648342D7F81AD6

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <vector>
#include <shared/mutex.h>
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
struct SDL_Window;

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
    extern std::string default_config_name;

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

    void HandleVBlank(VBlankMonitor *vblank_monitor, void *display_data, uint64_t ticks);

    void ThreadFillAudioBuffer(uint32_t audio_device_id, float *mix_buffer, size_t mix_buffer_size);

    void UpdateWindowTitles();

    // If the keymap is in use by any windows, they'll be reset to use
    // the default keymap.
    void RemoveBeebKeymapByIndex(size_t index);

    // AddBeebKeymap will adjust KEYMAP's name to make it unique.
    //
    // Returns the pointer to the keymap in the list.
    BeebKeymap *AddBeebKeymap(BeebKeymap keymap);

    // When necessary, name will be adjusted to make it unique.
    void BeebKeymapDidChange(size_t index);

    size_t GetNumBeebKeymaps();
    BeebKeymap *GetBeebKeymapByIndex(size_t index);

    // For each keymap k,
    // in the keymap list, calls func (k). If func returns false,
    // stop iteration and return the keymap it returned false for.
    //
    // Adding/removing keymaps in the loop is safe, but keymaps may be
    // missed or seen multiple times and/or the ForEach return value
    // might be wrong.
    //BeebKeymap *ForEachBeebKeymap(const std::function<bool(BeebKeymap *)> &func);

    // Keymaps are not optimised for retrieval by name.
    BeebKeymap *FindBeebKeymapByName(const std::string &name);

    // Fill out a BeebLoadedConfig for the given BeebConfig.
    //
    // If it's a stock config, hand out the stock loaded config and
    // return true.
    //
    // Otherwise, try to initialize a BeebLoadedConfig, returning
    // false if there was a problem and printing messages out to *msg.
    bool LoadConfigByName(BeebLoadedConfig *loaded_config, const std::string &config_name, Messages *msg);

    void AddConfig(BeebConfig config);
    void RemoveConfigByIndex(size_t index);

    void ConfigDidChange(size_t index);

    size_t GetNumConfigs();
    BeebConfig *GetConfigByIndex(size_t index);

    // When necessary, name will be adjusted to make it unique.
    void SetBeebWindowName(BeebWindow *window, std::string name);

    // Add job to the job queue.
    void AddJob(std::shared_ptr<JobQueue::Job> job);

    // Get the job queue's list of jobs.
    std::vector<std::shared_ptr<JobQueue::Job>> GetJobs();

    BeebWindow *FindBeebWindowForSDLWindow(SDL_Window *sdl_window);

    BeebWindow *FindBeebWindowBySDLWindowID(uint32_t sdl_window_id);

    BeebWindow *FindBeebWindowByName(const std::string &name);

    // Used by the launch mechanism to get the most recently-used window.
    BeebWindow *FindMRUBeebWindow();

    const std::vector<uint8_t> &GetLastWindowPlacementData();
    void SetLastWindowPlacementData(std::vector<uint8_t> placement_data);

    const BeebKeymap *GetDefaultBeebKeymap();

    size_t GetNumSavedStates();
    std::vector<std::shared_ptr<const BeebState>> GetSavedStates(size_t begin_index,
                                                                 size_t end_index);
    void AddSavedState(std::shared_ptr<const BeebState> saved_state);
    void DeleteSavedState(std::shared_ptr<const BeebState> saved_state);

} // namespace BeebWindows

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
