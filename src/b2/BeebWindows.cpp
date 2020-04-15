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
    // This mutex must be taken when creating or destroying a window. The
    // locking here is a bit careless - this only exists for the benefit of
    // the audio thread, which needs to run through the windows list.
    //
    // (All other accesses to the windows list are from the main thread only,
    // so no locking necessary.)
    Mutex windows_mutex;
    std::vector<BeebWindow *> windows;

    std::vector<std::unique_ptr<BeebKeymap>> beeb_keymaps;
    std::vector<std::unique_ptr<BeebConfig>> configs;

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

static std::string GetUniqueBeebWindowName(std::string name,BeebWindow *ignore) {
    return GetUniqueName(std::move(name),[](const std::string &name)->const void * {
        return BeebWindows::FindBeebWindowByName(name);
    },ignore);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetUniqueBeebKeymapName(std::string name,BeebKeymap *ignore) {
    return GetUniqueName(std::move(name),[](const std::string &name)->const void * {
        return BeebWindows::FindBeebKeymapByName(name);
    },ignore);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<std::unique_ptr<BeebKeymap>>::iterator FindBeebKeymapIterator(BeebKeymap *keymap) {
    auto &&it=g_->beeb_keymaps.begin();

    while(it!=g_->beeb_keymaps.end()) {
        if(it->get()==keymap) {
            break;
        }

        ++it;
    }

    return it;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BeebConfig *FindBeebConfigByName(const std::string &name) {
    for(size_t i=0;i<g_->configs.size();++i) {
        if(g_->configs[i]->name==name) {
            return g_->configs[i].get();
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void MakeNameUnique(BeebConfig *config) {
    config->name=GetUniqueName(config->name,[](const std::string &name)->const void * {
        return FindBeebConfigByName(name);
    },config);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindows::Init() {
    ASSERT(!g_);

    g_=new BeebWindowsState;

    MUTEX_SET_NAME(g_->windows_mutex,"BeebWindows windows mutex");
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
        std::lock_guard<Mutex> lock(g_->windows_mutex);

        for(BeebWindow *window:g_->windows) {
            delete window;
        }
    }

    {
        std::lock_guard<Mutex> lock(g_->saved_states_mutex);

        g_->saved_states.clear();
    }

//    // There probably needs to be a more general mechanism than this.
//    Timeline::DidChange();

    delete g_;
    g_=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow *BeebWindows::CreateBeebWindow(BeebWindowInitArguments init_arguments) {
    init_arguments.name=GetUniqueBeebWindowName(init_arguments.name,
                                                nullptr);

    auto window=new BeebWindow(std::move(init_arguments));

    if(!window->Init()) {
        delete window;
        window=nullptr;

        return nullptr;
    }

    std::lock_guard<Mutex> lock(g_->windows_mutex);
    g_->windows.push_back(window);

//    // There probably needs to be a more general mechanism than this.
//    Timeline::DidChange();

    return window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebWindows::GetNumWindows() {
    return g_->windows.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow *BeebWindows::GetWindowByIndex(size_t index) {
    ASSERT(index<g_->windows.size());

    return g_->windows[index];
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

            std::lock_guard<Mutex> lock(g_->windows_mutex);

            g_->windows.erase(std::remove(g_->windows.begin(),g_->windows.end(),window),g_->windows.end());

            delete window;
            window=nullptr;

            //Timeline::DidChange();
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

void BeebWindows::HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks) {
    size_t i=0;

    ASSERT(g_->windows.size()<PTRDIFF_MAX);

    while(i<g_->windows.size()) {
        size_t old_size=g_->windows.size();
        (void)old_size;

#if RMT_ENABLED
        char rmt_text[100];
        snprintf(rmt_text,sizeof rmt_text,"Window %zu",i);
#endif

        rmt_BeginCPUSampleDynamic(rmt_text,0);

        BeebWindow *window=g_->windows[i];
        bool keep_window=window->HandleVBlank(vblank_monitor,display_data,ticks);
        ASSERT(g_->windows.size()>=old_size);
        ASSERT(g_->windows[i]==window);

        if(keep_window) {
            ++i;
        } else {
            std::lock_guard<Mutex> lock(g_->windows_mutex);

            delete window;
            window=nullptr;

            g_->windows.erase(g_->windows.begin()+(ptrdiff_t)i);
        }

        rmt_EndCPUSample();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::ThreadFillAudioBuffer(uint32_t audio_device_id,float *mix_buffer,size_t mix_buffer_size) {
    // Just hold the lock for the duration - this shouldn't take too
    // long, and the only thing it will block is creation or
    // destruction of windows...
    std::lock_guard<Mutex> lock(g_->windows_mutex);

    for(BeebWindow *window:g_->windows) {
        window->ThreadFillAudioBuffer(audio_device_id,mix_buffer,mix_buffer_size);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::UpdateWindowTitles() {
    for(BeebWindow *window:g_->windows) {
        window->UpdateTitle();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::RemoveBeebKeymap(BeebKeymap *keymap) {
    for(BeebWindow *window:g_->windows) {
        window->BeebKeymapWillBeDeleted(keymap);
    }


    //keymap->WillBeDeleted(&g_->default_keymap);

    auto &&it=FindBeebKeymapIterator(keymap);
    ASSERT(it!=g_->beeb_keymaps.end());
    if(it!=g_->beeb_keymaps.end()) {
        g_->beeb_keymaps.erase(it);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebKeymap *BeebWindows::AddBeebKeymap(BeebKeymap new_keymap,BeebKeymap *other_keymap) {
    std::vector<std::unique_ptr<BeebKeymap>>::iterator it;
    if(!other_keymap) {
        it=g_->beeb_keymaps.end();
    } else {
        it=FindBeebKeymapIterator(other_keymap);
        ASSERT(it!=g_->beeb_keymaps.end());//but it won't break or anything
    }

    new_keymap.SetName(GetUniqueBeebKeymapName(new_keymap.GetName(),nullptr));
    it=g_->beeb_keymaps.insert(it,std::make_unique<BeebKeymap>(std::move(new_keymap)));
    return it->get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetBeebKeymapName(BeebKeymap *keymap,std::string name) {
    keymap->SetName(GetUniqueBeebKeymapName(std::move(name),keymap));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebKeymap *BeebWindows::ForEachBeebKeymap(const std::function<bool(BeebKeymap *)> &func) {
    for(size_t i=0;i<g_->beeb_keymaps.size();++i) {
        BeebKeymap *keymap=g_->beeb_keymaps[i].get();

        if(!func(keymap)) {
            if(i<g_->beeb_keymaps.size()) {
                return g_->beeb_keymaps[i].get();
            } else {
                return nullptr;
            }
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebKeymap *BeebWindows::FindBeebKeymapByName(const std::string &name) {
    return BeebWindows::ForEachBeebKeymap([&name](BeebKeymap *keymap) {
        return name!=keymap->GetName();
    });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindows::LoadConfigByName(BeebLoadedConfig *loaded_config,const std::string &config_name,Messages *msg) {
    const BeebConfig *config=FindBeebConfigByName(config_name);
    if(!config) {
        msg->e.f("unknown config: %s\n",config_name.c_str());
        return false;
    }

    if(!BeebLoadedConfig::Load(loaded_config,*config,msg)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::AddConfig(BeebConfig config) {
    g_->configs.push_back(std::make_unique<BeebConfig>(std::move(config)));

    MakeNameUnique(g_->configs.back().get());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::RemoveConfigByIndex(size_t index) {
    ASSERT(index<g_->configs.size());
    ASSERT(index<PTRDIFF_MAX);
    g_->configs.erase(g_->configs.begin()+(ptrdiff_t)index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::ConfigDidChange(size_t index) {
    ASSERT(index<g_->configs.size());

    MakeNameUnique(g_->configs[index].get());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebWindows::GetNumConfigs() {
    return g_->configs.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebConfig *BeebWindows::GetConfigByIndex(size_t index) {
    ASSERT(index<g_->configs.size());
    return g_->configs[index].get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetBeebWindowName(BeebWindow *window,std::string name) {
    window->SetName(GetUniqueBeebWindowName(std::move(name),window));
    window->UpdateTitle();
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

BeebWindow *BeebWindows::FindBeebWindowByName(const std::string &name) {
    for(BeebWindow *window:g_->windows) {
        if(window->GetName()==name) {
            return window;
        }
    }

    return nullptr;
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

const BeebKeymap *BeebWindows::GetDefaultBeebKeymap() {
    return g_->beeb_keymaps[0].get();
}

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
