#include <shared/system.h>
#include <shared/system_specific.h>
#include "BeebWindow.h"
#include <beeb/OutputData.h>
#include "Remapper.h"
#include <beeb/conf.h>
#include <mutex>
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
#include "NVRAMUI.h"
#include <IconsFontAwesome.h>
#include "DataRateUI.h"
#include "Timeline.h"
#include <shared/path.h>
#include "CommandContextStackUI.h"
#include "CommandKeymapsUI.h"

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
    open_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE)
{
    AddDiscImagesFileDialogFilter(&this->open_disc_image_file_dialog);
    this->open_disc_image_file_dialog.AddAllFilesFilter();
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

    m_beeb_thread=std::make_shared<BeebThread>(
        m_message_list,
        init_arguments.sound_device,
        init_arguments.sound_spec.freq,
        init_arguments.sound_spec.samples);

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

    delete m_imgui_stuff;
    m_imgui_stuff=nullptr;

#if TIMELINE_UI_ENABLED
    // Clean this up early... it maintains its own textures, and they
    // want to be destroyed before the renderer goes away.
    m_timeline_ui=nullptr;
#endif

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
    ASSERT(g_num_BeebWindow_inits>0);
    --g_num_BeebWindow_inits;
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

void BeebWindow::HandleSDLKeyEvent(const SDL_KeyboardEvent &event) {
    //printf("%s: scancode=%s key=%s mod=0x%X state=%s\n",__func__,SDL_GetScancodeName(keysym.scancode),SDL_GetKeyName(keysym.sym),keysym.mod,BOOL_STR(state));
    bool state=event.type==SDL_KEYDOWN;

    LOGF(OUTPUT,"%s: key=%s state=%s timestamp=%u\n",__func__,SDL_GetScancodeName(event.keysym.scancode),BOOL_STR(state),event.timestamp);

    m_imgui_stuff->SetKeyDown(event.keysym.scancode,state);

    if(m_imgui_has_kb_focus) {
        // Let dear imgui have the keypress.
        return;
    }

    bool prefer_shortcuts=m_keymap->GetPreferShortcuts();

    uint32_t keycode=0;
    if(state) {
        keycode=(uint32_t)event.keysym.sym|GetPCKeyModifiersFromSDLKeymod(event.keysym.mod);
    }

    if(prefer_shortcuts&&keycode!=0) {
        if(m_cc_stack.ExecuteCommandsForPCKey(keycode)) {
            // The emulator may later get key up messages for this
            // key, but that is (ought to be...) OK.
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
                    m_beeb_thread->SendKeySymMessage(beeb_keysym,false);
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
                m_beeb_thread->SendKeySymMessage((BeebKeySym)*beeb_sym,state);
            }
        }
    } else {
        const int8_t *beeb_keys=keymap->GetValuesForPCKey(keysym.scancode);
        if(!beeb_keys) {
            return false;
        }

        for(const int8_t *beeb_key=beeb_keys;*beeb_key>=0;++beeb_key) {
            m_beeb_thread->SendKeyMessage((BeebKey)*beeb_key,state);
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

bool BeebWindow::LoadDiscImageFile(int drive,const std::string &path) {
    std::unique_ptr<MemoryDiscImage> disc_image=MemoryDiscImage::LoadFromFile(path,&m_msg);
    if(!disc_image) {
        //m_msg.w.f("Failed to load %s: %s\n",path.c_str(),error.c_str());
        return false;
    } else {
        m_beeb_thread->SendLoadDiscMessage(drive,std::move(disc_image),true);
        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::Load65LinkFolder(int drive,const std::string &path) {
    std::string error;
    std::unique_ptr<DiscImage> disc_image=LoadDiscImageFrom65LinkFolder(path,&m_msg);
    if(!disc_image) {
        //m_msg.w.f("Failed to load %s: %s\n",path.c_str(),error.c_str());
        return false;
    } else {
        m_beeb_thread->SendLoadDiscMessage(drive,std::move(disc_image),true);
        return true;
    }
}

void BeebWindow::DoOptionsGui() {
    {
        bool paused=m_beeb_thread->IsPaused();
        if(ImGui::Checkbox("Paused",&paused)) {
            m_beeb_thread->SetPaused(paused);
        }
    }

    this->DoOptionsCheckbox("Limit speed",&BeebThread::IsSpeedLimited,&BeebThread::SendSetSpeedLimitingMessage);
    this->DoOptionsCheckbox("Turbo disc",&BeebThread::IsTurboDisc,&BeebThread::SendSetTurboDiscMessage);

    //    bool ImGui::Combo(const char* label, int* current_item, bool (*items_getter)(void*, int, const char**), void* data, int items_count, int height_in_items)

    // Manually do the scale minimum, since ImGui lets you type in
    // out-of-range values and you don't want 0.f.

    static const float MIN_SCALE=.1f;
    static const float MAX_SCALE=3.f;
    static const float SCALE_STEP=.01f;

    ImGui::DragFloat("Width scale",&m_settings.display_scale_x,SCALE_STEP,0.f,MAX_SCALE);
    m_settings.display_scale_x=std::max(m_settings.display_scale_x,MIN_SCALE);

    ImGui::DragFloat("Height scale",&m_settings.display_scale_y,SCALE_STEP,0.f,MAX_SCALE);
    m_settings.display_scale_y=std::max(m_settings.display_scale_y,MIN_SCALE);

    ImGui::Checkbox("Auto display size",&m_settings.display_auto_scale);
    if(!m_settings.display_overall_scale) {
        ImGui::DragFloat("Overall scale",&m_settings.display_overall_scale,SCALE_STEP,0.f,MAX_SCALE);
        m_settings.display_overall_scale=std::max(m_settings.display_overall_scale,MIN_SCALE);
    }

    if(ImGui::Checkbox("Filter display",&m_settings.display_filter)) {
        this->RecreateTexture();
    }

    //ImGuiSliderGetSet("Mode 7 gamma",&m_tv,&TVOutput::GetGamma,&TVOutput::SetGamma,1.f,10.f,"%.3f");

    if(ImGui::SliderFloat("BBC volume",&m_settings.bbc_volume,MIN_DB,MAX_DB,"%.1f dB")) {
        m_beeb_thread->SetBBCVolume(m_settings.bbc_volume);
    }

    if(ImGui::SliderFloat("Disc volume",&m_settings.disc_volume,MIN_DB,MAX_DB,"%.1f dB")) {
        m_beeb_thread->SetDiscVolume(m_settings.disc_volume);
    }

    if(ImGui::CollapsingHeader("Display Alignment",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImVec2 size(20.f,20.f);

        for(int y=0;y<3;++y) {
            ImGui::NewLine();
            for(int x=0;x<3;++x) {
                const char *caption="";
                if(x==m_settings.display_alignment_x&&y==m_settings.display_alignment_y) {
                    caption="*";
                }

                {
                    ImGuiIDPusher id_pusher(y*3+x);
                    if(ImGui::Button(caption,size)) {
                        m_settings.display_alignment_x=(BeebWindowDisplayAlignment)x;
                        m_settings.display_alignment_y=(BeebWindowDisplayAlignment)y;
                    }
                }

                ImGui::SameLine();
            }
        }

        //ImGui::End();
    }

    ImGui::NewLine();

    if(ImGui::CollapsingHeader("Debug Flags",ImGuiTreeNodeFlags_DefaultOpen)) {
        uint32_t flags=m_beeb_thread->GetDebugFlags();
        bool any_changed=false;

        for(uint32_t i=0;i<32;++i) {
            uint32_t mask=1u<<i;
            const char *name=GetBBCMicroDebugFlagEnumName((int)mask);
            if(name[0]=='?') {
                continue;
            }

            bool value=!!(flags&mask);
            if(ImGui::Checkbox(name,&value)) {
                any_changed=true;

                flags&=~mask;
                if(value) {
                    flags|=mask;
                }
            }
        }

        if(any_changed) {
            m_beeb_thread->SendDebugFlagsMessage(flags);
        }
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

#if SYSTEM_WINDOWS
#if ENABLE_DEBUG_MENU
static void ClearConsole() {
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
#endif

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

void BeebWindow::DoSettingsUI(uint32_t ui_flag,const char *name,std::unique_ptr<SettingsUI> *uptr,std::function<std::unique_ptr<SettingsUI>()> create_fun) {
    if(m_settings.ui_flags&ui_flag) {
        if(!*uptr) {
            *uptr=create_fun();
        }

        if(ImGuiBeginFlag(name,&m_settings.ui_flags,ui_flag)) {
            (*uptr)->DoImGui();

            if((*uptr)->WantsKeyboardFocus()) {
                m_imgui_has_kb_focus=true;
            }
        }
    } else if(!!*uptr) {
        this->MaybeSaveConfig((*uptr)->DidConfigChange());
        *uptr=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::DoImGui(int output_width,int output_height) {
    (void)output_width,(void)output_height;
    const uint64_t now=GetCurrentTickCount();

    bool keep_window=true;

    m_imgui_has_kb_focus=false;

    m_cc_stack.Reset();
    m_cc_stack.Push(m_occ,true);//true=force push

    if(m_imgui_stuff->WantCaptureKeyboard()) {
        m_imgui_has_kb_focus=true;
    }

    if(m_imgui_stuff->WantTextInput()) {
        m_imgui_has_kb_focus=true;
    }

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

#if ENABLE_IMGUI_DEMO
    if(m_imgui_demo) {
        ImGui::ShowTestWindow();
    }
#endif

    this->DoSettingsUI(BeebWindowUIFlag_Keymaps,"Keyboard layout",&m_keymaps_ui,[this]() {
        return KeymapsUI::Create(this);
    });

    this->DoSettingsUI(BeebWindowUIFlag_CommandKeymaps,"Command Keys",&m_command_keymaps_ui,[]() {
        return std::make_unique<CommandKeymapsUI>();
    });


    //if(m_settings.ui_flags&BeebWindowUIFlag_Keymaps) {
    //    if(!m_keymaps_ui) {
    //        m_keymaps_ui=KeymapsUI::Create();
    //        m_keymaps_ui->SetWindowDetails(this);
    //    }

    //    if(ImGuiBeginFlag("Keyboard layout",&m_settings.ui_flags,BeebWindowUIFlag_Keymaps)) {
    //        m_keymaps_ui->SetCurrentBeebKeymap(m_keymap);
    //        m_keymaps_ui->DoImGui();
    //        m_keymap=m_keymaps_ui->GetCurrentBeebKeymap();

    //        if(m_keymaps_ui->WantsKeyboardFocus()) {
    //            m_imgui_has_kb_focus=true;
    //        }
    //    }
    //    ImGui::End();
    //} else if(!!m_keymaps_ui) {
    //    this->MaybeSaveConfig(m_keymaps_ui->DidConfigChange());
    //    m_keymaps_ui=nullptr;
    //}

    //if(m_settings.ui_flags&BeebWindowUIFlag_CommandKeymaps) {
    //    if(!m_command_keymaps_ui) {
    //        m_command_keymaps_ui=std::make_unique<CommandKeymapsUI>();
    //    }

    //    if(ImGuiBeginFlag("Command Keys",&m_settings.ui_flags,BeebWindowUIFlag_CommandKeymaps)) {
    //        m_command_keymaps_ui->DoImGui();

    //        if(m_command_keymaps_ui->WantsKeyboardFocus()) {
    //            m_imgui_has_kb_focus=true;
    //        }
    //    }
    //    ImGui::End();
    //} else if(!!m_command_keymaps_ui) {
    //    this->MaybeSaveConfig(m_command_keymaps_ui->DidConfigChange());
    //    m_command_keymaps_ui=nullptr;
    //}

    if(m_settings.ui_flags&BeebWindowUIFlag_Options) {
        if(ImGuiBeginFlag("Options",&m_settings.ui_flags,BeebWindowUIFlag_Options)) {
            this->DoOptionsGui();
        }
        ImGui::End();

        if(!(m_settings.ui_flags&BeebWindowUIFlag_Options)) {
            this->MaybeSaveConfig(true);
        }
    }

    if(m_settings.ui_flags&BeebWindowUIFlag_Messages) {
        if(!m_messages_ui) {
            m_messages_ui=MessagesUI::Create();
            m_messages_ui->SetMessageList(m_message_list);
        }

        if(ImGuiBeginFlag("Messages",&m_settings.ui_flags,BeebWindowUIFlag_Messages)) {
            m_messages_ui->DoImGui(&m_cc_stack);
        }
        ImGui::End();
    } else {
        m_messages_ui=nullptr;
    }

#if TIMELINE_UI_ENABLED
    if(m_settings.ui_flags&BeebWindowUIFlag_Timeline) {
        if(!m_timeline_ui) {
            m_timeline_ui=TimelineUI::Create();
            m_timeline_ui->SetWindowDetails(this,this->GetNewWindowInitArguments());
            m_timeline_ui->SetRenderDetails(m_renderer,m_pixel_format);
        }

        if(ImGuiBeginFlag("Timeline",&m_settings.ui_flags,BeebWindowUIFlag_Timeline)) {
            m_timeline_ui->DoImGui();

            // if(std::shared_ptr<BeebState> state=m_timeline_ui->GetSelectedState()) {
            //     m_beeb_thread->SendLoadStateMessage(std::move(state));
            // }
        }
        ImGui::End();
    } else {
        m_timeline_ui=nullptr;
    }
#endif

    if(m_settings.ui_flags&BeebWindowUIFlag_Configs) {
        if(!m_configs_ui) {
            m_configs_ui=ConfigsUI::Create();
        }

        if(ImGuiBeginFlag("Configurations",&m_settings.ui_flags,BeebWindowUIFlag_Configs)) {
            m_configs_ui->DoImGui();
        }
        ImGui::End();
    } else if(!!m_configs_ui) {
        this->MaybeSaveConfig(m_configs_ui->DidConfigChange());
        m_configs_ui=nullptr;
    }

#if BBCMICRO_TRACE
    if(m_settings.ui_flags&BeebWindowUIFlag_Trace) {
        if(!m_trace_ui) {
            m_trace_ui=std::make_unique<TraceUI>(this);
        }

        if(ImGuiBeginFlag("Tracing",&m_settings.ui_flags,BeebWindowUIFlag_Trace)) {
            m_trace_ui->DoImGui();
        }
        ImGui::End();
    } else if(!!m_trace_ui) {
        this->MaybeSaveConfig(m_trace_ui->DoClose());
        m_trace_ui=nullptr;
    }
#endif

    if(m_settings.ui_flags&BeebWindowUIFlag_NVRAM) {
        if(!m_nvram_ui) {
            m_nvram_ui=std::make_unique<NVRAMUI>(this);
        }

        if(ImGuiBeginFlag("Non-volatile RAM",&m_settings.ui_flags,BeebWindowUIFlag_NVRAM)) {
            m_nvram_ui->DoImGui();
        }
        ImGui::End();
    } else {
        m_nvram_ui=nullptr;
    }

    if(m_settings.ui_flags&BeebWindowUIFlag_AudioCallback) {
        if(!m_data_rate_ui) {
            m_data_rate_ui=std::make_unique<DataRateUI>(this);
        }

        if(ImGuiBeginFlag("Data Rate",&m_settings.ui_flags,BeebWindowUIFlag_AudioCallback)) {
            m_data_rate_ui->DoImGui();
        }
        ImGui::End();
    } else {
        m_data_rate_ui=nullptr;
    }

    if(m_settings.ui_flags&BeebWindowUIFlag_CommandContextStack) {
        if(!m_cc_stack_ui) {
            m_cc_stack_ui=std::make_unique<CommandContextStackUI>(&m_cc_stack);
        }

        if(ImGuiBeginFlag("Command Context Stack",&m_settings.ui_flags,BeebWindowUIFlag_CommandContextStack)) {
            m_cc_stack_ui->DoImGui();
        }
        ImGui::End();
    } else {
        m_cc_stack_ui=nullptr;
    }

    if(ValueChanged(&m_msg_last_num_errors_and_warnings_printed,m_message_list->GetNumErrorsAndWarningsPrinted())) {
        m_settings.ui_flags|=BeebWindowUIFlag_Messages;
    }

    if(ValueChanged(&m_msg_last_num_messages_printed,m_message_list->GetNumMessagesPrinted())) {
        m_messages_popup_ui_active=true;
        m_messages_popup_ticks=now;
    }

    if(m_messages_popup_ui_active) {
        ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                ImGuiWindowFlags_ShowBorders|
                                ImGuiWindowFlags_AlwaysAutoResize|
                                ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::SetNextWindowPosCenter();

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
            m_message_list->ForEachMessage(5,[this](MessageList::Message *m) {
                if(!m->seen) {
                    MessagesUI::DoMessageImGui(m);
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

    bool replaying=m_beeb_thread->IsReplaying();
    if(ValueChanged(&m_leds,m_beeb_thread->GetLEDs())||(m_leds&BBCMicroLEDFlags_AllDrives)||replaying||m_beeb_thread->IsCopying()||m_beeb_thread->IsPasting()) {
        m_leds_popup_ui_active=true;
        m_leds_popup_ticks=now;
        //LOGF(OUTPUT,"leds now: 0x%x\n",m_leds);
    }

    if(m_leds_popup_ui_active) {
        ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                ImGuiWindowFlags_ShowBorders|
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
                if(ImGui::Button("Cancel")) {
                    m_beeb_thread->SendCancelReplayMessage();
                }
            }

            ImGui::SameLine();
            ImGuiLED(m_beeb_thread->IsCopying(),"Copy");

            ImGui::SameLine();
            ImGuiLED(m_beeb_thread->IsPasting(),"Paste");
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
                                        ImGuiWindowFlags_ShowBorders|
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

    return keep_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoFileMenu() {
    if(ImGui::BeginMenu("File")) {
        m_occ.DoMenuItemUI("hard_reset");

        if(ImGui::BeginMenu("Change config")) {
            bool seen_first_custom=false;
            BeebWindows::ForEachConfig([&](BeebConfig *config,const BeebLoadedConfig *loaded_config) {
                const char *name;

                if(config) {
                    if(!seen_first_custom) {
                        ImGui::Separator();
                        seen_first_custom=true;
                    }

                    name=config->name.c_str();
                } else {
                    name=loaded_config->config.name.c_str();
                }

                if(ImGui::MenuItem(name)) {
                    BeebLoadedConfig tmp;

                    if(!loaded_config) {
                        if(BeebWindows::GetLoadedConfigForConfig(&tmp,config,&m_msg)) {
                            loaded_config=&tmp;
                        }
                    }

                    if(loaded_config) {
                        m_beeb_thread->SendChangeConfigMessage(*loaded_config);
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

                if(ImGui::MenuItem(keymap->GetName().c_str(),keymap->IsKeySymMap()?KeymapsUI::KEYSYMS_KEYMAP_ICON:KeymapsUI::SCANCODES_KEYMAP_ICON,m_keymap==keymap)) {
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
                std::unique_lock<std::mutex> d_lock;
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
                    if(this->LoadDiscImageFile(drive,file_item.path)) {
                        file_item.Success();
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

                    if(load_method==MemoryDiscImage::LOAD_METHOD_FILE) {
                        if(ImGui::MenuItem("Save disc image")) {
                            disc_image->SaveToFile(disc_image->GetName(),&m_msg);
                        }
                    }

                    if(ImGui::MenuItem("Save disc image as...")) {
                        SaveFileDialog fd(RECENT_PATHS_DISC_IMAGE);

                        disc_image->AddFileDialogFilter(&fd);
                        fd.AddAllFilesFilter();

                        std::string path;
                        if(fd.Open(&path)) {
                            if(disc_image->SaveToFile(path,&m_msg)) {
                                fd.AddLastPathToRecentPaths();

                                m_beeb_thread->SendSetDiscImageNameAndLoadMethodMessage(drive,std::move(path),MemoryDiscImage::LOAD_METHOD_FILE);
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
#if TIMELINE_UI_ENABLED
        m_occ.DoMenuItemUI("toggle_timeline");
#endif
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
        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoDebugMenu() {
#if ENABLE_DEBUG_MENU
    if(ImGui::BeginMenu("Debug")) {
#if ENABLE_IMGUI_DEMO
        ImGui::MenuItem("ImGui demo...",NULL,&m_imgui_demo);
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

            m_beeb_thread->SendCloneWindowMessage(init_arguments);
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

static int GetAlignedCoordinate(BeebWindowDisplayAlignment alignment,int size,int dimension) {
    switch(alignment) {
    default:
        ASSERT(false);
        // fall through
    case BeebWindowDisplayAlignment_Centre:
        return (size-dimension)/2;

    case BeebWindowDisplayAlignment_Min:
        return 0;

    case BeebWindowDisplayAlignment_Max:
        return size-dimension;
    }
}

bool BeebWindow::HandleVBlank(uint64_t ticks) {
    bool keep_window=true;

    ImGuiContextSetter setter(m_imgui_stuff);

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

    {
        OutputDataBuffer<VideoDataUnit> *video_output=m_beeb_thread->GetVideoOutput();

        uint64_t num_us=(uint64_t)(GetSecondsFromTicks(vblank_record->num_ticks)*1e6);
        uint64_t num_us_left=num_us;

        const VideoDataUnit *a,*b;
        size_t na,nb;

        size_t num_units=0;

        if(video_output->ConsumerLock(&a,&na,&b,&nb)) {
            bool limited=m_beeb_thread->IsSpeedLimited();

            size_t n;

            n=na;
            if(limited) {
                n=(size_t)std::min((uint64_t)n,num_us_left);
            }
            m_tv.Update(a,n);
            num_us_left-=n;

            n=nb;
            if(limited) {
                n=(size_t)std::min((uint64_t)n,num_us_left);
            }
            m_tv.Update(b,n);
            num_us_left-=n;

            if(limited) {
                ASSERT(num_us>=num_us_left);
                num_units=num_us-num_us_left;
            } else {
                num_units=na+nb;
            }

            video_output->ConsumerUnlock(num_units);
        }

        vblank_record->num_video_units=num_units;
    }

    if(m_tv_texture) {
        if(const void *tv_texture_data=m_tv.GetTextureData(nullptr)) {
            SDL_UpdateTexture(m_tv_texture,NULL,tv_texture_data,(int)TV_TEXTURE_WIDTH*4);
        }
    }

    int output_width,output_height;
    SDL_GetRendererOutputSize(m_renderer,&output_width,&output_height);

    m_beeb_thread->FlushCallbacks();

    if(!this->DoImGui(output_width,output_height)) {
        keep_window=false;
    }

    SDL_Rect dest_rect;

    if(m_settings.display_auto_scale) {
        double tv_aspect=(TV_TEXTURE_WIDTH*m_settings.display_scale_x)/(TV_TEXTURE_HEIGHT*m_settings.display_scale_y);

        dest_rect.w=output_width;
        dest_rect.h=(int)(dest_rect.w/tv_aspect);

        if(dest_rect.h>output_height) {
            dest_rect.h=output_height;
            dest_rect.w=(int)(dest_rect.h*tv_aspect);
        }
    } else {
        dest_rect.w=(int)(TV_TEXTURE_WIDTH*m_settings.display_scale_x*m_settings.display_overall_scale);
        dest_rect.h=(int)(TV_TEXTURE_HEIGHT*m_settings.display_scale_y*m_settings.display_overall_scale);
    }

    dest_rect.x=GetAlignedCoordinate(m_settings.display_alignment_x,output_width,dest_rect.w);
    dest_rect.y=GetAlignedCoordinate(m_settings.display_alignment_y,output_height,dest_rect.h);

    SDL_RenderClear(m_renderer);
    if(m_tv_texture) {
        SDL_RenderCopy(m_renderer,m_tv_texture,NULL,&dest_rect);
    }
    m_imgui_stuff->Render();
    SDL_RenderPresent(m_renderer);

    bool got_mouse_focus=false;
    {
        SDL_Window *mouse_window=SDL_GetMouseFocus();
        if(mouse_window==m_window) {
            got_mouse_focus=true;
        }
    }

    m_imgui_stuff->NewFrame(got_mouse_focus);

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

    m_window=SDL_CreateWindow("",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              (int)(TV_TEXTURE_WIDTH*m_settings.display_scale_x),
                              (int)(TV_TEXTURE_HEIGHT*m_settings.display_scale_y),
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

#if SYSTEM_WINDOWS

    m_hwnd=wmi.info.win.window;

    if(m_hwnd) {
        if(m_init_arguments.placement_data.size()==sizeof(WINDOWPLACEMENT)) {
            auto wp=(const WINDOWPLACEMENT *)m_init_arguments.placement_data.data();

            SetWindowPlacement((HWND)m_hwnd,wp);
        }
    }

#elif SYSTEM_OSX

    m_nswindow=wmi.info.cocoa.window;
    SetCocoaFrameUsingName(m_nswindow,m_init_arguments.frame_name);

#else

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

    if(!!m_init_arguments.initial_state) {
        // Load initial state, and use parent timeline event ID (whichever it is).
        m_beeb_thread->SendLoadStateMessage(m_init_arguments.parent_timeline_event_id,m_init_arguments.initial_state);
    } else {
        if(m_init_arguments.parent_timeline_event_id==0) {
            // Create new root in timeline using config from init
            // arguments.
            m_init_arguments.parent_timeline_event_id=Timeline::AddEvent(0,BeebEvent::MakeRoot(m_init_arguments.default_config));
        }

        // Start from the requested timeline node.
        m_beeb_thread->SendGoToTimelineNodeMessage(m_init_arguments.parent_timeline_event_id);
    }


    if(!m_init_arguments.initially_paused) {
        m_beeb_thread->SetPaused(false);
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
    ia.parent_timeline_event_id=0;//m_beeb_thread->GetParentTimelineEventId();

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

void BeebWindow::DoOptionsCheckbox(const char *label,bool (BeebThread::*get_mfn)() const,void (BeebThread::*send_mfn)(bool)) {
    bool old_value=(*m_beeb_thread.*get_mfn)();
    bool new_value=old_value;
    ImGui::Checkbox(label,&new_value);

    if(new_value!=old_value) {
        (*m_beeb_thread.*send_mfn)(new_value);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HardReset() {
    m_beeb_thread->SendHardResetMessage(false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::LoadLastState() {
    m_beeb_thread->SendLoadLastStateMessage();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::IsLoadLastStateEnabled() const {
    return m_beeb_thread->GetLastSavedStateTimelineId()!=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SaveState() {
    m_beeb_thread->SendSaveStateMessage(true);
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

#if SYSTEM_WINDOWS
void BeebWindow::ClearConsole() {
    ::ClearConsole();
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
    Timeline::Dump(&LOG(OUTPUTND));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DumpTimelineDebuger() {
    Timeline::Dump(&LOG(OUTPUT));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::CheckTimeline() {
    Timeline::Check();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<BeebWindowUIFlag FLAG>
void BeebWindow::ToggleUICommand() {
    m_settings.ui_flags^=FLAG;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<BeebWindowUIFlag FLAG>
bool BeebWindow::IsUICommandTicked() const {
    return !!(m_settings.ui_flags&FLAG);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<BeebWindowUIFlag FLAG>
ObjectCommandTable<BeebWindow>::Initializer BeebWindow::GetToggleUICommand(std::string name,std::string text) {
    return ObjectCommandTable<BeebWindow>::Initializer(std::move(name),std::move(text),&BeebWindow::ToggleUICommand<FLAG>,&BeebWindow::IsUICommandTicked<FLAG>);
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

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

static const uint32_t UTF8_ACCEPT=0;
static const uint32_t UTF8_REJECT=1;

static const uint8_t utf8d[]={
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
    0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
    0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
    0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
    1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
    1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
    1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

uint32_t inline
decode(uint32_t* state,uint32_t* codep,uint32_t byte) {
    uint32_t type=utf8d[byte];

    *codep=(*state!=UTF8_ACCEPT)?
        (byte&0x3fu)|(*codep<<6):
        (0xffu>>type) & (byte);

    *state=utf8d[256+*state*16+type];
    return *state;
}

void BeebWindow::DoPaste(bool add_return) {
    if(m_beeb_thread->IsPasting()) {
        m_beeb_thread->SendStopPasteMessage();
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
            uint32_t state=UTF8_ACCEPT,codepoint;
            size_t char_start=0;
            for(size_t i=0;i<utf8.size();++i) {
                decode(&state,&codepoint,utf8[i]);
                if(state==UTF8_ACCEPT) {
                    // Do some useful translation.
                    if(codepoint==0xa3) {
                        // GBP symbol
                        codepoint=95;
                    }

                    if(codepoint==13) {
                    } else if(codepoint==10) {
                    } else if(codepoint>=32&&codepoint<=126) {
                    } else {
                        m_msg.e.f("Invalid character: ");

                        if(codepoint>=32) {
                            m_msg.e.f("'%.*s', ",(int)(i-char_start),&utf8[char_start]);
                        }

                        m_msg.e.f("%u (0x%X)\n",codepoint,codepoint);
                        return;
                    }

                    ascii.push_back((char)codepoint);
                    char_start=i+1;
                } else if(state==UTF8_REJECT) {
                    m_msg.e.f("Clipboard contents are not valid UTF-8 text\n");
                    return;
                }
            }
        }

        // Knobble newlines.
        if(ascii.size()>1) {
            std::string::size_type i=0;

            while(i<ascii.size()-1) {
                if(ascii[i]==10&&ascii[i+1]==13) {
                    ascii.erase(i,1);
                } else if(ascii[i]==13&&ascii[i+1]==10) {
                    ++i;
                    ascii.erase(i,1);
                } else if(ascii[i]==10) {
                    ascii[i++]=13;
                } else {
                    ++i;
                }
            }
        }

        if(add_return) {
            ascii.push_back(13);
        }

        m_beeb_thread->SendStartPasteMessage(std::move(ascii));
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
        m_beeb_thread->SendStopCopyMessage();
    } else {
        m_beeb_thread->SendStartCopyMessage([this](std::vector<uint8_t> data) {
            this->SetClipboardData(std::move(data),IS_TEXT);
        });
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
        m_beeb_thread->SendStopCopyMessage();
    } else {
        m_beeb_thread->SendStartCopyBASICMessage([this](std::vector<uint8_t> data) {
            this->SetClipboardData(std::move(data),true);
        });
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::IsCopyBASICEnabled() const {
    return !m_beeb_thread->IsPasting();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ObjectCommandTable<BeebWindow> BeebWindow::ms_command_table("Beeb Window",{
    {"hard_reset","Hard Reset",&BeebWindow::HardReset},
    {"load_last_state","Load Last State",&BeebWindow::LoadLastState,nullptr,&BeebWindow::IsLoadLastStateEnabled},
    {"save_state","Save State",&BeebWindow::SaveState},
    GetToggleUICommand<BeebWindowUIFlag_Options>("toggle_emulator_options","Emulator options..."),
    GetToggleUICommand<BeebWindowUIFlag_Keymaps>("toggle_keyboard_layout","Keyboard layout..."),
#if TIMELINE_UI_ENABLED
    GetToggleUICommand<BeebWindowUIFlag_Timeline>("toggle_timeline","Timeline..."),
#endif
    GetToggleUICommand<BeebWindowUIFlag_Messages>("toggle_messages","Messages..."),
    GetToggleUICommand<BeebWindowUIFlag_Configs>("toggle_configurations","Configurations..."),
    GetToggleUICommand<BeebWindowUIFlag_Trace>("toggle_event_trace","Event trace..."),
    GetToggleUICommand<BeebWindowUIFlag_AudioCallback>("toggle_date_rate","Data rate..."),
    GetToggleUICommand<BeebWindowUIFlag_CommandContextStack>("toggle_cc_stack","Command context stack..."),
    GetToggleUICommand<BeebWindowUIFlag_CommandKeymaps>("toggle_command_keymaps","Command shortcuts..."),
    {"exit","Exit",&BeebWindow::Exit,ConfirmCommand()},
    {"clean_up_recent_files_lists","Clean up recent files lists",&BeebWindow::CleanUpRecentFilesLists,ConfirmCommand()},
#if SYSTEM_WINDOWS
    {"clear_console","Clear Win32 console",&BeebWindow::ClearConsole},
#endif
    {"print_separator","Print stdout separator",&BeebWindow::PrintSeparator},
    {"dump_timeline_console","Dump timeline to console only",&BeebWindow::DumpTimelineConsole},
    {"dump_timeline_debugger","Dump timeline to console+debugger",&BeebWindow::DumpTimelineDebuger},
    {"check_timeline","Check timeline",&BeebWindow::CheckTimeline},
    {"paste","OSRDCH Paste",&BeebWindow::Paste,&BeebWindow::IsPasteTicked},
    {"paste_return","OSRDCH Paste (+Return)",&BeebWindow::PasteThenReturn,&BeebWindow::IsPasteTicked},
    {"toggle_copy_oswrch_text","OSWRCH Copy Text",&BeebWindow::CopyOSWRCH<true>,&BeebWindow::IsCopyOSWRCHTicked},
    //{"toggle_copy_oswrch_binary","OSWRCH Copy Binary",&BeebWindow::CopyOSWRCH<false>,&BeebWindow::IsCopyOSWRCHTicked},
    {"copy_basic","Copy BASIC listing",&BeebWindow::CopyBASIC,&BeebWindow::IsCopyOSWRCHTicked,&BeebWindow::IsCopyBASICEnabled},
});
