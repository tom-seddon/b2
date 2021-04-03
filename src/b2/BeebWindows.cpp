#include <shared/system.h>
#include "BeebWindows.h"
#include <shared/debug.h>
#include <SDL.h>
#include <algorithm>
#include "keymap.h"
#include "BeebWindow.h"
#include "BeebState.h"
#include "misc.h"
#include "BeebConfig.h"
#include "load_save.h"
#include "b2.h"
#include "BeebKeymap.h"
#include <Remotery.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(OUTPUT);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebWindowsState {
    //std::vector<std::unique_ptr<BeebKeymap>> beeb_keymaps;
    //std::vector<std::unique_ptr<BeebConfig>> configs;

    std::vector<uint8_t> last_window_placement_data;

    JobQueue job_queue;

    uint32_t default_ui_flags=0;

    // This mutex must be taken when manipulating saved_states.
    Mutex saved_states_mutex;
    std::vector<std::shared_ptr<const BeebState>> saved_states;
};

static BeebWindowsState *g_;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindowSettings BeebWindows::defaults;
std::string BeebWindows::default_config_name;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindows::Init() {
    ASSERT(!g_);

    g_=new BeebWindowsState;

    MUTEX_SET_NAME(g_->saved_states_mutex,"BeebWindows saved states mutex");

    if(!g_->job_queue.Init()) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::Shutdown() {
    {
        std::lock_guard<Mutex> lock(g_->saved_states_mutex);

        g_->saved_states.clear();
    }

    delete g_;
    g_=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::HandleSDLWindowEvent(const SDL_WindowEvent &event) {
    ASSERT(event.type==SDL_WINDOWEVENT);

    BeebWindow *window=FindBeebWindowBySDLWindowID(event.windowID);
    if(!window) {
        // ???
        return;
    }

    switch(event.event) {
    case SDL_WINDOWEVENT_CLOSE:
        {
            window->SaveSettings();

            delete window;
            window=nullptr;
        }
        break;

    case SDL_WINDOWEVENT_SHOWN:
    case SDL_WINDOWEVENT_HIDDEN:
    case SDL_WINDOWEVENT_MOVED:
    case SDL_WINDOWEVENT_RESIZED:
    case SDL_WINDOWEVENT_SIZE_CHANGED:
    case SDL_WINDOWEVENT_MAXIMIZED:
    case SDL_WINDOWEVENT_RESTORED:
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        window->SavePosition();
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::HandleSDLKeyEvent(const SDL_KeyboardEvent &event) {
    if(BeebWindow *window=FindBeebWindowBySDLWindowID(event.windowID)) {
        window->HandleSDLKeyEvent(event);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetSDLMouseWheelState(uint32_t sdl_window_id,int x,int y) {
    if(BeebWindow *window=FindBeebWindowBySDLWindowID(sdl_window_id)) {
        window->SetSDLMouseWheelState(x,y);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::HandleSDLTextInput(uint32_t sdl_window_id,const char *text) {
    if(BeebWindow *window=FindBeebWindowBySDLWindowID(sdl_window_id)) {
        window->HandleSDLTextInput(text);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event) {
    if(BeebWindow *window=FindBeebWindowBySDLWindowID(event.windowID)) {
        window->HandleSDLMouseMotionEvent(event);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::AddJob(std::shared_ptr<JobQueue::Job> job) {
    g_->job_queue.AddJob(std::move(job));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<JobQueue::Job>> BeebWindows::GetJobs() {
    return g_->job_queue.GetJobs();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow *BeebWindows::FindBeebWindowBySDLWindowID(uint32_t sdl_window_id) {
    // In principle, you ought to be able to have multiple independent
    // b2BeebWindows collections (not that this would be especially
    // useful...I don't think?), so this should really search its own
    // window list rather than the global SDL one.
    SDL_Window *sdl_window=SDL_GetWindowFromID(sdl_window_id);
    if(!sdl_window) {
        return NULL;
    }

    auto window=(BeebWindow *)SDL_GetWindowData(sdl_window,BeebWindow::SDL_WINDOW_DATA_NAME);
    if(!window) {
        return NULL;
    }

    return window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<uint8_t> &BeebWindows::GetLastWindowPlacementData() {
    return g_->last_window_placement_data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetLastWindowPlacementData(std::vector<uint8_t> placement_data) {
    g_->last_window_placement_data=std::move(placement_data);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//const BeebKeymap *BeebWindows::GetDefaultBeebKeymap() {
//    return g_->beeb_keymaps[0].get();
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebWindows::GetNumSavedStates() {
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    return g_->saved_states.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<const BeebState>> BeebWindows::GetSavedStates(size_t begin_index,
                                                                          size_t end_index)
{
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    ASSERT(begin_index<=PTRDIFF_MAX);
    ASSERT(begin_index<=g_->saved_states.size());
    ASSERT(end_index<=PTRDIFF_MAX);
    ASSERT(end_index<=g_->saved_states.size());
    ASSERT(begin_index<=end_index);

    std::vector<std::shared_ptr<const BeebState>> result(g_->saved_states.begin()+(ptrdiff_t)begin_index,
                                                         g_->saved_states.begin()+(ptrdiff_t)end_index);
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::AddSavedState(std::shared_ptr<const BeebState> saved_state) {
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    g_->saved_states.push_back(std::move(saved_state));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::DeleteSavedState(std::shared_ptr<const BeebState> saved_state) {
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    g_->saved_states.erase(std::remove(g_->saved_states.begin(),
                                       g_->saved_states.end(),
                                       saved_state),
                           g_->saved_states.end());
}
