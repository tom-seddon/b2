#include <shared/system.h>
#include <shared/system_specific.h>
#include "BeebWindow.h"
#include <beeb/OutputData.h>
#include "Remapper.h"
#include <beeb/conf.h>
#include <shared/mutex.h>
#include "BeebThread.h"
#include <beeb/sound.h>
#include <shared/debug.h>
#include "keymap.h"
#include "keys.h"
#include <algorithm>
#include "MemoryDiscImage.h"
#include "65link.h"
#include "BeebWindows.h"
#include <beeb/BBCMicro.h>
#include <SDL_syswm.h>
#include "VBlankMonitor.h"
#include "b2.h"
#include <inttypes.h>
#include "filters.h"
#include "misc.h"
#include "TimelineUI.h"
#include "BeebState.h"
#include "ConfigsUI.h"
#include "KeymapsUI.h"
#include "MessagesUI.h"
#include "load_save.h"
#include "TraceUI.h"
#include <IconsFontAwesome5.h>
#include "DataRateUI.h"
#include <shared/path.h>
#include "CommandContextStackUI.h"
#include "CommandKeymapsUI.h"
#include "PixelMetadataUI.h"
#include "DearImguiTestUI.h"
#include "debugger.h"
#include "HTTPServer.h"
#include "DirectDiscImage.h"
#include "SavedStatesUI.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#ifdef _DEBUG
#define GOT_CRTDBG 1
#endif
#endif

#ifdef Success
// X.h nonsense.
#undef Success
#endif

