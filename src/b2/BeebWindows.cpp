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
#include "Timeline.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(OUTPUT);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Config {
    std::unique_ptr<BeebConfig> config;
    BeebLoadedConfig loaded_config;
    bool got_loaded_config=false;
};

struct BeebWindowsState {
    // This mutex must be taken when adding to or removing from
    // m_windows.
    std::mutex windows_mutex;
    std::vector<BeebWindow *> windows;

    std::vector<std::unique_ptr<Keymap>> keymaps;
    std::vector<Config> configs;
    const BeebConfig *default_config=nullptr;
    const Keymap *default_keymap=&Keymap::DEFAULT;

    std::vector<uint8_t> last_window_placement_data;

    JobQueue job_queue;

    uint32_t default_ui_flags=0;
};

static BeebWindowsState *g_;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindowSettings BeebWindows::defaults;
uint32_t BeebWindows::save_state_shortcut_key=SDLK_PAGEDOWN;
uint32_t BeebWindows::load_last_state_shortcut_key=SDLK_PAGEUP;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BeebWindow *FindBeebWindowByName(const std::string &name) {
    for(BeebWindow *window:g_->windows) {
        if(window->GetName()==name) {
            return window;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetUniqueBeebWindowName(std::string name,BeebWindow *ignore) {
    return GetUniqueName(std::move(name),[](const std::string &name)->const void * {
        return FindBeebWindowByName(name);
    },ignore);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetUniqueKeymapName(std::string name,Keymap *ignore) {
    return GetUniqueName(std::move(name),[](const std::string &name)->const void * {
        return BeebWindows::ForEachKeymap([&name](const Keymap *keymap,Keymap *) {
            return name!=keymap->GetName();
        });
    },ignore);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ResetDefaultConfig() {
    g_->default_config=&BeebLoadedConfig::GetDefaultConfigByIndex(0)->config;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<std::unique_ptr<Keymap>>::iterator FindKeymapIterator(Keymap *keymap) {
    auto &&it=g_->keymaps.begin();

    while(it!=g_->keymaps.end()) {
        if(it->get()==keymap) {
            break;
        }

        ++it;
    }

    return it;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<Config>::iterator FindConfigIterator(BeebConfig *config) {
    auto &&it=g_->configs.begin();

    while(it!=g_->configs.end()) {
        if(it->config.get()==config) {
            break;
        }

        ++it;
    }

    return it;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BeebConfig *FindBeebConfigByName(const std::string &name) {
    const BeebConfig *result=nullptr;

    BeebWindows::ForEachConfig([&result,&name](const BeebConfig *config,const BeebLoadedConfig *loaded_config) {
        if(config) {
            if(config->name==name) {
                result=config;
                return false;
            }
        } else {
            ASSERT(loaded_config);
            if(loaded_config->config.name==name) {
                result=&loaded_config->config;
                return false;
            }
        }

        return true;
    });

    return result;
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

    ResetDefaultConfig();

    if(!g_->job_queue.Init()) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(g_->windows_mutex);

        for(BeebWindow *window:g_->windows) {
            delete window;
        }
    }

    // There probably needs to be a more general mechanism than this.
    Timeline::DidChange();

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

    std::lock_guard<std::mutex> lock(g_->windows_mutex);
    g_->windows.push_back(window);

    // There probably needs to be a more general mechanism than this.
    Timeline::DidChange();

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

            std::lock_guard<std::mutex> lock(g_->windows_mutex);

            g_->windows.erase(std::remove(g_->windows.begin(),g_->windows.end(),window),g_->windows.end());

            delete window;
            window=nullptr;

            Timeline::DidChange();
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

void BeebWindows::HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks) {
    size_t i=0;

    ASSERT(g_->windows.size()<PTRDIFF_MAX);

    while(i<g_->windows.size()) {
        size_t old_size=g_->windows.size();
        (void)old_size;

        BeebWindow *window=g_->windows[i];
        bool keep_window=window->HandleVBlank(vblank_monitor,display_data,ticks);
        ASSERT(g_->windows.size()>=old_size);
        ASSERT(g_->windows[i]==window);

        if(keep_window) {
            ++i;
        } else {
            std::lock_guard<std::mutex> lock(g_->windows_mutex);

            delete window;
            window=nullptr;

            g_->windows.erase(g_->windows.begin()+(ptrdiff_t)i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::ThreadFillAudioBuffer(uint32_t audio_device_id,float *mix_buffer,size_t mix_buffer_size) {
    // Just hold the lock for the duration - this shouldn't take too
    // long, and the only thing it will block is creation or
    // destruction of windows...
    std::lock_guard<std::mutex> lock(g_->windows_mutex);

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

void BeebWindows::RemoveKeymap(Keymap *keymap) {
    for(BeebWindow *window:g_->windows) {
        window->KeymapWillBeDeleted(keymap);
    }

    keymap->WillBeDeleted(&g_->default_keymap);

    auto &&it=FindKeymapIterator(keymap);
    ASSERT(it!=g_->keymaps.end());
    if(it!=g_->keymaps.end()) {
        g_->keymaps.erase(it);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Keymap *BeebWindows::AddKeymap(Keymap new_keymap,Keymap *other_keymap) {
    std::vector<std::unique_ptr<Keymap>>::iterator it;
    if(!other_keymap) {
        it=g_->keymaps.end();
    } else {
        it=FindKeymapIterator(other_keymap);
        ASSERT(it!=g_->keymaps.end());//but it won't break or anything
    }

    new_keymap.SetName(GetUniqueKeymapName(new_keymap.GetName(),nullptr));
    it=g_->keymaps.insert(it,std::make_unique<Keymap>(std::move(new_keymap)));
    return it->get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetKeymapName(Keymap *keymap,std::string name) {
    keymap->SetName(GetUniqueKeymapName(std::move(name),keymap));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const Keymap *const STOCK_KEYMAPS[]={
    &Keymap::DEFAULT,
    &Keymap::DEFAULT_CC,
    &Keymap::DEFAULT_UK,
    &Keymap::DEFAULT_US,
    nullptr,
};

const Keymap *BeebWindows::ForEachKeymap(const std::function<bool(const Keymap *,Keymap *)> &func) {
    for(const Keymap *const *keymap_ptr=STOCK_KEYMAPS;*keymap_ptr;++keymap_ptr) {
        if(!func(*keymap_ptr,nullptr)) {
            return *keymap_ptr;
        }
    }

    for(size_t i=0;i<g_->keymaps.size();++i) {
        Keymap *keymap=g_->keymaps[i].get();

        if(!func(keymap,keymap)) {
            if(i<g_->keymaps.size()) {
                return g_->keymaps[i].get();
            } else {
                return nullptr;
            }
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindows::GetLoadedConfigForConfig(BeebLoadedConfig *loaded_config,const BeebConfig *config,Messages *msg) {
    // Hmm. This isn't very nice. But if you just ignore it... it's
    // probably OK.
    for(size_t i=0;i<BeebLoadedConfig::GetNumDefaultConfigs();++i) {
        const BeebLoadedConfig *c=BeebLoadedConfig::GetDefaultConfigByIndex(i);

        if(config==&c->config) {
            *loaded_config=*c;
            return true;
        }
    }

    for(size_t i=0;i<g_->configs.size();++i) {
        Config *c=&g_->configs[i];

        if(c->config.get()==config) {
            if(!c->got_loaded_config) {
                if(!BeebLoadedConfig::Load(&c->loaded_config,*config,msg)) {
                    return false;
                }

                c->got_loaded_config=true;
            }

            *loaded_config=c->loaded_config;
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebConfig *BeebWindows::AddConfig(BeebConfig config,BeebConfig *other_config) {
    std::vector<Config>::iterator it;
    if(!other_config) {
        it=g_->configs.end();
    } else {
        it=FindConfigIterator(other_config);
        ASSERT(it!=g_->configs.end());//but it won't break
    }

    it=g_->configs.insert(it,Config());
    it->config=std::make_unique<BeebConfig>(std::move(config));

    MakeNameUnique(it->config.get());

    return it->config.get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::RemoveConfig(BeebConfig *config) {
    auto &&it=FindConfigIterator(config);
    ASSERT(it!=g_->configs.end());

    if(it!=g_->configs.end()) {
        if(it->config.get()==g_->default_config) {
            ResetDefaultConfig();
        }

        g_->configs.erase(it);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::ConfigDidChange(BeebConfig *config) {
    auto &&it=FindConfigIterator(config);
    ASSERT(it!=g_->configs.end());

    MakeNameUnique(it->config.get());

    if(it!=g_->configs.end()) {
        // This could be improved. Not all changes will affect the
        // BeebLoadedConfig.

        it->loaded_config=BeebLoadedConfig();//clear out any shared_ptr refs...
        it->got_loaded_config=false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebConfig *BeebWindows::GetDefaultConfig() {
    return g_->default_config;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetDefaultConfig(const std::string &name) {
    g_->default_config=FindBeebConfigByName(name);

    if(!g_->default_config) {
        ResetDefaultConfig();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::ForEachConfig(const std::function<bool(BeebConfig *,const BeebLoadedConfig *)> &func) {
    for(size_t i=0;i<BeebLoadedConfig::GetNumDefaultConfigs();++i) {
        const BeebLoadedConfig *loaded_config=BeebLoadedConfig::GetDefaultConfigByIndex(i);

        if(!func(nullptr,loaded_config)) {
            return;
        }
    }

    for(size_t i=0;i<g_->configs.size();++i) {
        const Config *c=&g_->configs[i];

        const BeebLoadedConfig *l=nullptr;
        if(c->got_loaded_config) {
            l=&c->loaded_config;
        }

        if(!func(c->config.get(),l)) {
            return;
        }
    }
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

const Keymap *BeebWindows::GetDefaultKeymap() {
    return g_->default_keymap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetDefaultKeymap(const Keymap *keymap) {
    g_->default_keymap=keymap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
