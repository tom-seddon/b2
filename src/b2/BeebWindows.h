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

void HandleSDLWindowEvent(const SDL_WindowEvent &event);
void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);
void SetSDLMouseWheelState(uint32_t sdl_window_id,int x,int y);
void HandleSDLTextInput(uint32_t sdl_window_id,const char *text);
void HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event);

void UpdateWindowTitles();

// Add job to the job queue.
void AddJob(std::shared_ptr<JobQueue::Job> job);

// Get the job queue's list of jobs.
std::vector<std::shared_ptr<JobQueue::Job>> GetJobs();

BeebWindow *FindBeebWindowBySDLWindowID(uint32_t sdl_window_id);

const std::vector<uint8_t> &GetLastWindowPlacementData();
void SetLastWindowPlacementData(std::vector<uint8_t> placement_data);

size_t GetNumSavedStates();
std::vector<std::shared_ptr<const BeebState>> GetSavedStates(size_t begin_index,
                                                             size_t end_index);
void AddSavedState(std::shared_ptr<const BeebState> saved_state);
void DeleteSavedState(std::shared_ptr<const BeebState> saved_state);

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