#include <shared/enum_def.h>
#include "BeebWindow.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// if true, beeb display should fill the entire imgui window, ignoring
// aspect ratio. Use when debugging imgui window size.
#define BEEB_DISPLAY_FILL 0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(OUTPUT);
LOG_EXTERN(OUTPUTND);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if RMT_ENABLED
static size_t g_num_BeebWindow_inits=0;
#if RMT_USE_OPENGL
static int g_unbind_opengl=0;
#endif
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const double MESSAGES_POPUP_TIME_SECONDS=2.5;
static const double LEDS_POPUP_TIME_SECONDS=1.;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_65LINK="65link";
static const std::string RECENT_PATHS_DISC_IMAGE="disc_image";
//static const std::string RECENT_PATHS_RAM="ram";
static const std::string RECENT_PATHS_NVRAM="nvram";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CallbackCallData {
    uint64_t ticks=0;
    size_t num_units_mixed=0;
    uint32_t max_num_cycles_2MHz=0;
    int num_samples=0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// GetWindowData has a loop (!) with strcmp in it (!) so the data name
// wants to be short.
const char BeebWindow::SDL_WINDOW_DATA_NAME[]="D";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::DriveState::DriveState():
open_65link_folder_dialog(RECENT_PATHS_65LINK),
open_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE),
open_direct_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE)
{
    const std::string &patterns=GetDiscImageFileDialogPatterns();

    this->open_disc_image_file_dialog.AddFilter("BBC disc images",patterns+";*.zip");
    this->open_disc_image_file_dialog.AddAllFilesFilter();

    this->open_direct_disc_image_file_dialog.AddFilter("BBC disc images",patterns);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebWindow::OptionsUI:
    public SettingsUI
{
public:
    explicit OptionsUI(BeebWindow *beeb_window);

    void DoImGui(CommandContextStack *cc_stack) override;

    bool OnClose() override;
protected:
private:
    BeebWindow *m_beeb_window=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::OptionsUI::OptionsUI(BeebWindow *beeb_window):
    m_beeb_window(beeb_window)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::OptionsUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    const std::shared_ptr<BeebThread> &beeb_thread=m_beeb_window->m_beeb_thread;
    BeebWindowSettings *settings=&m_beeb_window->m_settings;

    {
        bool paused=m_beeb_window->m_beeb_thread->IsPaused();
        if(ImGui::Checkbox("Paused",&paused)) {
            beeb_thread->Send(std::make_unique<BeebThread::PauseMessage>(paused));
        }
    }

    {
        bool value=beeb_thread->IsSpeedLimited();
        if(ImGui::Checkbox("Limit speed",&value)) {
            beeb_thread->Send(std::make_unique<BeebThread::SetSpeedLimitingMessage>(value));
        }
    }

    {
        bool value=beeb_thread->IsTurboDisc();
        if(ImGui::Checkbox("Turbo disc",&value)) {
            beeb_thread->Send(std::make_unique<BeebThread::SetTurboDiscMessage>(value));
        }
    }

    //m_beeb_window->DoOptionsCheckbox("Limit speed",&BeebThread::IsSpeedLimited,&BeebThread::SendSetSpeedLimitingMessage);
    //m_beeb_window->DoOptionsCheckbox("Turbo disc",&BeebThread::IsTurboDisc,&BeebThread::SendSetTurboDiscMessage);

    ImGui::Checkbox("Correct aspect ratio",&settings->correct_aspect_ratio);

    ImGui::Checkbox("Auto scale",&settings->display_auto_scale);

    ImGui::DragFloat("Manual scale",&settings->display_manual_scale,.01f,0.f,10.f);

    if(ImGui::Checkbox("Filter display",&settings->display_filter)) {
        m_beeb_window->RecreateTexture();
    }

    if(ImGui::SliderFloat("BBC volume",&settings->bbc_volume,MIN_DB,MAX_DB,"%.1f dB")) {
        beeb_thread->SetBBCVolume(settings->bbc_volume);
    }

    if(ImGui::SliderFloat("Disc volume",&settings->disc_volume,MIN_DB,MAX_DB,"%.1f dB")) {
        beeb_thread->SetDiscVolume(settings->disc_volume);
    }

    ImGui::NewLine();

#if BBCMICRO_DEBUGGER
    if(ImGui::CollapsingHeader("Display Debug Flags",ImGuiTreeNodeFlags_DefaultOpen)) {
        {
            std::unique_lock<Mutex> lock;
            BBCMicro *m=m_beeb_window->m_beeb_thread->LockMutableBeeb(&lock);

            bool debug=m->GetTeletextDebug();
            if(ImGui::Checkbox("Teletext debug",&debug)) {
                m->SetTeletextDebug(debug);
            }
        }

        ImGui::Checkbox("Show TV beam position",&m_beeb_window->m_show_beam_position);
        if(ImGui::Checkbox("Test pattern",&m_beeb_window->m_test_pattern)) {
            if(m_beeb_window->m_test_pattern) {
                m_beeb_window->m_tv.FillWithTestPattern();
            }
        }
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::OptionsUI::OnClose() {
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::BeebWindow(BeebWindowInitArguments init_arguments):
    m_init_arguments(std::move(init_arguments)),
    m_occ(this,&ms_command_table)
{
    m_name=m_init_arguments.name;

    m_message_list=std::make_shared<MessageList>();
    m_msg=Messages(m_message_list);

    m_beeb_thread=std::make_shared<BeebThread>(m_message_list,
                                               init_arguments.sound_device,
                                               init_arguments.sound_spec.freq,
                                               init_arguments.sound_spec.samples,
                                               m_init_arguments.default_config,
                                               std::vector<BeebThread::TimelineEvent>());

    if(init_arguments.use_settings) {
        m_settings=init_arguments.settings;
    } else {
        m_settings=BeebWindows::defaults;
    }

    m_beeb_thread->SetBBCVolume(m_settings.bbc_volume);
    m_beeb_thread->SetDiscVolume(m_settings.disc_volume);

    m_blend_amt=1.f;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::~BeebWindow() {
    m_beeb_thread->Stop();

    // Clear these explicitly before destroying the dear imgui stuff
    // and shutting down SDL.
    for(int i=0;i<BeebWindowPopupType_MaxValue;++i) {
        m_popups[i]=nullptr;
    }

    delete m_imgui_stuff;
    m_imgui_stuff=nullptr;

    if(m_tv_texture) {
        SDL_DestroyTexture(m_tv_texture);
    }

    if(m_renderer) {
        SDL_DestroyRenderer(m_renderer);
    }

    if(m_window) {
        SDL_DestroyWindow(m_window);
    }

    if(m_pixel_format) {
        SDL_FreeFormat(m_pixel_format);
    }

#if RMT_ENABLED
    if(g_num_BeebWindow_inits>0) {
        --g_num_BeebWindow_inits;
    }

    if(g_num_BeebWindow_inits==0) {
#if RMT_USE_OPENGL
        if(g_unbind_opengl) {
            rmt_UnbindOpenGL();
            g_unbind_opengl=0;
        }
#endif
    }
#endif

    if(m_sound_device!=0) {
        SDL_UnlockAudioDevice(m_sound_device);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &BeebWindow::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SetName(std::string name) {
    m_name=std::move(name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::GetBeebKeyState(BeebKey key) const {
    if(key<0) {
        return false;
    } else if(key==BeebKey_Break) {
        return false;
    } else {
        return m_beeb_thread->GetKeyState(key);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - nasty, hacky logic dating from when the BBC display was a
// special case. It doesn't even work properly. Now that the BBC has a
// dear imgui window, like everything else, it can probably be
// improved.

void BeebWindow::HandleSDLKeyEvent(const SDL_KeyboardEvent &event) {
    bool state=event.type==SDL_KEYDOWN;

    //LOGF(OUTPUT,"%s: key=%s state=%s timestamp=%u\n",__func__,SDL_GetScancodeName(event.keysym.scancode),BOOL_STR(state),event.timestamp);

    m_imgui_stuff->SetKeyDown(event.keysym.scancode,state);

    if(m_imgui_has_kb_focus) {
        // Let dear imgui have the keypress.
        return;
    }

    bool prefer_shortcuts=m_keymap->GetPreferShortcuts();

    bool got_ccs=false;
    if(m_cc_stack.GetNumCCs()>1) {
        got_ccs=true;
    }

    uint32_t keycode=0;
    if(state) {
        keycode=(uint32_t)event.keysym.sym|GetPCKeyModifiersFromSDLKeymod(event.keysym.mod);
    }

    if((prefer_shortcuts||got_ccs)&&keycode!=0) {
        if(m_cc_stack.ExecuteCommandsForPCKey(keycode)) {
            // The emulator may later get key up messages for this
            // key, but that is (ought to be...) OK.
            return;
        }

        if(got_ccs) {
            // a command context has the focus. Don't let the keypress
            // get through to the emulator.
            return;
        }
    }

    if(this->HandleBeebKey(event.keysym,state)) {
        // The emulated BBC ate the keypress.
        return;
    }

    if(!prefer_shortcuts&&keycode!=0) {
        if(m_cc_stack.ExecuteCommandsForPCKey(keycode)) {
            return;
        }
    }
}

bool BeebWindow::HandleBeebKey(const SDL_Keysym &keysym,bool state) {
    const BeebKeymap *keymap=m_keymap;
    if(!keymap) {
        return false;
    }

    if(keymap->IsKeySymMap()) {
        uint32_t pc_key=(uint32_t)keysym.sym;
        if(pc_key&PCKeyModifier_All) {
            // Bleargh... can't handle this one.
            return false;
        }

        uint32_t modifiers=GetPCKeyModifiersFromSDLKeymod(keysym.mod);

        if(!state) {
            auto &&it=m_beeb_keysyms_by_keycode.find(pc_key);

            if(it!=m_beeb_keysyms_by_keycode.end()) {
                for(BeebKeySym beeb_keysym:it->second) {
                    m_beeb_thread->Send(std::make_unique<BeebThread::KeySymMessage>(beeb_keysym,false));
                }

                m_beeb_keysyms_by_keycode.erase(it);
            }
        }

        const int8_t *beeb_syms=keymap->GetValuesForPCKey(pc_key|modifiers);
        if(!beeb_syms) {
            // If key+modifier isn't bound, just go for key on its
            // own (and the modifiers will be applied in the
            // emulated BBC).
            beeb_syms=keymap->GetValuesForPCKey(pc_key&~PCKeyModifier_All);
        }

        if(!beeb_syms) {
            return false;
        }

        for(const int8_t *beeb_sym=beeb_syms;*beeb_sym>=0;++beeb_sym) {
            if(state) {
                m_beeb_keysyms_by_keycode[pc_key].insert((BeebKeySym)*beeb_sym);
                m_beeb_thread->Send(std::make_unique<BeebThread::KeySymMessage>((BeebKeySym)*beeb_sym,state));
            }
        }
    } else {
        const int8_t *beeb_keys=keymap->GetValuesForPCKey(keysym.scancode);
        if(!beeb_keys) {
            return false;
        }

        for(const int8_t *beeb_key=beeb_keys;*beeb_key>=0;++beeb_key) {
            m_beeb_thread->Send(std::make_unique<BeebThread::KeyMessage>((BeebKey)*beeb_key,state));
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BeebWindow::GetSDLWindowID() const {
    return SDL_GetWindowID(m_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SetSDLMouseWheelState(int x,int y) {
    (void)x;

    m_imgui_stuff->SetMouseWheel(y);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLTextInput(const char *text) {
    m_imgui_stuff->AddInputCharactersUTF8(text);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event) {
    //LOGF(OUTPUT,"%s: x=%" PRId32 " y=%" PRId32 "\n",__func__,event.x,event.y);
    m_mouse_pos.x=event.x;
    m_mouse_pos.y=event.y;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool BeebWindow::LoadDiscImageFile(int drive,const std::string &path) {
//    std::unique_ptr<MemoryDiscImage> disc_image=MemoryDiscImage::LoadFromFile(path,&m_msg);
//    if(!disc_image) {
//        //m_msg.w.f("Failed to load %s: %s\n",path.c_str(),error.c_str());
//        return false;
//    } else {
//        m_beeb_thread->Send(std::make_unique<BeebThread::LoadDiscMessage>(drive,std::move(disc_image),true));
//        return true;
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::Load65LinkFolder(int drive,const std::string &path) {
    std::string error;
    std::shared_ptr<DiscImage> disc_image=LoadDiscImageFrom65LinkFolder(path,&m_msg);
    if(!disc_image) {
        //m_msg.w.f("Failed to load %s: %s\n",path.c_str(),error.c_str());
        return false;
    } else {
        m_beeb_thread->Send(std::make_unique<BeebThread::LoadDiscMessage>(drive,std::move(disc_image),true));
        return true;
    }
}

class FileMenuItem {
public:
    bool selected=false;
    std::string path;
    bool recent=false;

    explicit FileMenuItem(SelectorDialog *dialog,const char *open_title,const char *recent_title):
        m_dialog(dialog)
    {
        bool recent_enabled=true;

        ImGuiIDPusher id_pusher(open_title);

        if(ImGui::MenuItem(open_title)) {
            if(dialog->Open(&this->path)) {
                this->recent=false;
                this->selected=true;
                recent_enabled=false;
            }
        }

        RecentPaths *rp=nullptr;//dialog->GetRecentPaths();
        size_t num_rp=0;
        if(recent_enabled) {
            rp=dialog->GetRecentPaths();
            if(rp) {
                num_rp=rp->GetNumPaths();
                recent_enabled=num_rp>0;
            }
        }

        if(ImGui::BeginMenu(recent_title,recent_enabled)) {
            for(size_t i=0;i<num_rp;++i) {
                const std::string &p=rp->GetPathByIndex(i);
                if(ImGui::MenuItem(p.c_str())) {
                    this->selected=true;
                    this->path=p;
                }
            }

            ImGui::Separator();

            if(ImGui::BeginMenu("Remove item")) {
                size_t i=0;

                while(i<rp->GetNumPaths()) {
                    if(ImGui::MenuItem(rp->GetPathByIndex(i).c_str())) {
                        rp->RemovePathByIndex(i);
                    } else {
                        ++i;
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
    }

    void Success() {
        if(!this->recent) {
            m_dialog->AddLastPathToRecentPaths();
        }
    }
protected:
private:
    SelectorDialog *m_dialog;
};

static size_t CleanUpRecentPaths(const std::string &tag,bool (*exists_fn)(const std::string &)) {
    size_t n=0;

    if(RecentPaths *rp=GetRecentPathsByTag(tag)) {
        size_t i=0;

        while(i<rp->GetNumPaths()) {
            if((*exists_fn)(rp->GetPathByIndex(i))) {
                ++i;
            } else {
                rp->RemovePathByIndex(i);
                ++n;
            }
        }
    }

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::DoImGui(uint64_t ticks) {
    //const uint64_t now=GetCurrentTickCount();

    int output_width,output_height;
    SDL_GetRendererOutputSize(m_renderer,&output_width,&output_height);

    bool keep_window=true;

    m_imgui_has_kb_focus=false;

#if BBCMICRO_DEBUGGER
    m_got_debug_halted=false;
#endif

    m_cc_stack.Reset();
    m_cc_stack.Push(m_occ,true);//true=force push

    if(m_imgui_stuff->WantCaptureKeyboard()) {
        m_imgui_has_kb_focus=true;
    }

    if(m_imgui_stuff->WantTextInput()) {
        m_imgui_has_kb_focus=true;
    }

    // Set the window padding to 0x0, so that the docking stuff, which
    // makes its own child windows, ends up tightly aligned to the
    // window border. (Well, tightly enough, anyway... in fact there's
    // still a 1 pixel border that I haven't figured out yet.)
    //
    // 0x0 padding makes everything else look a bit of a mess, so this
    // does mean that the default window padding has to be pushed in
    // various places. Need to fix this...
    ImGuiStyleVarPusher vpusher1(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));
    {
        {
            ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);

            if(!this->DoMenuUI()) {
                keep_window=false;
            }
        }

        {
            ImVec2 pos=ImGui::GetCursorPos();
            pos.x=0.f;
            ImVec2 size={(float)output_width-pos.x,(float)output_height-pos.y};

            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
        }

        ImGuiCol style_colour=ImGuiCol_WindowBg;
        ImGuiStyleColourPusher cpusher1(style_colour,ImVec4(0.f,0.f,0.f,0.f));

        if(ImGui::Begin("DockHolder",
                        nullptr,
                        (ImGuiWindowFlags_NoScrollWithMouse|
                         ImGuiWindowFlags_NoTitleBar|
                         ImGuiWindowFlags_NoResize|
                         ImGuiWindowFlags_NoMove|
                         ImGuiWindowFlags_NoScrollbar|
                         ImGuiWindowFlags_NoSavedSettings|
                         ImGuiWindowFlags_NoInputs|
                         ImGuiWindowFlags_NoBringToFrontOnFocus)))
        {
            {
                ImGuiStyleColourPusher cpusher2;
                cpusher2.PushDefault(style_colour);

                ImGui::BeginDockspace();
                {
                    ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);

                    this->DoSettingsUI();

                    this->DoBeebDisplayUI();
                }
                ImGui::EndDockspace();

                {
                    ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);

#if ENABLE_IMGUI_DEMO
                    if(m_imgui_demo) {
                        ImGui::ShowDemoWindow();
                    }
#endif

                    if(m_imgui_dock_debug) {
                        ImGui::DockDebugWindow();
                    }

#if STORE_DRAWLISTS
                    if(m_imgui_drawlists) {
                        m_imgui_stuff->DoStoredDrawListWindow();
                    }
#endif

                    this->DoPopupUI(ticks,output_width,output_height);
                }
            }
            ImGui::End();
        }
    }

    return keep_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::DoMenuUI() {
    bool keep_window=true;

    if(ImGui::BeginMainMenuBar()) {
        this->DoFileMenu();
        this->DoEditMenu();
        this->DoToolsMenu();
        this->DoDebugMenu();
        if(!this->DoWindowMenu()) {
            keep_window=false;
        }
        ImGui::EndMainMenuBar();
    }

    return keep_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The table can be in any order. Entries are found by their type
// field (since C++11 doesn't have C99-style designated
// initializers..).
struct BeebWindow::SettingsUIMetadata {
    BeebWindowPopupType type;
    const char *name;
    const char *command_name;
    std::unique_ptr<SettingsUI> (*create_fn)(BeebWindow *beeb_window);
};

const BeebWindow::SettingsUIMetadata BeebWindow::ms_settings_uis[]={
    {BeebWindowPopupType_Keymaps,"Keyboard Layout","toggle_keyboard_layout",&CreateKeymapsUI},
    {BeebWindowPopupType_CommandKeymaps,"Command Keys","toggle_command_keymaps",&CreateCommandKeymapsUI},
    {BeebWindowPopupType_Options,"Options","toggle_emulator_options",&BeebWindow::CreateOptionsUI},
    {BeebWindowPopupType_Messages,"Messages","toggle_messages",&CreateMessagesUI},
    {BeebWindowPopupType_Timeline,"Timeline","toggle_timeline",&BeebWindow::CreateTimelineUI},
    {BeebWindowPopupType_SavedStates,"Saved states","toggle_saved_states",&BeebWindow::CreateSavedStatesUI},
    {BeebWindowPopupType_Configs,"Configurations","toggle_configurations",&CreateConfigsUI},
#if BBCMICRO_TRACE
    {BeebWindowPopupType_Trace,"Tracing","toggle_event_trace",&CreateTraceUI},
#endif
    {BeebWindowPopupType_AudioCallback,"Data Rate","toggle_date_rate",&CreateDataRateUI},
#if VIDEO_TRACK_METADATA
    {BeebWindowPopupType_PixelMetadata,"Pixel Metadata","toggle_pixel_metadata",&CreatePixelMetadataUI},
#endif
#if ENABLE_IMGUI_TEST
    {BeebWindowPopupType_DearImguiTest,"dear imgui Test","toggle_dear_imgui_test",&CreateDearImguiTestUI},
#endif
#if BBCMICRO_DEBUGGER
    {BeebWindowPopupType_6502Debugger,"6502 Debug","toggle_6502_debugger",&Create6502DebugWindow,},
    {BeebWindowPopupType_MemoryDebugger1,"Memory Debug 1","toggle_memory_debugger1",&CreateMemoryDebugWindow,},
    {BeebWindowPopupType_MemoryDebugger2,"Memory Debug 2","toggle_memory_debugger2",&CreateMemoryDebugWindow,},
    {BeebWindowPopupType_MemoryDebugger3,"Memory Debug 3","toggle_memory_debugger3",&CreateMemoryDebugWindow,},
    {BeebWindowPopupType_MemoryDebugger4,"Memory Debug 4","toggle_memory_debugger4",&CreateMemoryDebugWindow,},
    {BeebWindowPopupType_ExtMemoryDebugger1,"External Memory Debug 1","toggle_ext_memory_debugger1",&CreateExtMemoryDebugWindow,},
    {BeebWindowPopupType_ExtMemoryDebugger2,"External Memory Debug 2","toggle_ext_memory_debugger2",&CreateExtMemoryDebugWindow,},
    {BeebWindowPopupType_ExtMemoryDebugger3,"External Memory Debug 3","toggle_ext_memory_debugger3",&CreateExtMemoryDebugWindow,},
    {BeebWindowPopupType_ExtMemoryDebugger4,"External Memory Debug 4","toggle_ext_memory_debugger4",&CreateExtMemoryDebugWindow,},
    {BeebWindowPopupType_DisassemblyDebugger,"Disassembly Debug","toggle_disassembly_debugger",&CreateDisassemblyDebugWindow,},
    {BeebWindowPopupType_CRTCDebugger,"CRTC Debug","toggle_crtc_debugger",&CreateCRTCDebugWindow,},
    {BeebWindowPopupType_VideoULADebugger,"Video ULA Debug","toggle_video_ula_debugger",&CreateVideoULADebugWindow,},
    {BeebWindowPopupType_SystemVIADebugger,"System VIA Debug","toggle_system_via_debugger",&CreateSystemVIADebugWindow,},
    {BeebWindowPopupType_UserVIADebugger,"User VIA Debug","toggle_user_via_debugger",&CreateUserVIADebugWindow,},
    {BeebWindowPopupType_NVRAMDebugger,"NVRAM Debug","toggle_nvram_debugger",&CreateNVRAMDebugWindow,},
#endif

    // Keep this one at the end, because the command context stack is
    // updated as it goes...
    //
    // (Could maybe rearrange things so that this can display the
    // previous frame's stack... but it hardly seems worth the
    // bother.)
    {BeebWindowPopupType_CommandContextStack,"Command Context Stack","toggle_cc_stack",&BeebWindow::CreateCommandContextStackUI},

    // terminator
    {BeebWindowPopupType_MaxValue},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoSettingsUI() {
    for(const SettingsUIMetadata *ui=ms_settings_uis;ui->type!=BeebWindowPopupType_MaxValue;++ui) {
        uint64_t mask=(uint64_t)1<<ui->type;
        bool opened=!!(m_settings.popups&mask);

        if(opened) {
            ImGui::SetNextDock(ImGuiDockSlot_None);
            ImVec2 default_pos=ImVec2(10.f,30.f);
            ImVec2 default_size=ImGui::GetIO().DisplaySize*.4f;

            // The extra flags could be wrong for the first frame
            // after the window is created.
            ImGuiWindowFlags extra_flags=0;
            if(m_popups[ui->type]) {
                extra_flags|=(ImGuiWindowFlags)m_popups[ui->type]->GetExtraImGuiWindowFlags();
            }

            if(ImGui::BeginDock(ui->name,&opened,extra_flags,default_size,default_pos)) {
                if(!m_popups[ui->type]) {
                    m_popups[ui->type]=(*ui->create_fn)(this);
                    ASSERT(!!m_popups[ui->type]);
                }

                m_popups[ui->type]->DoImGui(&m_cc_stack);

                if(m_popups[ui->type]->WantsKeyboardFocus()) {
                    m_imgui_has_kb_focus=true;
                }

                m_settings.popups|=mask;
            }
            ImGui::EndDock();

            if(!opened) {
                m_settings.popups&=~mask;

                // Leave the deletion until the next frame -
                // references to its textures might still be queued up
                // in the dear imgui drawlists.
            }
        } else {
            if(m_popups[ui->type]) {
                this->MaybeSaveConfig(m_popups[ui->type]->OnClose());
                m_popups[ui->type]=nullptr;
            }
        }
    }

    if(ValueChanged(&m_msg_last_num_errors_and_warnings_printed,m_message_list->GetNumErrorsAndWarningsPrinted())) {
        m_settings.popups|=1<<BeebWindowPopupType_Messages;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateOptionsUI(BeebWindow *beeb_window) {
    return std::make_unique<OptionsUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateTimelineUI(BeebWindow *beeb_window) {
    return ::CreateTimelineUI(beeb_window,
                              beeb_window->m_renderer,
                              beeb_window->m_pixel_format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateSavedStatesUI(BeebWindow *beeb_window) {
    return ::CreateSavedStatesUI(beeb_window,
                                 beeb_window->m_renderer,
                                 beeb_window->m_pixel_format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateCommandContextStackUI(BeebWindow *beeb_window) {
    return std::make_unique<CommandContextStackUI>(&beeb_window->m_cc_stack);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoPopupUI(uint64_t now,int output_width,int output_height) {
    (void)output_width;

    if(ValueChanged(&m_msg_last_num_messages_printed,m_message_list->GetNumMessagesPrinted())) {
        m_messages_popup_ui_active=true;
        m_messages_popup_ticks=now;
    }

    if(m_messages_popup_ui_active) {
        ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                //ImGuiWindowFlags_ShowBorders|
                                ImGuiWindowFlags_AlwaysAutoResize|
                                ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f,ImGuiCond_Always,ImVec2(0.5f,0.5f));

        // What's supposed to happen here: the window is 90% of the
        // screen width and as tall as it needs to be ("set axis to
        // 0.0f to force an auto-fit on this axis").
        //
        // What actually happens: it's the right width, but 0 pixels
        // high, and gets saved to imgui.ini that way. Then on the
        // next run, it's 32 pixels high (?) - and that seems to
        // persist for future runs with the same ini file. But 32
        // pixels high is still too short.
        //
        // Fortunately, with AlwaysAutoResize and
        // SetNextWindowPosCenter, the default size is sensible.

        //ImGui::SetNextWindowSize(ImVec2(output_width*0.9f,0));

        if(ImGui::Begin("Recent Messages",&m_messages_popup_ui_active,flags)) {
            m_message_list->ForEachMessage(5,[](MessageList::Message *m) {
                if(!m->seen) {
                    ImGuiMessageListMessage(m);
                }
            });
        }
        ImGui::End();

        if(GetSecondsFromTicks(now-m_messages_popup_ticks)>MESSAGES_POPUP_TIME_SECONDS) {
            m_message_list->ForEachMessage([](MessageList::Message *m) {
                m->seen=true;
            });

            m_messages_popup_ui_active=false;
        }
    }

    bool replaying=false;//m_beeb_thread->IsReplaying();
    bool pasting=m_beeb_thread->IsPasting();
    bool copying=m_beeb_thread->IsCopying();
    if(ValueChanged(&m_leds,m_beeb_thread->GetLEDs())||(m_leds&BBCMicroLEDFlags_AllDrives)||replaying||copying||pasting) {
        m_leds_popup_ui_active=true;
        m_leds_popup_ticks=now;
        //LOGF(OUTPUT,"leds now: 0x%x\n",m_leds);
    }

    if(m_leds_popup_ui_active) {
        ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                //ImGuiWindowFlags_ShowBorders|
                                ImGuiWindowFlags_AlwaysAutoResize|
                                ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::SetNextWindowPos(ImVec2(10.f,output_height-50.f));

        if(ImGui::Begin("LEDs",&m_leds_popup_ui_active,flags)) {
            ImGuiStyleColourPusher colour_pusher;
            colour_pusher.Push(ImGuiCol_CheckMark,ImVec4(1.f,0.f,0.f,1.f));

            ImGuiLED(!!(m_leds&BBCMicroLEDFlag_CapsLock),"Caps Lock");

            ImGui::SameLine();
            ImGuiLED(!!(m_leds&BBCMicroLEDFlag_ShiftLock),"Shift Lock");

            for(int i=0;i<NUM_DRIVES;++i) {
                ImGui::SameLine();
                ImGuiLEDf(!!(m_leds&(uint32_t)BBCMicroLEDFlag_Drive0<<i),"Drive %d",i);
            }

            colour_pusher.Push(ImGuiCol_CheckMark,ImVec4(0.f,1.f,0.f,1.f));

            //ImGui::SameLine();
            ImGuiLED(replaying,"Replay");

            if(replaying) {
                ImGui::SameLine();
                if(ImGui::Button("Stop")) {
                    m_beeb_thread->Send(std::make_unique<BeebThread::StopReplayMessage>());
                }
            }

            ImGui::SameLine();
            ImGuiLED(copying,"Copy");
            if(copying) {
                ImGui::SameLine();
                if(ImGui::Button("Cancel")) {
                    m_beeb_thread->Send(std::make_unique<BeebThread::StopCopyMessage>());
                }
            }

            ImGui::SameLine();
            ImGuiLED(pasting,"Paste");
            if(pasting) {
                ImGui::SameLine();
                if(ImGui::Button("Cancel")) {
                    m_beeb_thread->Send(std::make_unique<BeebThread::StopPasteMessage>());
                }
            }
        }
        ImGui::End();

        if(GetSecondsFromTicks(now-m_leds_popup_ticks)>LEDS_POPUP_TIME_SECONDS) {
            m_leds_popup_ui_active=false;
        }
    }

    std::vector<std::shared_ptr<JobQueue::Job>> jobs=BeebWindows::GetJobs();
    if(!jobs.empty()) {
        bool open=false;

        for(const std::shared_ptr<JobQueue::Job> &job:jobs) {
            if(!job->HasImGui()) {
                continue;
            }

            if(open) {
                ImGui::Separator();
            } else {
                ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                        //ImGuiWindowFlags_ShowBorders|
                                        ImGuiWindowFlags_AlwaysAutoResize|
                                        ImGuiWindowFlags_NoFocusOnAppearing);

                ImGui::SetNextWindowPos(ImVec2(10.f,30.f));

                open=true;

                if(!ImGui::Begin("Jobs",nullptr,flags)) {
                    goto jobs_imgui_done;
                }
            }

            job->DoImGui();

            if(ImGui::Button("Cancel")) {
                job->Cancel();
            }
        }

    jobs_imgui_done:
        if(open) {
            ImGui::End();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoFileMenu() {
    if(ImGui::BeginMenu("File")) {
        m_occ.DoMenuItemUI("hard_reset");

        if(ImGui::BeginMenu("Change config")) {
            bool seen_first_custom=false;
            BeebWindows::ForEachConfig([&](const BeebConfig *config,BeebConfig *editable_config) {
                if(editable_config) {
                    if(!seen_first_custom) {
                        ImGui::Separator();
                        seen_first_custom=true;
                    }
                }

                if(ImGui::MenuItem(config->name.c_str())) {
                    BeebLoadedConfig tmp;

                    if(BeebLoadedConfig::Load(&tmp,*config,&m_msg)) {
                        auto message=std::make_unique<BeebThread::HardResetAndChangeConfigMessage>(0,std::move(tmp));

                        m_beeb_thread->Send(std::move(message));
                    }
                }

                return true;
            });

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if(ImGui::BeginMenu("Keymap")) {
            bool seen_first_custom=false;
            BeebWindows::ForEachBeebKeymap([&](const BeebKeymap *keymap,BeebKeymap *editable_keymap) {
                if(editable_keymap) {
                    if(!seen_first_custom) {
                        ImGui::Separator();
                        seen_first_custom=true;
                    }
                }

                if(ImGui::MenuItem(keymap->GetName().c_str(),keymap->IsKeySymMap()?KEYMAP_KEYSYMS_KEYMAP_ICON:KEYMAP_SCANCODES_KEYMAP_ICON,m_keymap==keymap)) {
                    m_keymap=keymap;
                }

                return true;
            });

            ImGui::EndMenu();
        }

        ImGui::Separator();

        for(int drive=0;drive<NUM_DRIVES;++drive) {
            DriveState *d=&m_drives[drive];

            char title[100];
            snprintf(title,sizeof title,"Drive %d",drive);

            if(ImGui::BeginMenu(title)) {
                std::string name,load_method;
                std::unique_lock<Mutex> d_lock;
                std::shared_ptr<const DiscImage> disc_image=m_beeb_thread->GetDiscImage(&d_lock,drive);

                if(disc_image) {
                    name=disc_image->GetName();
                    ImGui::MenuItem(name.empty()?"(no name)":name.c_str(),nullptr,false,false);

                    std::string desc=disc_image->GetDescription();
                    if(!desc.empty()) {
                        ImGui::MenuItem(("Info: "+desc).c_str(),nullptr,false,false);
                    }

                    std::string hash=disc_image->GetHash();
                    if(!hash.empty()) {
                        ImGui::MenuItem(("SHA1: "+hash).c_str(),nullptr,false,false);
                    }

                    load_method=disc_image->GetLoadMethod();
                    ImGui::MenuItem(("Loaded from: "+load_method).c_str(),nullptr,false,false);
                } else {
                    ImGui::MenuItem("(empty)",NULL,false,false);
                }

                if(!name.empty()) {
                    if(ImGui::MenuItem("Copy path to clipboard")) {
                        SDL_SetClipboardText(name.c_str());
                    }
                }

                ImGui::Separator();

                FileMenuItem file_item(&d->open_disc_image_file_dialog,"Disc image...","Recent disc image");
                if(file_item.selected) {
                    std::shared_ptr<MemoryDiscImage> disc_image=MemoryDiscImage::LoadFromFile(file_item.path,
                                                                                              &m_msg);
                    if(!!disc_image) {
                        m_beeb_thread->Send(std::make_unique<BeebThread::LoadDiscMessage>(drive,
                                                                                          std::move(disc_image),
                                                                                          true));
                        file_item.Success();
                    }
                }

                FileMenuItem direct_item(&d->open_direct_disc_image_file_dialog,
                                         "Direct disc image...",
                                         "Recent direct disc image");
                if(direct_item.selected) {
                    std::shared_ptr<DirectDiscImage> disc_image=DirectDiscImage::CreateForFile(direct_item.path,
                                                                                               &m_msg);
                    if(!!disc_image) {
                        m_beeb_thread->Send(std::make_unique<BeebThread::LoadDiscMessage>(drive,
                                                                                          std::move(disc_image),
                                                                                          true));
                        direct_item.Success();
                    }
                }

                FileMenuItem folder_item(&d->open_65link_folder_dialog,"65Link folder...","Recent 65Link folder");
                if(folder_item.selected) {
                    if(this->Load65LinkFolder(drive,folder_item.path)) {
                        folder_item.Success();
                    }
                }

                if(!!disc_image) {
                    ImGui::Separator();

                    if(disc_image->CanSave()) {
                        if(ImGui::MenuItem("Save")) {
                            disc_image->SaveToFile(disc_image->GetName(),&m_msg);
                        }
                    }

                    if(ImGui::MenuItem("Save copy as...")) {
                        SaveFileDialog fd(RECENT_PATHS_DISC_IMAGE);

                        disc_image->AddFileDialogFilter(&fd);
                        fd.AddAllFilesFilter();

                        std::string path;
                        if(fd.Open(&path)) {
                            if(disc_image->SaveToFile(path,&m_msg)) {
                                fd.AddLastPathToRecentPaths();
                            }
                        }
                    }

                    if(ImGui::MenuItem("Export 65Link folder...")) {
                        FolderDialog fd(RECENT_PATHS_65LINK);

                        std::string path;
                        if(fd.Open(&path)) {
                            if(SaveDiscImageTo65LinkFolder(disc_image,path,&m_msg)) {
                                fd.AddLastPathToRecentPaths();
                            }
                        }
                    }
                }

                ImGui::EndMenu();
            }
        }

        ImGui::Separator();

        if(ImGui::MenuItem("Save NVRAM...",nullptr,false,m_beeb_thread->HasNVRAM())) {
            SaveFileDialog fd(RECENT_PATHS_NVRAM);

            fd.AddFilter("BBC NVRAM data","*.bbcnvram");
            fd.AddAllFilesFilter();

            std::string file_name;
            if(fd.Open(&file_name)) {
                std::vector<uint8_t> nvram=m_beeb_thread->GetNVRAM();

                if(SaveFile(nvram,file_name,&m_msg)) {
                    fd.AddLastPathToRecentPaths();
                }
            }
        }

        //if(ImGui::MenuItem("Save RAM...")) {
        //    SaveFileDialog fd(RECENT_PATHS_RAM);

        //    fd.AddFilter("BBC RAM","*.bbcram");
        //    fd.AddAllFilesFilter();

        //    std::string file_name;
        //    if(fd.Open(&file_name)) {
        //        BBCMicro *beeb=m_beeb_thread->Pause(BeebWindowPauser_SaveRAM);

        //        size_t ram_size=BBCMicro_GetType(beeb)->ram_size;
        //        const uint8_t *ram=BBCMicro_GetRAM(beeb);

        //        if(SaveFile(ram,ram_size,file_name,&m_msg)) {
        //            fd.AddLastPathToRecentPaths();
        //        }

        //        m_beeb_thread->Resume(BeebWindowPauser_SaveRAM,&beeb);
        //    }
        //}

        m_occ.DoMenuItemUI("load_last_state");
        m_occ.DoMenuItemUI("save_state");
        ImGui::Separator();
        m_occ.DoMenuItemUI("exit");
        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoEditMenu() {
    if(ImGui::BeginMenu("Edit")) {
        m_occ.DoMenuItemUI("toggle_copy_oswrch_text");
        m_occ.DoMenuItemUI("copy_basic");

        //m_occ.DoMenuItemUI("toggle_copy_oswrch_binary");
        m_occ.DoMenuItemUI("paste");
        m_occ.DoMenuItemUI("paste_return");

        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoToolsMenu() {
    if(ImGui::BeginMenu("Tools")) {
        m_occ.DoMenuItemUI("toggle_emulator_options");
        m_occ.DoMenuItemUI("toggle_keyboard_layout");
        m_occ.DoMenuItemUI("toggle_command_keymaps");
        m_occ.DoMenuItemUI("toggle_messages");
        m_occ.DoMenuItemUI("toggle_timeline");
        m_occ.DoMenuItemUI("toggle_saved_states");
        m_occ.DoMenuItemUI("toggle_configurations");

        // if(ImGui::MenuItem("Dump states")) {
        //     std::vector<std::shared_ptr<BeebState>> all_states=BeebState::GetAllStates();

        //     for(size_t i=0;i<all_states.size();++i) {
        //         LOGF(OUTPUT,"%zu. ",i);
        //         LOGI(OUTPUT);
        //         LOGF(OUTPUT,"(BeebState *)%p\n",(void *)all_states[i].get());
        //         all_states[i]->Dump(&LOG(OUTPUT));
        //     }
        // }

        ImGui::Separator();
        m_occ.DoMenuItemUI("clean_up_recent_files_lists");
        m_occ.DoMenuItemUI("reset_dock_windows");
        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoDebugMenu() {
#if ENABLE_DEBUG_MENU
    if(ImGui::BeginMenu("Debug")) {
#if ENABLE_IMGUI_TEST
        m_occ.DoMenuItemUI("toggle_dear_imgui_test");
#endif
        m_occ.DoMenuItemUI("toggle_event_trace");
        m_occ.DoMenuItemUI("toggle_date_rate");
        m_occ.DoMenuItemUI("toggle_cc_stack");

#if SYSTEM_WINDOWS
        if(GetConsoleWindow()) {
            m_occ.DoMenuItemUI("clear_console");
            m_occ.DoMenuItemUI("print_separator");
        }
#endif

        m_occ.DoMenuItemUI("dump_timeline_console");
        m_occ.DoMenuItemUI("dump_timeline_debugger");
        m_occ.DoMenuItemUI("check_timeline");
#if VIDEO_TRACK_METADATA
        m_occ.DoMenuItemUI("toggle_pixel_metadata");
#endif

#if BBCMICRO_DEBUGGER
        ImGui::Separator();

        m_occ.DoMenuItemUI("toggle_6502_debugger");
        if(ImGui::BeginMenu("Memory Debug")) {
            m_occ.DoMenuItemUI("toggle_memory_debugger1");
            m_occ.DoMenuItemUI("toggle_memory_debugger2");
            m_occ.DoMenuItemUI("toggle_memory_debugger3");
            m_occ.DoMenuItemUI("toggle_memory_debugger4");
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("External Memory Debug")) {
            m_occ.DoMenuItemUI("toggle_ext_memory_debugger1");
            m_occ.DoMenuItemUI("toggle_ext_memory_debugger2");
            m_occ.DoMenuItemUI("toggle_ext_memory_debugger3");
            m_occ.DoMenuItemUI("toggle_ext_memory_debugger4");
            ImGui::EndMenu();
        }
        m_occ.DoMenuItemUI("toggle_disassembly_debugger");
        m_occ.DoMenuItemUI("toggle_crtc_debugger");
        m_occ.DoMenuItemUI("toggle_video_ula_debugger");
        m_occ.DoMenuItemUI("toggle_system_via_debugger");
        m_occ.DoMenuItemUI("toggle_user_via_debugger");
        m_occ.DoMenuItemUI("toggle_nvram_debugger");

        ImGui::Separator();

        m_occ.DoMenuItemUI("debug_stop");
        m_occ.DoMenuItemUI("debug_run");
        m_occ.DoMenuItemUI("debug_step_over");
        m_occ.DoMenuItemUI("debug_step_in");

#endif

        ImGui::Separator();

#if ENABLE_IMGUI_DEMO
        ImGui::MenuItem("ImGui demo...",NULL,&m_imgui_demo);
#endif
        ImGui::MenuItem("ImGui dock debug...",nullptr,&m_imgui_dock_debug);
#if STORE_DRAWLISTS
        ImGui::MenuItem("ImGui drawlists...",nullptr,&m_imgui_drawlists);
#endif

        ImGui::EndMenu();
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::DoWindowMenu() {
    bool keep_window=true;

    if(ImGui::BeginMenu("Window")) {
        {
            char name[100];
            strlcpy(name,m_name.c_str(),sizeof name);

            if(ImGui::InputText("Name",name,sizeof name,ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll)) {
                BeebWindows::SetBeebWindowName(this,name);
            }
        }

        ImGui::Separator();

        if(ImGui::MenuItem("New")) {
            PushNewWindowMessage(this->GetNewWindowInitArguments());
        }

        if(ImGui::MenuItem("Clone")) {
            BeebWindowInitArguments init_arguments=this->GetNewWindowInitArguments();

            if(m_keymap) {
                init_arguments.keymap_name=m_keymap->GetName();
            }

            init_arguments.settings=m_settings;
            init_arguments.use_settings=true;

            m_beeb_thread->Send(std::make_unique<BeebThread::CloneWindowMessage>(init_arguments));
        }

        ImGui::Separator();

        if(ImGui::BeginMenu("Close")) {
            if(ImGui::MenuItem("Confirm")) {
                keep_window=false;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    return keep_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::UpdateTVTexture(VBlankRecord *vblank_record) {
    OutputDataBuffer<VideoDataUnit> *video_output=m_beeb_thread->GetVideoOutput();

    uint64_t num_units=(uint64_t)(GetSecondsFromTicks(vblank_record->num_ticks)*1e6)*2;
    uint64_t num_units_left=num_units;

    const VideoDataUnit *a,*b;
    size_t na,nb;

    size_t num_units_consumed=0;

    bool update=true;
#if BBCMICRO_DEBUGGER
    if(m_test_pattern) {
        update=false;
    }
#endif

    if(video_output->GetConsumerBuffers(&a,&na,&b,&nb)) {
        bool limited=m_beeb_thread->IsSpeedLimited();

        if(limited) {
            if(na+nb>num_units_left) {
                if(na>num_units_left) {
                    na=num_units_left;
                    nb=0;
                } else {
                    nb=num_units_left-na;
                }
            }
        }

        size_t num_left;
        const size_t MAX_UPDATE_SIZE=200;

        // A.
        num_left=na;
        while(num_left>0) {
            size_t n=num_left;
            if(n>MAX_UPDATE_SIZE) {
                n=MAX_UPDATE_SIZE;
            }

            if(update) {
                m_tv.Update(a,n);
            }

            a+=n;
            video_output->Consume(n);
            num_left-=n;
        }

        // B.
        num_left=nb;
        while(num_left>0) {
            size_t n=num_left;
            if(n>MAX_UPDATE_SIZE) {
                n=MAX_UPDATE_SIZE;
            }

            if(update) {
                m_tv.Update(b,n);
            }

            b+=n;
            video_output->Consume(n);
            num_left-=n;
        }

        num_units_consumed+=na+nb;
    } else {
#if BBCMICRO_DEBUGGER
        if(update) {
            if(m_show_beam_position) {
                m_tv.AddBeamMarker();
            }
        }
#endif
    }

    vblank_record->num_video_units=num_units_consumed;

    if(m_tv_texture) {
        if(const void *tv_texture_data=m_tv.GetTextureData(nullptr)) {
            SDL_UpdateTexture(m_tv_texture,NULL,tv_texture_data,(int)TV_TEXTURE_WIDTH*4);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::VBlankRecord *BeebWindow::NewVBlankRecord(uint64_t ticks) {
    VBlankRecord *vblank_record;

    if(m_vblank_records.size()<NUM_VBLANK_RECORDS) {
        m_vblank_records.emplace_back();
        vblank_record=&m_vblank_records.back();
    } else {
        vblank_record=&m_vblank_records[m_vblank_index];
        m_vblank_index=(m_vblank_index+1)%NUM_VBLANK_RECORDS;
    }

    vblank_record->num_ticks=ticks-m_last_vblank_ticks;
    m_last_vblank_ticks=ticks;

    return vblank_record;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoBeebDisplayUI() {
    //bool opened=m_imgui_stuff->AreAnyDocksDocked();

    double scale_x;
    if(m_settings.correct_aspect_ratio) {
        scale_x=1.2/1.25;
    } else {
        scale_x=1;
    }

    ImGuiWindowFlags flags=ImGuiWindowFlags_NoTitleBar;
    ImVec2 pos;
    ImVec2 size;
    if(!m_settings.display_auto_scale) {
        pos.x=0.f;
        pos.y=0.f;

        size.x=(float)(m_settings.display_manual_scale*TV_TEXTURE_WIDTH*scale_x);
        size.y=(float)(m_settings.display_manual_scale*TV_TEXTURE_HEIGHT);

        flags|=ImGuiWindowFlags_HorizontalScrollbar;

        ImGui::SetNextWindowContentSize(size);
    }

    if(ImGui::BeginDock("Display",nullptr,flags)) {
        ImVec2 padding=GImGui->Style.WindowPadding;

        ImGuiStyleVarPusher vpusher(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));
        if(m_tv_texture) {
#if BEEB_DISPLAY_FILL

            const ImVec2 &size=ImGui::GetWindowSize();
            ImVec2 pos(0.f,0.f);

#else

            ImVec2 window_size=ImGui::GetWindowSize()-padding*2.f;

            if(m_settings.display_auto_scale) {
                double tv_aspect=(TV_TEXTURE_WIDTH*scale_x)/TV_TEXTURE_HEIGHT;

                double width=window_size.x;
                double height=width/tv_aspect;

                if(height>window_size.y) {
                    height=window_size.y;
                    width=height*tv_aspect;
                }

                size.x=(float)width;
                size.y=(float)height;

                pos=(window_size-size)*.5f;

                // Don't fight any half pixel offset.
                pos.x=(float)(int)pos.x;
                pos.y=(float)(int)pos.y;
            }

#endif

            ImGui::SetCursorPos(pos);
            ImVec2 screen_pos=ImGui::GetCursorScreenPos();
            ImGui::Image(m_tv_texture,size);

#if VIDEO_TRACK_METADATA

            m_got_mouse_pixel_metadata=false;

            if(ImGui::IsItemHovered()) {
                ImVec2 mouse_pos=ImGui::GetMousePos();
                mouse_pos-=screen_pos;

                double tx=mouse_pos.x/size.x;
                double ty=mouse_pos.y/size.y;

                if(tx>=0.&&tx<1.&&ty>=0.&&ty<1.) {
                    int x=(int)(tx*TV_TEXTURE_WIDTH);
                    int y=(int)(ty*TV_TEXTURE_HEIGHT);


                    ASSERT(x>=0&&x<TV_TEXTURE_WIDTH);
                    ASSERT(y>=0&&y<TV_TEXTURE_HEIGHT);

                    m_got_mouse_pixel_metadata=true;

                    const VideoDataUnitMetadata *metadata=m_tv.GetTextureMetadata();
                    m_mouse_pixel_metadata=metadata[y*TV_TEXTURE_WIDTH+x];
                    m_got_mouse_pixel_metadata=true;
                }
            }
#endif
    }
}
    ImGui::EndDock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HandleVBlank(uint64_t ticks) {
    ImGuiContextSetter setter(m_imgui_stuff);

    bool keep_window=true;

    VBlankRecord *vblank_record=this->NewVBlankRecord(ticks);

    this->UpdateTVTexture(vblank_record);

    if(!this->DoImGui(ticks)) {
        keep_window=false;
    }

    SDL_RenderClear(m_renderer);

    m_imgui_stuff->Render();

    SDL_RenderPresent(m_renderer);

    bool got_mouse_focus=false;
    {
        SDL_Window *mouse_window=SDL_GetMouseFocus();
        if(mouse_window==m_window) {
            got_mouse_focus=true;
        }
    }

    if(m_pushed_window_padding) {
        ImGui::PopStyleVar(1);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));

    m_imgui_stuff->NewFrame(got_mouse_focus);

    m_pushed_window_padding=true;

    return keep_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::Init() {
    bool good=this->InitInternal();

    if(good) {
        // Insert pre-init messages in their proper place. Then discard
        // them - there's no point keeping them around.
        if(m_init_arguments.preinit_message_list) {
            m_message_list->InsertMessages(*m_init_arguments.preinit_message_list);

            m_init_arguments.preinit_message_list=nullptr;
        }

        return true;
    } else {
        std::shared_ptr<MessageList> msg=MessageList::stdio;

        if(m_init_arguments.preinit_message_list) {
            msg=m_init_arguments.preinit_message_list;
        } else if(m_init_arguments.initiating_window_id!=0) {
            if(BeebWindow *initiating_window=BeebWindows::FindBeebWindowBySDLWindowID(m_init_arguments.initiating_window_id)) {
                msg=initiating_window->GetMessageList();
            }
        }

        msg->InsertMessages(*m_message_list);

        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SaveSettings() {
    m_settings.dock_config=m_imgui_stuff->SaveDockContext();

    BeebWindows::defaults=m_settings;

    this->SavePosition();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SavePosition() {
#if SYSTEM_WINDOWS

    if(m_hwnd) {
        std::vector<uint8_t> placement_data;
        placement_data.resize(sizeof(WINDOWPLACEMENT));

        auto wp=(WINDOWPLACEMENT *)placement_data.data();
        memset(wp,0,sizeof *wp);
        wp->length=sizeof *wp;

        if(GetWindowPlacement((HWND)m_hwnd,wp)) {
            BeebWindows::SetLastWindowPlacementData(std::move(placement_data));
        }
    }

#elif SYSTEM_OSX

    SaveCocoaFrameUsingName(m_nswindow,m_init_arguments.frame_name);

#else

    std::vector<uint8_t> buf=BeebWindows::GetLastWindowPlacementData();
    if(buf.size()!=sizeof(WindowPlacementData)) {
        buf.clear();
        buf.resize(sizeof(WindowPlacementData));
        new(buf.data()) WindowPlacementData;
    }

    auto wp=(WindowPlacementData *)buf.data();

    uint32_t flags=SDL_GetWindowFlags(m_window);

    wp->maximized=!!(flags&SDL_WINDOW_MAXIMIZED);

    if(flags&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN)) {
        // Don't update the size in this case.
    } else {
        SDL_GetWindowPosition(m_window,&wp->x,&wp->y);
        SDL_GetWindowSize(m_window,&wp->width,&wp->height);
    }

    //LOGF(OUTPUT,"Placement; (%d,%d)+(%dx%d); maximized=%s\n",wp->x,wp->y,wp->width,wp->height,BOOL_STR(wp->maximized));

    BeebWindows::SetLastWindowPlacementData(buf);

#endif
    }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::InitInternal() {
    //m_msg.i.f("info init message\n");
    //m_msg.w.f("warning init message\n");
    //m_msg.e.f("error init message\n");

    m_sound_device=m_init_arguments.sound_device;
    ASSERT(m_init_arguments.sound_spec.freq>0);

    // Add some extra space round the edges so the display doesn't have to
    // be scaled down noticeably.
    //
    // 19 is the height of the dear imgui menu bar with the default font.
    // (Ideally this would be retrieved at runtime, but that can't be done
    // until after the window is created.)
    //
    // Maddeningly, this still isn't quite perfect - at least on OS X. It
    // seems like there's a window border that's drawn on top of everything,
    // inside the window? Bleargh. The dear imgui window position is probably
    // wrong as well. Maybe all the border size saving and restoring is
    // causing problems.
    //
    // Anyway, obvious with the test pattern, but in practice not an issue,
    // as the borders are so large...
    m_window=SDL_CreateWindow("",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              TV_TEXTURE_WIDTH+(int)(IMGUI_DEFAULT_STYLE.WindowPadding.x*2.f),
                              19+TV_TEXTURE_HEIGHT+(int)(IMGUI_DEFAULT_STYLE.WindowPadding.y*2.f),
                              SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL);
    if(!m_window) {
        m_msg.e.f("SDL_CreateWindow failed: %s\n",SDL_GetError());
        return false;
    }

    m_renderer=SDL_CreateRenderer(m_window,
                                  m_init_arguments.render_driver_index,
                                  0);
    if(!m_renderer) {
        m_msg.e.f("SDL_CreateRenderer failed: %s\n",SDL_GetError());
        return false;
    }

    m_pixel_format=SDL_AllocFormat(m_init_arguments.pixel_format);
    if(!m_pixel_format) {
        m_msg.e.f("SDL_AllocFormat failed: %s\n",SDL_GetError());
        return false;
    }

    SDL_SetWindowData(m_window,SDL_WINDOW_DATA_NAME,this);

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    SDL_GetWindowWMInfo(m_window,&wmi);

    bool reset_windows=m_init_arguments.reset_windows;
    m_init_arguments.reset_windows=false;

#if SYSTEM_WINDOWS

    m_hwnd=wmi.info.win.window;

    if(!reset_windows) {
        if(m_hwnd) {
            if(m_init_arguments.placement_data.size()==sizeof(WINDOWPLACEMENT)) {
                auto wp=(const WINDOWPLACEMENT *)m_init_arguments.placement_data.data();

                SetWindowPlacement((HWND)m_hwnd,wp);
            }
        }
    }

#elif SYSTEM_OSX

    m_nswindow=wmi.info.cocoa.window;

    if(!reset_windows) {
        SetCocoaFrameUsingName(m_nswindow,m_init_arguments.frame_name);
    }

#else

    if(!reset_windows) {
        if(m_init_arguments.placement_data.size()==sizeof(WindowPlacementData)) {
            auto wp=(const WindowPlacementData *)m_init_arguments.placement_data.data();

            SDL_RestoreWindow(m_window);

            if(wp->x!=INT_MIN&&wp->y!=INT_MIN) {
                SDL_SetWindowPosition(m_window,wp->x,wp->y);
            }

            if(wp->width>0&&wp->height>0) {
                SDL_SetWindowSize(m_window,wp->width,wp->height);
            }

            if(wp->maximized) {
                SDL_MaximizeWindow(m_window);
            }
        }
    }

#endif

    SDL_RendererInfo info;
    if(SDL_GetRendererInfo(m_renderer,&info)<0) {
        m_msg.e.f("SDL_GetRendererInfo failed: %s\n",SDL_GetError());
        return false;
    }

#if RMT_ENABLED
    if(g_num_BeebWindow_inits==0) {
#if RMT_USE_OPENGL
        if(strcmp(info.name,"opengl")==0) {
            rmt_BindOpenGL();
            g_unbind_opengl=1;
}
#endif
}
    ++g_num_BeebWindow_inits;
#endif

    if(!this->RecreateTexture()) {
        return false;
    }

    if(!m_tv.InitTexture(m_pixel_format)) {
        m_msg.e.f("Failed to initialise TVOutput texture\n");
        return false;
    }

    m_imgui_stuff=new ImGuiStuff(m_renderer);
    if(!m_imgui_stuff->Init()) {
        m_msg.e.f("failed to initialise ImGui\n");
        return false;
    }

    //m_beeb_thread=BeebThread_Alloc();

    if(!m_beeb_thread->Start()) {
        m_msg.e.f("Failed to start BBC\n");//: %s",BeebThread_GetError(m_beeb_thread));
        return false;
    }

//    // Need to initialise BeebLoadedConfig here from m_init_arguments.default_config,
//    // now that the timeline is no longer with us...
//    ASSERT(false);

    if(!!m_init_arguments.initial_state) {
        // Load initial state, and use parent timeline event ID (whichever it is).
        m_beeb_thread->Send(std::make_unique<BeebThread::LoadStateMessage>(m_init_arguments.initial_state,
                                                                           false));
    } else {
        std::unique_ptr<BBCMicro> init_beeb=m_init_arguments.default_config.CreateBBCMicro(0);
        auto init_state=std::make_shared<BeebState>(std::move(init_beeb));
        m_beeb_thread->Send(std::make_unique<BeebThread::LoadStateMessage>(init_state,
                                                                           false));
    }

    if(!m_init_arguments.initially_paused) {
        m_beeb_thread->Send(std::make_unique<BeebThread::PauseMessage>(false));
    }

    if(reset_windows) {
        m_imgui_stuff->ResetDockContext();
    }

    m_imgui_stuff->NewFrame(false);

    m_display_size_options.push_back("Auto");

    for(size_t i=1;i<4;++i) {
        char name[100];
        snprintf(name,sizeof name,"%zux (%zux%zu)",i,i*TV_TEXTURE_WIDTH,i*TV_TEXTURE_HEIGHT);

        m_display_size_options.push_back(name);
    }

    if(!m_init_arguments.keymap_name.empty()) {
        m_keymap=BeebWindows::FindBeebKeymapByName(m_init_arguments.keymap_name);
    }

    if(!m_keymap) {
        m_keymap=BeebWindows::GetDefaultBeebKeymap();
    }

    if(SDL_GL_GetCurrentContext()) {
        if(SDL_GL_SetSwapInterval(0)!=0) {
            m_msg.i.f("failed to set GL swap interval to 0: %s\n",SDL_GetError());
        }
    }

    if(!m_settings.dock_config.empty()) {
        if(!m_imgui_stuff->LoadDockContext(m_settings.dock_config)) {
            m_msg.w.f("failed to load dock config\n");
        }
    }

    {
        Uint32 format;
        int width,height;
        SDL_QueryTexture(m_tv_texture,&format,nullptr,&width,&height);
        m_msg.i.f("Renderer: %s, %dx%d %s\n",
                  info.name,
                  width,
                  height,
                  SDL_GetPixelFormatName(format));
    }

    m_msg.i.f("Sound: %s, %dHz %d-channel (%d byte buffer)\n",
              SDL_GetCurrentAudioDriver(),
              m_init_arguments.sound_spec.freq,
              m_init_arguments.sound_spec.channels,
              m_init_arguments.sound_spec.size);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::ThreadFillAudioBuffer(SDL_AudioDeviceID audio_device_id,float *mix_buffer,size_t num_samples) {
    if(!m_beeb_thread->IsStarted()) {
        return;
    }

    if(m_sound_device!=audio_device_id) {
        return;
    }

    m_beeb_thread->AudioThreadFillAudioBuffer(mix_buffer,num_samples,false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::UpdateTitle() {
    if(!m_beeb_thread->IsStarted()) {
        return;
    }

    char title[1000];

#if GOT_CRTDBG
    size_t malloc_bytes=0,malloc_count=0;
    {
        _CrtMemState mem_state;
        _CrtMemCheckpoint(&mem_state);

        for(int i=0;i<_MAX_BLOCKS;++i) {
            malloc_bytes+=mem_state.lSizes[i];
            malloc_count+=mem_state.lCounts[i];
        }
    }
#endif

    double speed=0.0;
    {
        uint64_t num_2MHz_cycles=m_beeb_thread->GetEmulated2MHzCycles();
        uint64_t num_2MHz_cycles_elapsed=num_2MHz_cycles-m_last_title_update_2MHz_cycles;

        uint64_t now=GetCurrentTickCount();
        double secs_elapsed=GetSecondsFromTicks(now-m_last_title_update_ticks);

        if(m_last_title_update_ticks!=0) {
            double hz=num_2MHz_cycles_elapsed/secs_elapsed;
            speed=hz/2.e6;
        }

        m_last_title_update_2MHz_cycles=num_2MHz_cycles;
        m_last_title_update_ticks=now;
    }

    snprintf(title,sizeof title,"%s [%.3fx]",m_name.c_str(),speed);

    SDL_SetWindowTitle(m_window,title);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::BeebKeymapWillBeDeleted(BeebKeymap *keymap) {
    if(m_keymap==keymap) {
        m_keymap=&DEFAULT_KEYMAP;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SDL_Texture *BeebWindow::GetTextureForRenderer(SDL_Renderer *renderer) const {
    if(renderer!=m_renderer) {
        return nullptr;
    }

    return m_tv_texture;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::GetTextureData(BeebWindowTextureDataVersion *version,
                                const SDL_PixelFormat **format_ptr,const void **pixels_ptr) const
{
    uint64_t v;
    *pixels_ptr=m_tv.GetTextureData(&v);
    if(v==version->version) {
        return false;
    }

    *format_ptr=m_tv.GetPixelFormat();

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<BeebThread> BeebWindow::GetBeebThread() const {
    return m_beeb_thread;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<MessageList> BeebWindow::GetMessageList() const {
    return m_message_list;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
void *BeebWindow::GetHWND() const {
    return m_hwnd;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX
void *BeebWindow::GetNSWindow() const {
    return m_nswindow;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<BeebWindow::VBlankRecord> BeebWindow::GetVBlankRecords() const {
    std::vector<BeebWindow::VBlankRecord> records;

    if(m_vblank_records.size()<NUM_VBLANK_RECORDS) {
        records=m_vblank_records;
    } else {
        ASSERT(m_vblank_index<m_vblank_records.size());
        auto &&it=m_vblank_records.begin()+(ptrdiff_t)m_vblank_index;

        records.reserve(m_vblank_records.size());
        records.insert(records.end(),it,m_vblank_records.end());
        records.insert(records.end(),m_vblank_records.begin(),it);
    }

    return records;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebKeymap *BeebWindow::GetCurrentKeymap() const {
    return m_keymap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SetCurrentKeymap(const BeebKeymap *keymap) {
    m_keymap=keymap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if VIDEO_TRACK_METADATA
const VideoDataUnitMetadata *BeebWindow::GetMetadataForMousePixel() const {
    if(m_got_mouse_pixel_metadata) {
        return &m_mouse_pixel_metadata;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks) {
    // There's an API for exactly this on Windows. But it's probably
    // better to have the same code on every platform. 99% of the time
    // (and possibly even more often than that...) this will get the
    // right display.
    int wx,wy;
    SDL_GetWindowPosition(m_window,&wx,&wy);

    int ww,wh;
    SDL_GetWindowSize(m_window,&ww,&wh);

    void *dd=vblank_monitor->GetDisplayDataForPoint(wx+ww/2,wy+wh/2);
    if(dd!=display_data) {
        return true;
    }

    return this->HandleVBlank(ticks);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindowInitArguments BeebWindow::GetNewWindowInitArguments() const {
    BeebWindowInitArguments ia=m_init_arguments;

    // Propagate current name, not original name.
    ia.name=m_name;

    // Caller will choose the initial state.
    ia.initial_state=nullptr;

    // Feed any output to this window's message list.
    ia.initiating_window_id=SDL_GetWindowID(m_window);

    // New window is parent of whatever.
    //ia.parent_timeline_event_id=0;//m_beeb_thread->GetParentTimelineEventId();

    return ia;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::MaybeSaveConfig(bool save_config) {
    if(save_config) {
        this->SaveSettings();

        SaveGlobalConfig(&m_msg);

        m_msg.i.f("Configuration saved.\n");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void BeebWindow::DoOptionsCheckbox(const char *label,bool (BeebThread::*get_mfn)() const,void (BeebThread::*send_mfn)(bool)) {
//    bool old_value=(*m_beeb_thread.*get_mfn)();
//    bool new_value=old_value;
//    ImGui::Checkbox(label,&new_value);
//
//    if(new_value!=old_value) {
//        (*m_beeb_thread.*send_mfn)(new_value);
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HardReset() {
    m_beeb_thread->Send(std::make_unique<BeebThread::HardResetMessage>(BeebThreadHardResetFlag_Run));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void BeebWindow::LoadLastState() {
//    m_beeb_thread->Send(std::make_unique<BeebThread::LoadLastStateMessage>());
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool BeebWindow::IsLoadLastStateEnabled() const {
//    return m_beeb_thread->GetLastSavedStateTimelineId()!=0;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SaveState() {
    m_beeb_thread->Send(std::make_unique<BeebThread::SaveStateMessage>(true));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::RecreateTexture() {
    if(m_tv_texture) {
        SDL_DestroyTexture(m_tv_texture);
        m_tv_texture=nullptr;
    }

    SetRenderScaleQualityHint(m_settings.display_filter);

    m_tv_texture=SDL_CreateTexture(m_renderer,m_pixel_format->format,SDL_TEXTUREACCESS_STREAMING,TV_TEXTURE_WIDTH,TV_TEXTURE_HEIGHT);
    if(!m_tv_texture) {
        m_msg.e.f("Failed to create TV texture: %s\n",SDL_GetError());
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::Exit() {
    this->SaveSettings();

    SDL_Event event={};
    event.type=SDL_QUIT;

    SDL_PushEvent(&event);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::CleanUpRecentFilesLists() {
    size_t n=0;

    n+=CleanUpRecentPaths(RECENT_PATHS_65LINK,&PathIsFolderOnDisk);
    n+=CleanUpRecentPaths(RECENT_PATHS_DISC_IMAGE,&PathIsFileOnDisk);
    n+=CleanUpRecentPaths(RECENT_PATHS_NVRAM,&PathIsFileOnDisk);

    if(n>0) {
        m_msg.i.f("Removed %zu items\n",n);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::ResetDockWindows() {
    m_imgui_stuff->ResetDockContext();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
void BeebWindow::ClearConsole() {
    HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if(!GetConsoleScreenBufferInfo(h,&csbi)) {
        return;
    }

    COORD coord={0,0};
    DWORD num_chars=csbi.dwSize.X*csbi.dwSize.Y,num_written;
    FillConsoleOutputAttribute(h,csbi.wAttributes,num_chars,coord,&num_written);
    FillConsoleOutputCharacter(h,' ',num_chars,coord,&num_written);
    SetConsoleCursorPosition(h,coord);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::PrintSeparator() {
    printf("--------------------------------------------------\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DumpTimelineConsole() {
//    Timeline::Dump(&LOG(OUTPUTND));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DumpTimelineDebuger() {
//    Timeline::Dump(&LOG(OUTPUT));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::CheckTimeline() {
//    Timeline::Check();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<BeebWindowPopupType POPUP_TYPE>
void BeebWindow::TogglePopupCommand() {
    m_settings.popups^=(uint64_t)1<<POPUP_TYPE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<BeebWindowPopupType POPUP_TYPE>
bool BeebWindow::IsPopupCommandTicked() const {
    return !!(m_settings.popups&(uint64_t)1<<POPUP_TYPE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<BeebWindowPopupType POPUP_TYPE>
ObjectCommandTable<BeebWindow>::Initializer BeebWindow::GetTogglePopupCommand() {
    const SettingsUIMetadata *ui;
    for(ui=ms_settings_uis;ui->type!=BeebWindowPopupType_MaxValue;++ui) {
        if(ui->type==POPUP_TYPE) {
            break;
        }
    }

    const char *command_name;
    if(ui->type==BeebWindowPopupType_MaxValue) {
        command_name="?";
    } else {
        command_name=ui->command_name;
    }

    std::string text;
    if(ui->type==BeebWindowPopupType_MaxValue||strlen(ui->name)==0) {
        text="?";
    } else {
        text=std::string(ui->name)+"...";
    }

    return ObjectCommandTable<BeebWindow>::Initializer(CommandDef(command_name,
                                                                  std::move(text)),
                                                       &BeebWindow::TogglePopupCommand<POPUP_TYPE>,
                                                       &BeebWindow::IsPopupCommandTicked<POPUP_TYPE>);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::Paste() {
    this->DoPaste(false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::PasteThenReturn() {
    this->DoPaste(true);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoPaste(bool add_return) {
    if(m_beeb_thread->IsPasting()) {
        m_beeb_thread->Send(std::make_unique<BeebThread::StopPasteMessage>());
    } else {
        // Get UTF-8 clipboard.
        std::vector<uint8_t> utf8;
        {
            char *tmp=SDL_GetClipboardText();
            if(!tmp) {
                m_msg.e.f("Clipboard error: %s\n",SDL_GetError());
                return;
            }

            utf8.resize(strlen(tmp));
            memcpy(utf8.data(),tmp,utf8.size());

            SDL_free(tmp);
            tmp=nullptr;

            if(utf8.empty()) {
                return;
            }
        }

        // Convert UTF-8 into BBC-friendly ASCII.
        std::string ascii;
        {
            uint32_t bad_codepoint;
            const uint8_t *bad_char_start;
            int bad_char_len;
            if(!GetBBCASCIIFromUTF8(&ascii,utf8,&bad_codepoint,&bad_char_start,&bad_char_len)) {
                if(bad_codepoint==0) {
                    m_msg.e.f("Clipboard contents are not valid UTF-8 text\n");
                } else {
                    m_msg.e.f("Invalid character: ");

                    if(bad_codepoint>=32) {
                        m_msg.e.f("'%.*s', ",bad_char_len,bad_char_start);
                    }

                    m_msg.e.f("%u (0x%X)\n",bad_codepoint,bad_codepoint);
                }

                return;
            }
        }

        FixBBCASCIINewlines(&ascii);

        if(add_return) {
            ascii.push_back(13);
        }

        m_beeb_thread->Send(std::make_unique<BeebThread::StartPasteMessage>(std::move(ascii)));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::IsPasteTicked() const {
    return m_beeb_thread->IsPasting();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t VDU_CODE_LENGTHS[32]={
    0,1,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,1,2,5,0,0,1,9,
    8,5,0,0,4,4,0,2,
};

void BeebWindow::SetClipboardData(std::vector<uint8_t> data,bool is_text) {
    if(is_text) {
        // Normalize line endings and strip out control codes.
        //
        // TODO: do it in a less dumb fashion.

        std::vector<uint8_t>::iterator it=data.begin();
        uint8_t delete_counter=0;
        while(it!=data.end()) {
            if(delete_counter>0) {
                it=data.erase(it);
                --delete_counter;
            } else if(*it==10&&it+1!=data.end()&&*(it+1)==13) {
#if SYSTEM_WINDOWS
                // DOS-style line endings.
                *it=13;
                *(it+1)=10;
                it+=2;
#else
                // Unix-style line endings.
                *it++='\n';
                delete_counter=1;
#endif
            } else if(*it<32) {
                delete_counter=VDU_CODE_LENGTHS[*it];
                ++it;
            } else {
                ++it;
            }
        }
    }

    data.push_back(0);

    int rc=SDL_SetClipboardText((const char *)data.data());
    if(rc!=0) {
        m_msg.e.f("Failed to copy to clipboard: %s\n",SDL_GetError());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<bool IS_TEXT>
void BeebWindow::CopyOSWRCH() {
    if(m_beeb_thread->IsCopying()) {
        m_beeb_thread->Send(std::make_unique<BeebThread::StopCopyMessage>());
    } else {
        m_beeb_thread->Send(std::make_unique<BeebThread::StartCopyMessage>([this](std::vector<uint8_t> data) {
            this->SetClipboardData(std::move(data),IS_TEXT);
        },false));//false=not Copy BASIC
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::IsCopyOSWRCHTicked() const {
    return m_beeb_thread->IsCopying();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::CopyBASIC() {
    if(m_beeb_thread->IsCopying()) {
        m_beeb_thread->Send(std::make_unique<BeebThread::StopCopyMessage>());
    } else {
        m_beeb_thread->Send(std::make_unique<BeebThread::StartCopyMessage>([this](std::vector<uint8_t> data) {
            this->SetClipboardData(std::move(data),true);
        },true));//true=Copy BASIC
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::IsCopyBASICEnabled() const {
    return !m_beeb_thread->IsPasting();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebWindow::DebugStop() {
    std::unique_lock<Mutex> lock;
    BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

    m->DebugHalt("manual stop");
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebWindow::DebugRun() {
    std::unique_lock<Mutex> lock;
    BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

    m->DebugRun();
    m_beeb_thread->Send(std::make_unique<BeebThread::DebugWakeUpMessage>());
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebWindow::DebugStepOver() {
    std::unique_lock<Mutex> lock;
    BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

    const M6502 *s=m->GetM6502();
    uint8_t opcode=M6502_GetOpcode(s);
    const M6502DisassemblyInfo *di=&s->config->disassembly_info[opcode];

    if(di->always_step_in) {
        this->DebugStepInLocked(m);
    } else {
        M6502Word next_pc={(uint16_t)(s->opcode_pc.w+di->num_bytes)};
        m->DebugAddTempBreakpoint(next_pc);
        m->DebugRun();
        m_beeb_thread->Send(std::make_unique<BeebThread::DebugWakeUpMessage>());
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebWindow::DebugStepIn() {
    std::unique_lock<Mutex> lock;
    BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

    this->DebugStepInLocked(m);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebWindow::DebugStepInLocked(BBCMicro *m) {
    m->DebugStepIn();
    m->DebugRun();
    m_beeb_thread->Send(std::make_unique<BeebThread::DebugWakeUpMessage>());
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebWindow::DebugIsStopEnabled() const {
    return !this->DebugIsHalted();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebWindow::DebugIsRunEnabled() const {
    return this->DebugIsHalted();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebWindow::DebugIsHalted() const {
    if(!m_got_debug_halted) {
        std::unique_lock<Mutex> lock;
        BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

        m_debug_halted=m->DebugIsHalted();
        m_got_debug_halted=true;
    }

    return m_debug_halted;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ObjectCommandTable<BeebWindow> BeebWindow::ms_command_table("Beeb Window",{
    {{"hard_reset","Hard Reset"},&BeebWindow::HardReset},
//    {{"load_last_state","Load Last State"},&BeebWindow::LoadLastState,nullptr,&BeebWindow::IsLoadLastStateEnabled},
    {{"save_state","Save State"},&BeebWindow::SaveState},
    GetTogglePopupCommand<BeebWindowPopupType_Options>(),
    GetTogglePopupCommand<BeebWindowPopupType_Keymaps>(),
    GetTogglePopupCommand<BeebWindowPopupType_Timeline>(),
    GetTogglePopupCommand<BeebWindowPopupType_SavedStates>(),
    GetTogglePopupCommand<BeebWindowPopupType_Messages>(),
    GetTogglePopupCommand<BeebWindowPopupType_Configs>(),
#if BBCMICRO_TRACE
    GetTogglePopupCommand<BeebWindowPopupType_Trace>(),
#endif
    GetTogglePopupCommand<BeebWindowPopupType_AudioCallback>(),
    GetTogglePopupCommand<BeebWindowPopupType_CommandContextStack>(),
    GetTogglePopupCommand<BeebWindowPopupType_CommandKeymaps>(),
    {CommandDef("exit","Exit").MustConfirm(),&BeebWindow::Exit},
    {CommandDef("clean_up_recent_files_lists","Clean up recent files lists").MustConfirm(),&BeebWindow::CleanUpRecentFilesLists},
    {CommandDef("reset_dock_windows","Reset dock windows").MustConfirm(),&BeebWindow::ResetDockWindows},
#if SYSTEM_WINDOWS
    {{"clear_console","Clear Win32 console"},&BeebWindow::ClearConsole},
#endif
    {{"print_separator","Print stdout separator"},&BeebWindow::PrintSeparator},
    {{"dump_timeline_console","Dump timeline to console only"},&BeebWindow::DumpTimelineConsole},
    {{"dump_timeline_debugger","Dump timeline to console+debugger"},&BeebWindow::DumpTimelineDebuger},
    {{"check_timeline","Check timeline"},&BeebWindow::CheckTimeline},
    {{"paste","OSRDCH Paste"},&BeebWindow::Paste,&BeebWindow::IsPasteTicked},
    {{"paste_return","OSRDCH Paste (+Return)"},&BeebWindow::PasteThenReturn,&BeebWindow::IsPasteTicked},
    {{"toggle_copy_oswrch_text","OSWRCH Copy Text"},&BeebWindow::CopyOSWRCH<true>,&BeebWindow::IsCopyOSWRCHTicked},
    {{"copy_basic","Copy BASIC listing"},&BeebWindow::CopyBASIC,&BeebWindow::IsCopyOSWRCHTicked,&BeebWindow::IsCopyBASICEnabled},
#if VIDEO_TRACK_METADATA
    GetTogglePopupCommand<BeebWindowPopupType_PixelMetadata>(),
#endif
#if ENABLE_IMGUI_TEST
    GetTogglePopupCommand<BeebWindowPopupType_DearImguiTest>(),
#endif
#if BBCMICRO_DEBUGGER
    GetTogglePopupCommand<BeebWindowPopupType_6502Debugger>(),
    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger1>(),
    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger2>(),
    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger3>(),
    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger4>(),
    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger1>(),
    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger2>(),
    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger3>(),
    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger4>(),
    GetTogglePopupCommand<BeebWindowPopupType_DisassemblyDebugger>(),
    GetTogglePopupCommand<BeebWindowPopupType_CRTCDebugger>(),
    GetTogglePopupCommand<BeebWindowPopupType_VideoULADebugger>(),
    GetTogglePopupCommand<BeebWindowPopupType_SystemVIADebugger>(),
    GetTogglePopupCommand<BeebWindowPopupType_UserVIADebugger>(),
    GetTogglePopupCommand<BeebWindowPopupType_NVRAMDebugger>(),

    {CommandDef("debug_stop","Stop").Shortcut(SDLK_F5|PCKeyModifier_Shift),&BeebWindow::DebugStop,nullptr,&BeebWindow::DebugIsStopEnabled},
    {CommandDef("debug_run","Run").Shortcut(SDLK_F5),&BeebWindow::DebugRun,nullptr,&BeebWindow::DebugIsRunEnabled},
    {CommandDef("debug_step_over","Step Over").Shortcut(SDLK_F10),&BeebWindow::DebugStepOver,nullptr,&BeebWindow::DebugIsRunEnabled},
    {CommandDef("debug_step_in","Step In").Shortcut(SDLK_F11),&BeebWindow::DebugStepIn,nullptr,&BeebWindow::DebugIsRunEnabled},
#endif
});
