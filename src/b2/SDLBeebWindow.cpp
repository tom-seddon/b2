#include <shared/system.h>
#include "SDLBeebWindow.h"
#include "BeebWindow.h"
#include <SDL.h>
#include "VBlankMonitor.h"
#include "load_save.h"
#include <SDL_syswm.h>
#include <beeb/BBCMicro.h>
#include <beeb/sound.h>
#include "misc.h"
#include "dear_imgui.h"
#include "MessagesUI.h"
#include "BeebWindows.h"
#include "KeymapsUI.h"
#include "keys.h"
#include "commands.h"
#include <shared/path.h>
#include <beeb/DiscImage.h>
#include "MemoryDiscImage.h"
#include "DirectDiscImage.h"
#include "discs.h"
#include "SettingsUI.h"
#include "CommandKeymapsUI.h"
#include "DataRateUI.h"
#include "Remapper.h"
#include "filters.h"

#include <shared/enum_def.h>
#include "SDLBeebWindow.inl"
#include <shared/enum_end.h>

LOG_EXTERN(OUTPUT);

static const double MESSAGES_POPUP_TIME_SECONDS=2.5;
static const double LEDS_POPUP_TIME_SECONDS=1.;
static const double TITLE_UPDATE_TIME_SECONDS=.5;

static const std::string RECENT_PATHS_DISC_IMAGE="disc_image";
//static const std::string RECENT_PATHS_RAM="ram";
static const std::string RECENT_PATHS_NVRAM="nvram";

static const float VOLUMES_TABLE[]={
    0.00000f, 0.03981f, 0.05012f, 0.06310f,
    0.07943f, 0.10000f, 0.12589f, 0.15849f,
    0.19953f, 0.25119f, 0.31623f, 0.39811f,
    0.50119f, 0.63096f, 0.79433f, 1.00000f,
};

struct SDLBeebWindow::AudioThreadData {
    Remapper remapper;

    float bbc_sound_scale=1.f;
    float disc_sound_scale=1.f;

    size_t sound_buffer_size_samples=0;
    size_t num_consumed_sound_units=0;
    
    // over time, this should settle down to some size that means no more
    // reallocs.
    std::vector<SoundDataUnit> work_units;

    // When taking the SDL audio lock and locking units_mutex, do the SDL audio
    // lock first!
    Mutex units_mutex;
    std::vector<SoundDataUnit> units;
};

struct SDLBeebWindow::SettingsUIMetadata {
    BeebWindowPopupType type;
    const char *name;
    const char *command_name;
    std::unique_ptr<SettingsUI> (*create_fn)(SDLBeebWindow *beeb_window);
};

const SDLBeebWindow::SettingsUIMetadata SDLBeebWindow::ms_settings_uis[]={
    {BeebWindowPopupType_Keymaps,"Keyboard Layouts","toggle_keyboard_layout",&CreateKeymapsUI},
    {BeebWindowPopupType_CommandKeymaps,"Command Keys","toggle_command_keymaps",&CreateCommandKeymapsUI},
    {BeebWindowPopupType_Options,"Options","toggle_emulator_options",&SDLBeebWindow::CreateOptionsUI},
//    {BeebWindowPopupType_Messages,"Messages","toggle_messages",&CreateMessagesUI},
//    {BeebWindowPopupType_Timeline,"Timeline","toggle_timeline",&BeebWindow::CreateTimelineUI},
//    {BeebWindowPopupType_SavedStates,"Saved States","toggle_saved_states",&BeebWindow::CreateSavedStatesUI},
//    {BeebWindowPopupType_Configs,"Configs","toggle_configurations",&CreateConfigsUI},
//#if BBCMICRO_TRACE
//    {BeebWindowPopupType_Trace,"Tracing","toggle_event_trace",&CreateTraceUI},
//#endif
    // inconsistent naming as this window has had multiple rebrands.
    {BeebWindowPopupType_AudioCallback,"Performance","toggle_date_rate",&CreateDataRateUI},
//#if BBCMICRO_DEBUGGER&&VIDEO_TRACK_METADATA
//    // slightly inconsistent naming as this was created before the debugger...
//    {BeebWindowPopupType_PixelMetadata,"Pixel Metadata","toggle_pixel_metadata",&CreatePixelMetadataDebugWindow},
//#endif
//#if ENABLE_IMGUI_TEST
//    {BeebWindowPopupType_DearImguiTest,"dear imgui Test","toggle_dear_imgui_test",&CreateDearImguiTestUI},
//#endif
//#if BBCMICRO_DEBUGGER
//    {BeebWindowPopupType_6502Debugger,"6502 Debug","toggle_6502_debugger",&Create6502DebugWindow,},
//    {BeebWindowPopupType_MemoryDebugger1,"Memory Debug 1","toggle_memory_debugger1",&CreateMemoryDebugWindow,},
//    {BeebWindowPopupType_MemoryDebugger2,"Memory Debug 2","toggle_memory_debugger2",&CreateMemoryDebugWindow,},
//    {BeebWindowPopupType_MemoryDebugger3,"Memory Debug 3","toggle_memory_debugger3",&CreateMemoryDebugWindow,},
//    {BeebWindowPopupType_MemoryDebugger4,"Memory Debug 4","toggle_memory_debugger4",&CreateMemoryDebugWindow,},
//    {BeebWindowPopupType_ExtMemoryDebugger1,"External Memory Debug 1","toggle_ext_memory_debugger1",&CreateExtMemoryDebugWindow,},
//    {BeebWindowPopupType_ExtMemoryDebugger2,"External Memory Debug 2","toggle_ext_memory_debugger2",&CreateExtMemoryDebugWindow,},
//    {BeebWindowPopupType_ExtMemoryDebugger3,"External Memory Debug 3","toggle_ext_memory_debugger3",&CreateExtMemoryDebugWindow,},
//    {BeebWindowPopupType_ExtMemoryDebugger4,"External Memory Debug 4","toggle_ext_memory_debugger4",&CreateExtMemoryDebugWindow,},
//    {BeebWindowPopupType_DisassemblyDebugger1,"Disassembly Debug 1","toggle_disassembly_debugger1",&CreateDisassemblyDebugWindow1,},
//    {BeebWindowPopupType_DisassemblyDebugger2,"Disassembly Debug 2","toggle_disassembly_debugger2",&CreateDisassemblyDebugWindowN,},
//    {BeebWindowPopupType_DisassemblyDebugger3,"Disassembly Debug 3","toggle_disassembly_debugger3",&CreateDisassemblyDebugWindowN,},
//    {BeebWindowPopupType_DisassemblyDebugger4,"Disassembly Debug 4","toggle_disassembly_debugger4",&CreateDisassemblyDebugWindowN,},
//    {BeebWindowPopupType_CRTCDebugger,"CRTC Debug","toggle_crtc_debugger",&CreateCRTCDebugWindow,},
//    {BeebWindowPopupType_VideoULADebugger,"Video ULA Debug","toggle_video_ula_debugger",&CreateVideoULADebugWindow,},
//    {BeebWindowPopupType_SystemVIADebugger,"System VIA Debug","toggle_system_via_debugger",&CreateSystemVIADebugWindow,},
//    {BeebWindowPopupType_UserVIADebugger,"User VIA Debug","toggle_user_via_debugger",&CreateUserVIADebugWindow,},
//    {BeebWindowPopupType_NVRAMDebugger,"NVRAM Debug","toggle_nvram_debugger",&CreateNVRAMDebugWindow,},
//    {BeebWindowPopupType_SN76489Debugger,"SN76489 Debug","toggle_sn76489_debugger",&CreateSN76489DebugWindow,},
//    {BeebWindowPopupType_PagingDebugger,"Paging Debug","toggle_paging_debugger",&CreatePagingDebugWindow,},
//    {BeebWindowPopupType_BreakpointsDebugger,"Breakpoints","toggle_breakpoints_debugger",&CreateBreakpointsDebugWindow,},
//    {BeebWindowPopupType_StackDebugger,"Stack","toggle_stack_debugger",&CreateStackDebugWindow,},
//#endif
//    {BeebWindowPopupType_BeebLink,"BeebLink Options","toggle_beeblink_options",&CreateBeebLinkUI},

    // terminator
    {BeebWindowPopupType_MaxValue},
};

class SDLBeebWindow::OptionsUI:
public SettingsUI
{
public:
    explicit OptionsUI(SDLBeebWindow *beeb_window);
    
    void DoImGui() override;

    bool OnClose() override;
protected:
private:
    SDLBeebWindow *m_beeb_window=nullptr;
};

SDLBeebWindow::OptionsUI::OptionsUI(SDLBeebWindow *beeb_window):
m_beeb_window(beeb_window)
{
}

void SDLBeebWindow::OptionsUI::DoImGui() {
//    {
//        float speed_scale=beeb_thread->GetSpeedScale();
//        bool limit_speed=beeb_thread->IsSpeedLimited();
//
//        if(ImGui::Checkbox("Limit Speed",&limit_speed)) {
//            beeb_thread->Send(std::make_shared<BeebThread::SetSpeedLimitedMessage>(limit_speed));
//        }
//
//        if(limit_speed) {
//            ImGui::SameLine();
//
//            bool changed=false;
//
//            if(ImGui::Button("1x")) {
//                speed_scale=1.f;
//                changed=true;
//            }
//
//            if(ImGui::SliderFloat("Speed scale",&speed_scale,0.f,2.f)) {
//                changed=true;
//            }
//
//            if(changed) {
//                beeb_thread->Send(std::make_shared<BeebThread::SetSpeedScaleMessage>(speed_scale));
//            }
//        }
//    }

    ImGui::Checkbox("Correct aspect ratio",&m_beeb_window->m_settings.correct_aspect_ratio);

    ImGui::Checkbox("Auto scale",&m_beeb_window->m_settings.display_auto_scale);

    ImGui::DragFloat("Manual scale",&m_beeb_window->m_settings.display_manual_scale,.01f,0.f,10.f);

    if(ImGui::Checkbox("Filter display",&m_beeb_window->m_settings.display_filter)) {
        m_beeb_window->m_recreate_tv_texture=true;
    }

    ImGui::Checkbox("Emulate interlace",&m_beeb_window->m_settings.display_interlace);

    if(ImGui::SliderFloat("BBC volume",&m_beeb_window->m_settings.bbc_volume,MIN_DB,MAX_DB,"%.1f dB")) {
        //m_beeb_window->m_beeb->SetBBCVolume(settings->bbc_volume);
    }

    if(ImGui::SliderFloat("Disc volume",&m_beeb_window->m_settings.disc_volume,MIN_DB,MAX_DB,"%.1f dB")) {
        //beeb_thread->SetDiscVolume(settings->disc_volume);
    }

    if(ImGui::Checkbox("Power-on tone",&m_beeb_window->m_settings.power_on_tone)) {
        //beeb_thread->SetPowerOnTone(m_beeb_window->m_settings.power_on_tone);
    }

    ImGui::NewLine();

#if BBCMICRO_DEBUGGER
    if(ImGui::CollapsingHeader("Display Debug Flags",ImGuiTreeNodeFlags_DefaultOpen)) {
        
        bool teletext_debug=m_beeb_window->m_beeb->GetTeletextDebug();
        if(ImGui::Checkbox("Teletext debug",&teletext_debug)) {
            m_beeb_window->m_beeb->SetTeletextDebug(teletext_debug);
        }
        
        TVOutputSettings settings=m_beeb_window->m_tv.GetSettings();
        
        ImGui::Checkbox("Show TV beam position",&settings.show_beam_position);
//        if(ImGui::Checkbox("Test pattern",&m_beeb_window->m_test_pattern)) {
//            if(m_beeb_window->m_test_pattern) {
//                //m_beeb_window->m_tv.FillWithTestPattern();
//            }
//        }

        ImGui::Checkbox("1.0 usec",&settings.show_usec_markers);
        ImGui::SameLine();
        ImGui::Checkbox("0.5 usec",&settings.show_half_usec_markers);

        ImGui::Checkbox("6845 rows",&settings.show_6845_row_markers);
        ImGui::SameLine();
        ImGui::Checkbox("6845 DISPEN",&settings.show_6845_dispen_markers);

        m_beeb_window->m_tv.SetSettings(settings);
    }
#endif
}

bool SDLBeebWindow::OptionsUI::OnClose() {
    return true;
}

SDLBeebWindow::Command::~Command() {
}

CommandPrepareResult SDLBeebWindow::Command::Prepare(SDLBeebWindow *beeb_window) {
    return CommandPrepareResult_Execute;
}

//void SDLBeebWindow::Command::CallCompletionFun(CompletionFun *fun,bool success,const char *message) {
//    if(fun) {
//        (*fun)(success,message);
//    }
//}

//bool SDLBeebWindow::Command::PrepareUnlessReplayingOrHalted(SDLBeebWindow *beeb_window,CompletionFun *completion_fun) {
//    (void)beeb_window,(void)completion_fun;
//
//#if BBCMICRO_DEBUG
//    if(beeb_window->m_beeb) {
//        if(beeb_window->m_beeb->DebugIsHalted()) {
//            CallCompletionFun(completion_fun,false,"not valid while halted");
//            return false;
//        }
//    }
//#endif
//
//    return true;
//}

//bool SDLBeebWindow::Command::PrepareUnlessReplaying(SDLBeebWindow *beeb_window,CompletionFun *completion_fun) {
//    (void)beeb_window,(void)completion_fun;
//
//    return true;
//}

SDLBeebWindow::KeySymCommand::KeySymCommand(BeebKeySym key_sym,bool down):
m_down(down)
{
    if(!GetBeebKeyComboForKeySym(&m_beeb_key,&m_shift_state,key_sym)) {
        m_beeb_key=BeebKey_None;
    }
}

CommandPrepareResult SDLBeebWindow::KeySymCommand::Prepare(SDLBeebWindow *beeb_window) {
//    if(!this->PrepareUnlessReplayingOrHalted(beeb_window,completion_fun)) {
//        return false;
//    }
    
    if(m_beeb_key==BeebKey_None) {
        return CommandPrepareResult_Ignore;
    }
    
    if(beeb_window->m_beeb) {
        if(beeb_window->m_beeb->GetKeyState(m_beeb_key)==m_down&&
           beeb_window->m_fake_shift_state==m_shift_state)
        {
            // not an error - just don't duplicate events when the key is held.
            return CommandPrepareResult_Ignore;
        }
    }
    
    return CommandPrepareResult_Execute;
}

void SDLBeebWindow::KeySymCommand::Execute(SDLBeebWindow *beeb_window) const {
    beeb_window->SetKeyState(m_beeb_key,m_down);
    beeb_window->SetFakeShiftState(m_shift_state);
}

SDLBeebWindow::KeyCommand::KeyCommand(BeebKey beeb_key,bool down):
m_beeb_key(beeb_key),
m_down(down)
{
}

CommandPrepareResult SDLBeebWindow::KeyCommand::Prepare(SDLBeebWindow *beeb_window) {
//    if(!this->PrepareUnlessReplayingOrHalted(beeb_window,completion_fun)) {
//        return false;
//    }
    
    if(m_beeb_key==BeebKey_None) {
        return CommandPrepareResult_Ignore;
    }

    if(beeb_window->m_beeb) {
        // Don't use the Beeb key states for this, as they may not reflect the
        // actual key state. (e.g., Beeb never reports Break as held)
        if(beeb_window->m_real_key_states.GetKeyState(m_beeb_key)==m_down) {
            // not an error - just don't duplicate events when the key is held.
            return CommandPrepareResult_Ignore;
        }
    }
    
    return CommandPrepareResult_Execute;
}

void SDLBeebWindow::KeyCommand::Execute(SDLBeebWindow *beeb_window) const {
    beeb_window->SetKeyState(m_beeb_key,m_down);
}

SDLBeebWindow::LoadDiscCommand::LoadDiscCommand(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose):
m_drive(drive),
m_disc_image(std::move(disc_image)),
m_verbose(verbose)
{
}

void SDLBeebWindow::LoadDiscCommand::Execute(SDLBeebWindow *beeb_window) const {
    beeb_window->m_beeb->SetDiscImage(m_drive,m_disc_image);
}

SDLBeebWindow::EjectDiscCommand::EjectDiscCommand(int drive):
m_drive(drive)
{
}

void SDLBeebWindow::EjectDiscCommand::Execute(SDLBeebWindow *beeb_window) const {
    beeb_window->m_beeb->SetDiscImage(m_drive,nullptr);
}

SDLBeebWindow::SetDriveWriteProtectedCommand::SetDriveWriteProtectedCommand(int drive,bool write_protected):
m_drive(drive),
m_write_protected(write_protected)
{
}

void SDLBeebWindow::SetDriveWriteProtectedCommand::Execute(SDLBeebWindow *beeb_window) const {
    beeb_window->m_beeb->SetDriveWriteProtected(m_drive,m_write_protected);
}

SDLBeebWindow::HardResetAndReloadConfigCommand::HardResetAndReloadConfigCommand(uint32_t reset_flags):
m_reset_flags(reset_flags)
{
}

CommandPrepareResult SDLBeebWindow::HardResetAndReloadConfigCommand::Prepare(SDLBeebWindow *beeb_window) {
    BeebLoadedConfig reloaded_config;
    if(!BeebLoadedConfig::Load(&m_config,beeb_window->m_current_config.config,&beeb_window->m_msg)) {
        return CommandPrepareResult_Fail;
    }
    
    m_config.ReuseROMs(beeb_window->m_current_config);
    m_nvram_contents=beeb_window->m_beeb->GetNVRAM();

    return CommandPrepareResult_Execute;
}

void SDLBeebWindow::HardResetAndReloadConfigCommand::Execute(SDLBeebWindow *beeb_window) const {
    beeb_window->HardReset(m_reset_flags,m_config,m_nvram_contents);
}

SDLBeebWindow::HardResetAndChangeConfigCommand::HardResetAndChangeConfigCommand(uint32_t reset_flags,BeebLoadedConfig config):
m_reset_flags(reset_flags),
m_config(std::move(config)),
m_nvram_contents(GetDefaultNVRAMContents(m_config.config.type))
{
}

void SDLBeebWindow::HardResetAndChangeConfigCommand::Execute(SDLBeebWindow *beeb_window) const {
    beeb_window->HardReset(m_reset_flags,m_config,m_nvram_contents);
}

SDLBeebWindow::DriveState::DriveState():
    new_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE),
    open_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE),
    new_direct_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE),
    open_direct_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE)
{
    this->new_disc_image_file_dialog.AddFilter("BBC disc images",DISC_IMAGE_EXTENSIONS);

    {
        std::vector<std::string> extensions=DISC_IMAGE_EXTENSIONS;
        extensions.push_back(".zip");

        this->open_disc_image_file_dialog.AddFilter("BBC disc images",extensions);
        this->open_disc_image_file_dialog.AddAllFilesFilter();
    }

    this->new_direct_disc_image_file_dialog.AddFilter("BBC disc images",DISC_IMAGE_EXTENSIONS);

    this->open_direct_disc_image_file_dialog.AddFilter("BBC disc images",DISC_IMAGE_EXTENSIONS);
}

SDLBeebWindow::~SDLBeebWindow() {
    delete m_beeb;
    m_beeb=nullptr;
    
    delete m_imgui_stuff;
    m_imgui_stuff=nullptr;

    for(size_t i=0;i<sizeof m_sdl_cursors/sizeof m_sdl_cursors[0];++i) {
        SDL_FreeCursor(m_sdl_cursors[i]);
        m_sdl_cursors[i]=nullptr;
    }
    
    m_font_texture=nullptr;
    m_tv_texture=nullptr;
    m_renderer=nullptr;
    m_window=nullptr;
    m_pixel_format=nullptr;
}

bool SDLBeebWindow::Init(const BeebWindowInitArguments &init_arguments,
                         const BeebWindowSettings &settings,
                         std::shared_ptr<MessageList> message_list,
                         std::vector<uint8_t> window_placement_data,
                         uint32_t *sdl_window_id)
{
    m_init_arguments=init_arguments;
    m_settings=settings;

    m_message_list=message_list;
    m_msg=Messages(m_message_list);

    //m_beeb_window=new BeebWindow();

    if(!this->InitInternal(std::move(window_placement_data))) {
        return false;
    }

    m_audio_thread_data=new AudioThreadData;
    m_audio_thread_data->remapper=Remapper(m_init_arguments.sound_spec.freq,SOUND_CLOCK_HZ);
    m_audio_thread_data->sound_buffer_size_samples=m_init_arguments.sound_spec.samples;


    m_imgui_stuff=new ImGuiStuff();
    if(!m_imgui_stuff->Init(m_renderer.get(),&m_font_texture)) {
        m_msg.e.f("failed to initialize ImGui\n");
        return false;
    }

    if(!m_settings.dock_config.empty()) {
        if(!m_imgui_stuff->LoadDockContext(m_settings.dock_config)) {
            m_msg.w.f("failed to load dock config\n");
        }
    }

//    if(!m_beeb_window->Init(m_init_arguments,
//                            settings,
//                            m_message_list,
//                            std::move(imgui_stuff),
//                            m_pixel_format))
//    {
//        return false;
//    }

    *sdl_window_id=SDL_GetWindowID(m_window.get());

    return true;
}

//BeebWindow *SDLBeebWindow::GetBeebWindow() const {
//    return m_beeb_window;
//}

//void SDLBeebWindow::SaveSettings() {
//    m_beeb_window->SaveSettings();
//    this->SavePosition();
//}

bool SDLBeebWindow::MaybeStoreCommandKeycode(uint32_t keycode) {
    for(const CommandTable *table:m_current_command_tables) {
        if(table->FindCommandsForPCKey(keycode)) {
            m_command_keycodes.insert(keycode);
            return true;
        }
    }

    return false;
}

void SDLBeebWindow::HandleSDLKeyEvent(const SDL_KeyboardEvent &event) {
    uint32_t keycode;
    bool state;
    
    //LOGF(OUTPUT,"key event: down=%d scancode=%d\n",event.type==SDL_KEYDOWN,event.keysym.scancode);
    
    if(event.type==SDL_KEYDOWN) {
        if(event.repeat) {
            // Don't set again if it's just key repeat. If the flag is
            // still set from last time, that's fine; if it's been reset,
            // there'll be a reason, so don't set it again.
        } else {
            m_imgui_stuff->SetKeyDown(event.keysym.scancode,true);
        }
        
        keycode=(uint32_t)event.keysym.sym|GetPCKeyModifiersFromSDLKeymod(event.keysym.mod);
        state=true;
    } else {
        m_imgui_stuff->SetKeyDown(event.keysym.scancode,false);
        
        keycode=0;
        state=false;
    }
    
//    uint32_t keycode=0;
//    bool state=false;
//    if(event.type==SDL_KEYDOWN) {
//        if(ImGui::IsKeyDown(event.keysym.scancode)) {
//            keycode=(uint32_t)event.keysym.sym|GetPCKeyModifiersFromSDLKeymod(event.keysym.mod);
//            state=true;
//        }
//    }

    if(m_beeb_focus) {
        bool handled=false;

        if(m_prefer_shortcuts) {
            handled=MaybeStoreCommandKeycode(keycode);
        }

        if(!handled) {
            handled=this->HandleBeebKey(event.keysym,state);
        }

        if(!m_prefer_shortcuts) {
            if(!handled) {
                handled=MaybeStoreCommandKeycode(keycode);
            }
        }
    } else {
        MaybeStoreCommandKeycode(keycode);
    }
}

void SDLBeebWindow::SetSDLMouseWheelState(int x,int y) {
    m_mouse_wheel_delta.x=x;
    m_mouse_wheel_delta.y=y;
}

void SDLBeebWindow::HandleSDLTextInput(const char *text) {
    m_sdl_text_input.append(text);
}

void SDLBeebWindow::HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event) {
    //m_beeb_window->HandleSDLMouseMotionEvent(event);

    // is this actually necessary??
}

bool SDLBeebWindow::HandleVBlank(VBlankMonitor *vblank_monitor,uint32_t vblank_display_id,uint64_t ticks) {
    // There's an API for exactly this on Windows. But it's probably
    // better to have the same code on every platform. 99% of the time
    // (and possibly even more often than that...) this will get the
    // right display.
    int wx,wy;
    SDL_GetWindowPosition(m_window.get(),&wx,&wy);

    int ww,wh;
    SDL_GetWindowSize(m_window.get(),&ww,&wh);

    uint32_t wdisplay_id=vblank_monitor->GetDisplayIDForPoint(wx+ww/2,wy+wh/2);
    if(wdisplay_id!=vblank_display_id) {
        return false;
    }
    
    this->UpdateTitle(ticks);
    
    // Quick as possible, draw the last frame's stuff, so it'll be ready before the
    // vertical blank is done.
    //
    // The UI is one frame behind, which is annoying, but the Beeb TV texture will
    // be up to date...
    //
    // Revisit this. Perhaps dear imgui will draw everything quickly enough? Update
    // order could also be a toggle.
    this->RenderLastImGuiFrame();
    
    // It's now safe to delete any textures and whatnot.
    if(m_recreate_tv_texture) {
        this->RecreateTexture();
        m_recreate_tv_texture=false;
    }
    
    // Would be nice to have a better place for this.
    //
    // It's not currently part of the TVOutpuSettings, because those
    // aren't (yet?) persistent.
    m_tv.SetInterlace(m_settings.display_interlace);
    
    this->RunNextImGuiFrame(ticks);
    
    return true;
}

UpdateResult SDLBeebWindow::UpdateBeeb() {
    if(m_beeb) {
        static const size_t MAX_NUM_CYCLES=5000;
        // TODO - excessive size! Should be more like
        // (MAX_NUM_CYCLES+(1<<SOUND_CLOCK_SHIFT)-1)>>SOUND_CLOCK_SHIFT, or w/e.
        SoundDataUnit sus[MAX_NUM_CYCLES];
        size_t sus_idx=0;
        size_t i;
        VideoDataUnit vu;
        //SoundDataUnit su;
        
        for(i=0;i<MAX_NUM_CYCLES;++i) {
            if(m_beeb->DebugIsHalted()) {
                break;
            }
            
            if(m_beeb->Update(&vu,&sus[sus_idx])) {
                // TODO: process sound unit
                ++sus_idx;
            }
            
            m_tv.Update(&vu);
        }
        
        {
            std::lock_guard<Mutex> lock(m_audio_thread_data->units_mutex);
            m_audio_thread_data->units.insert(m_audio_thread_data->units.end(),sus,sus+sus_idx);
        }

        return UpdateResult_FlatOut;
    } else {
        return UpdateResult_SpeedLimited;
    }
}

const BeebKeymap *SDLBeebWindow::GetKeymap() const {
    return m_keymap;
}

void SDLBeebWindow::SetKeymap(const BeebKeymap *keymap) {
    m_keymap=keymap;
    if(m_keymap) {
        m_settings.keymap_name=m_keymap->GetName();
        m_prefer_shortcuts=m_keymap->GetPreferShortcuts();
    } else {
        m_settings.keymap_name.clear();
        m_prefer_shortcuts=false;
    }
}

bool SDLBeebWindow::GetBeebKeyState(BeebKey key) const {
    if(key<0) {
        return false;
    } else if(key==BeebKey_Break) {
        return false;
    } else {
        return m_beeb->GetKeyState(key);
    }
}

void SDLBeebWindow::RenderLastImGuiFrame() {
    m_tv.CopyTexturePixels(&m_tv_texture_pixels);
    
    SDL_UpdateTexture(m_tv_texture.get(),nullptr,m_tv_texture_pixels.data(),TV_TEXTURE_WIDTH*4);
    
    SDL_RenderClear(m_renderer.get());

    if(!m_last_imgui_draw_lists.empty()) {
        std::vector<ImGuiStuff::StoredDrawList> *stored_draw_lists=nullptr;
#if STORE_DRAWLISTS
        stored_draw_lists=&m_stored_draw_lists;
#endif
        
        ImGuiStuff::RenderSDL(m_renderer.get(),
                              m_last_imgui_draw_lists,
                              stored_draw_lists);
    }
    
    SDL_RenderPresent(m_renderer.get());
}

void SDLBeebWindow::RunNextImGuiFrame(uint64_t ticks) {
    ImGuiContextSetter setter(m_imgui_stuff);
    
    bool got_mouse_focus;
    SDL_Window *mouse_window=SDL_GetMouseFocus();
    if(mouse_window==m_window.get()) {
        got_mouse_focus=true;
    } else {
        got_mouse_focus=false;
    }
    
    SDL_Point mouse_pos;
    uint32_t mouse_buttons=SDL_GetMouseState(&mouse_pos.x,&mouse_pos.y);
    
    SDL_Point mouse_wheel_delta=m_mouse_wheel_delta;
    m_mouse_wheel_delta={};

    SDL_Keymod keymod=SDL_GetModState();
    
    int renderer_output_width;
    int renderer_output_height;
    SDL_GetRendererOutputSize(m_renderer.get(),
                              &renderer_output_width,
                              &renderer_output_height);
    
//    for(const SDL_KeyboardEvent &event:m_sdl_keyboard_events) {
//        switch(event.type) {
//            case SDL_KEYDOWN:
//                if(event.repeat) {
//                    // Don't set again if it's just key repeat. If the flag is
//                    // still set from last time, that's fine; if it's been reset,
//                    // there'll be a reason, so don't set it again.
//                } else {
//                    m_imgui_stuff->SetKeyDown(event.keysym.scancode,true);
//                }
//                break;
//
//            case SDL_KEYUP:
//                m_imgui_stuff->SetKeyDown(event.keysym.scancode,false);
//                break;
//        }
//    }
//    m_sdl_keyboard_events.clear();
    
    m_imgui_stuff->AddInputCharactersUTF8(m_sdl_text_input.c_str());
    m_sdl_text_input.clear();
    
    m_imgui_stuff->NewFrame(got_mouse_focus,
                            mouse_pos,
                            mouse_buttons,
                            mouse_wheel_delta,
                            keymod,
                            renderer_output_width,
                            renderer_output_height,
                            m_font_texture.get());
    
    // Command contexts to try, in order of preference.
    CommandContext ccs[]={
        {},//panel that has focus - if any
        CommandContext(this,&ms_command_table),//for this window
    };
    static const size_t num_ccs=sizeof ccs/sizeof ccs[0];

    m_beeb_focus=false;

    {
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
            ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);
            
            this->DoMenuUI();
        }
        
        {
            ImVec2 pos=ImGui::GetCursorPos();
            pos.x=0.f;
            ImVec2 size={(float)renderer_output_width-pos.x,(float)renderer_output_height-pos.y};
            
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
                         ImGuiWindowFlags_NoBringToFrontOnFocus)))
        {
            {
                ImGuiStyleColourPusher cpusher2;
                cpusher2.PushDefault(style_colour);
                
                ImGui::BeginDockspace();
                {
                    ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);
                    
                    ccs[0]=this->DoSettingsUI();
                    
                    m_beeb_focus=this->DoBeebDisplayUI();
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
                        m_imgui_stuff->DoStoredDrawListWindow(m_stored_draw_lists);
                    }
#endif
                    
                    if(m_imgui_debug) {
                        m_imgui_stuff->DoDebugWindow();
                    }
                    
                    if(m_imgui_metrics) {
                        ImGui::ShowMetricsWindow();
                    }
                    
                    this->DoPopupUI(ticks,renderer_output_width,renderer_output_height);
                }
            }
            ImGui::End();
        }
    }
    
    for(uint32_t keycode:m_command_keycodes) {
        for(size_t i=0;i<num_ccs;++i) {
            if(ccs[i].ExecuteCommandsForPCKey(keycode)) {
                break;
            }
        }
    }
    m_command_keycodes.clear();
    
    m_imgui_stuff->RenderImGui();
    
    m_last_imgui_draw_lists=m_imgui_stuff->CloneDrawLists();
    
    m_current_command_tables.clear();
    for(size_t i=0;i<num_ccs;++i) {
        if(const CommandTable *table=ccs[i].GetCommandTable()) {
            m_current_command_tables.push_back(table);
        }
    }
    
    ImGuiMouseCursor cursor=ImGui::GetMouseCursor();
    if(cursor>=0&&cursor<ImGuiMouseCursor_COUNT) {
        if(SDL_Cursor *sdl_cursor=m_sdl_cursors[cursor]) {
            SDL_SetCursor(sdl_cursor);
        }
    }
}

CommandContext SDLBeebWindow::DoSettingsUI() {
    CommandContext cc;

    for(int type=0;type<BeebWindowPopupType_MaxValue;++type) {
        uint64_t mask=(uint64_t)1<<type;
        bool opened=!!(m_settings.popups&mask);

        if(opened) {
            if(!m_popups[type]) {
                const SettingsUIMetadata *ui=ms_settings_uis;
                while(ui->type!=BeebWindowPopupType_MaxValue) {
                    if(ui->type==type) {
                        break;
                    }

                    ++ui;
                }

                if(ui->type==type) {
                    m_popups[type]=(*ui->create_fn)(this);
                    ASSERT(!!m_popups[type]);
                    m_popups[type]->SetName(ui->name);
                }

            }

            SettingsUI *popup=m_popups[type].get();

            ImGui::SetNextDock(ImGuiDockSlot_None);
            ImVec2 default_pos=ImVec2(10.f,30.f);
            ImVec2 default_size=ImGui::GetIO().DisplaySize*.4f;

            if(popup) {
                if(ImGui::BeginDock(popup->GetName().c_str(),
                                    &opened,
                                    (ImGuiWindowFlags)popup->GetExtraImGuiWindowFlags(),
                                    default_size,
                                    default_pos))
                {
                    m_settings.popups|=mask;

                    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                        if(const CommandTable *table=popup->GetCommandTable()) {
                            cc=CommandContext(popup,table);
                        }
                    }

                    popup->DoImGui();
                }
                ImGui::EndDock();

                if(!opened) {
                    m_settings.popups&=~mask;

                    // Leave the deletion until the next frame -
                    // references to its textures might still be queued up
                    // in the dear imgui drawlists.
                }
            }
        } else {
            if(m_popups[type]) {
                if(m_popups[type]->OnClose()) {
                    this->SaveConfig();
                }

                m_popups[type]=nullptr;
            }
        }
    }

    if(ValueChanged(&m_msg_last_num_errors_and_warnings_printed,m_message_list->GetNumErrorsAndWarningsPrinted())) {
        m_settings.popups|=1<<BeebWindowPopupType_Messages;
    }

    return cc;
}


void SDLBeebWindow::ThreadFillAudioBuffer(SDL_AudioDeviceID audio_device_id,float *mix_buffer,size_t num_samples) {
    AudioThreadData *const atd=m_audio_thread_data;
    if(!atd) {
        return;
    }

    ASSERT(num_samples==atd->sound_buffer_size_samples);

    uint64_t num_units_needed_now=atd->remapper.GetNumUnits(num_samples);
    uint64_t num_units_needed_future=atd->remapper.GetNumUnits(num_samples*5/2);

    bool limit_speed=false;

    uint64_t max_num_sound_units;
    if(limit_speed) {
        max_num_sound_units=atd->num_consumed_sound_units+num_units_needed_future;
    } else {
        max_num_sound_units=UINT64_MAX;
    }

    Remapper *remapper,temp_remapper;
    {
        std::lock_guard<Mutex> lock(atd->units_mutex);

        if(atd->units.empty()) {
            return;
        }

        if(limit_speed) {
            if(num_units_needed_now<=atd->units.size()) {
                remapper=&atd->remapper;

                // TODO - it is in principle possible to determine statically
                // that SoundDataUnit is memcpy'able! But does that happen?
                size_t num_units_left=atd->units.size()-num_units_needed_now;
                atd->work_units.resize(num_units_needed_now);
                memcpy(atd->work_units.data(),atd->units.data(),num_units_needed_now*sizeof(SoundDataUnit));
                memmove(atd->units.data(),atd->units.data()+num_units_needed_now,num_units_left*sizeof(SoundDataUnit));
                atd->units.resize(num_units_left);
            } else {
                //if(perfect) {
                //    return;
                //}
                goto use_units_available;
            }
        } else {
        use_units_available:;
            // Underflow, or no speed limiting... just eat it all, however
            // much there is.
            temp_remapper=Remapper(num_samples,atd->units.size());
            remapper=&temp_remapper;

            atd->work_units.swap(atd->units);
            atd->units.clear();
        }
    }

    float *dest=mix_buffer;
    float sn_scale=1/4.f*atd->bbc_sound_scale;
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    float disc_sound_scale=1.f*atd->disc_sound_scale;
#endif

    float acc=0.f;
    const SoundDataUnit *unit=atd->work_units.data();

    for(size_t sample_idx=0;sample_idx<num_samples;++sample_idx) {
        uint64_t num_units_=remapper->Step();
        ASSERT(num_units_<=SIZE_MAX);
        size_t num_units=(size_t)num_units_;

        if(num_units>0) {
            acc=0.f;

            const float *filter;
            size_t filter_width;
            GetFilterForWidth(&filter,&filter_width,(size_t)num_units);
            ASSERT(filter_width<=num_units);

            for(size_t i=0;i<filter_width;++i) {
                acc+=(sn_scale*(VOLUMES_TABLE[unit->sn_output.ch[0]]+
                                VOLUMES_TABLE[unit->sn_output.ch[1]]+
                                VOLUMES_TABLE[unit->sn_output.ch[2]]+
                                VOLUMES_TABLE[unit->sn_output.ch[3]])+
                     disc_sound_scale*unit->disc_drive_sound)**filter++;
            }

            unit+=num_units-filter_width;
            atd->num_consumed_sound_units+=num_units;
        }

        dest[sample_idx]=acc;
    }
}

void SDLBeebWindow::UpdateWindowPlacement() {
#if SYSTEM_WINDOWS

    if(m_hwnd) {
        m_window_placement_data.resize(sizeof(WINDOWPLACEMENT));

        auto wp=(WINDOWPLACEMENT *)m_window_placement_data.data();
        memset(wp,0,sizeof *wp);
        wp->length=sizeof *wp;

        if(!GetWindowPlacement((HWND)m_hwnd,wp)) {
            m_window_placement_data.clear();
        }
    }

#elif SYSTEM_OSX

    SaveCocoaFrameUsingName(m_nswindow,COCOA_FRAME_NAME);

#else

    WindowPlacementData *wp;
    if(m_window_placement_data.size()!=sizeof(WindowPlacementData)) {
        m_window_placement_data.clear();
        m_window_placement_data.resize(sizeof(WindowPlacementData));

        wp=(WindowPlacementData *)m_window_placement_data.data();

        wp->maximized=0;
        wp->x=INT_MIN;
        wp->y=INT_MIN;
        wp->width=0;
        wp->height=0;
    } else {
        wp=(WindowPlacementData *)m_window_placement_data.data();
    }

    uint32_t flags=SDL_GetWindowFlags(m_window);

    wp->maximized=!!(flags&SDL_WINDOW_MAXIMIZED);

    if(flags&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN)) {
        // Don't update the size in this case.
    } else {
        SDL_GetWindowPosition(m_window,&wp->x,&wp->y);
        SDL_GetWindowSize(m_window,&wp->width,&wp->height);
    }

#endif
}

//const std::vector<uint8_t> &SDLBeebWindow::GetWindowPlacementData() const {
//    return m_window_placement_data;
//}

std::shared_ptr<MessageList> SDLBeebWindow::GetMessageList() const {
    return m_message_list;
}

bool SDLBeebWindow::InitInternal(std::vector<uint8_t> window_placement_data) {
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
        m_window.reset(SDL_CreateWindow("",
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        TV_TEXTURE_WIDTH+(int)(IMGUI_DEFAULT_STYLE.WindowPadding.x*2.f),
                                        19+TV_TEXTURE_HEIGHT+(int)(IMGUI_DEFAULT_STYLE.WindowPadding.y*2.f),
                                        SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL));
    if(!m_window) {
        m_msg.e.f("SDL_CreateWindow failed: %s\n",SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");
    m_renderer.reset(SDL_CreateRenderer(m_window.get(),-1,0));
    if(!m_renderer) {
        m_msg.e.f("SDL_CreateRenderer failed: %s\n",SDL_GetError());
        return false;
    }

    if(SDL_GL_GetCurrentContext()) {
        if(SDL_GL_SetSwapInterval(0)!=0) {
            m_msg.i.f("failed to set GL swap interval to 0: %s\n",SDL_GetError());
        }
    }

    // The OpenGL driver supports SDL_PIXELFORMAT_ARGB8888.
    m_pixel_format.reset(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888));
    if(!m_pixel_format) {
        m_msg.e.f("SDL_AllocFormat failed: %s\n",SDL_GetError());
        return false;
    }
    
    m_tv.Init(m_pixel_format->Rshift,
              m_pixel_format->Gshift,
              m_pixel_format->Bshift);

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    SDL_GetWindowWMInfo(m_window.get(),&wmi);

#if SYSTEM_WINDOWS

    m_hwnd=wmi.info.win.window;

    if(!m_init_arguments.reset_windows) {
        if(m_hwnd) {
            if(window_placement_data.size()==sizeof(WINDOWPLACEMENT)) {
                m_window_placement_data=std::move(window_placement_data);
                auto wp=(const WINDOWPLACEMENT *)m_window_placement_data.data();

                if(!SetWindowPlacement((HWND)m_hwnd,wp)) {
                    m_window_placement_data.clear();
                }
            }
        }
    }

#elif SYSTEM_OSX

    (void)window_placement_data;

    m_nswindow=wmi.info.cocoa.window;

    if(!m_init_arguments.reset_windows) {
        SetCocoaFrameUsingName(m_nswindow,COCOA_FRAME_NAME);
    }

#else

    if(!m_init_arguments.reset_windows) {
        if(window_placement_data.size()==sizeof(WindowPlacementData)) {
            m_window_placement_data=std::move(window_placement_data);
            auto wp=(const WindowPlacementData *)m_window_placement_data.data();

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
    if(SDL_GetRendererInfo(m_renderer.get(),&info)<0) {
        m_msg.e.f("SDL_GetRendererInfo failed: %s\n",SDL_GetError());
        return false;
    }

    if(!this->RecreateTexture()) {
        return false;
    }

    m_sdl_cursors[ImGuiMouseCursor_Arrow]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    m_sdl_cursors[ImGuiMouseCursor_ResizeAll]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    m_sdl_cursors[ImGuiMouseCursor_ResizeEW]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    m_sdl_cursors[ImGuiMouseCursor_ResizeNS]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    m_sdl_cursors[ImGuiMouseCursor_ResizeNESW]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    m_sdl_cursors[ImGuiMouseCursor_ResizeNWSE]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    m_sdl_cursors[ImGuiMouseCursor_TextInput]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

    {
        Uint32 format;
        int width,height;
        SDL_QueryTexture(m_tv_texture.get(),&format,nullptr,&width,&height);
        m_msg.i.f("Renderer: %s, %dx%d %s\n",
                  info.name,
                  width,
                  height,
                  SDL_GetPixelFormatName(format));
    }
    
    m_tv_texture_pixels.resize(TV_TEXTURE_WIDTH*TV_TEXTURE_HEIGHT);
    
    if(!LoadBeebConfigByName(&m_current_config,m_settings.config_name,&m_msg)) {
        if(!BeebLoadedConfig::Load(&m_current_config,*GetDefaultBeebConfigByIndex(0),&m_msg)) {
            m_msg.e.f("Failed to create Beeb config.\n");
            return false;
        }
    }
    
    std::vector<uint8_t> nvram=GetDefaultNVRAMContents(m_current_config.config.type);
    this->HardReset(0,
                    m_current_config,
                    std::move(nvram));
    
    const BeebKeymap *keymap=nullptr;
    if(!m_settings.keymap_name.empty()) {
        keymap=FindBeebKeymapByName(m_settings.keymap_name);
    }

    if(!keymap) {
        if(GetNumBeebKeymaps()>0) {
            keymap=GetBeebKeymapByIndex(0);
        } else {
            m_msg.e.f("No keymaps - please configure one using Keyboard > Keyboard Layouts...\n");
        }
    }
    
    this->SetKeymap(keymap);
    
//    m_msg.i.f("Sound: %s, %dHz %d-channel (%d byte buffer)\n",
//              SDL_GetCurrentAudioDriver(),
//              m_init_arguments.sound_spec.freq,
//              m_init_arguments.sound_spec.channels,
//              m_init_arguments.sound_spec.size);

    return true;
}

bool SDLBeebWindow::RecreateTexture() {
    SetRenderScaleQualityHint(m_settings.display_filter);

    m_tv_texture.reset(SDL_CreateTexture(m_renderer.get(),m_pixel_format->format,SDL_TEXTUREACCESS_STREAMING,TV_TEXTURE_WIDTH,TV_TEXTURE_HEIGHT));
    if(!m_tv_texture) {
        m_msg.e.f("Failed to create TV texture: %s\n",SDL_GetError());
        return false;
    }

    return true;
}

void SDLBeebWindow::HardReset(uint32_t reset_flags,
                              const BeebLoadedConfig &loaded_config,
                              const std::vector<uint8_t> &nvram_contents)
{
    uint32_t replace_flags=ReplaceFlag_KeepCurrentDiscs;

    if(reset_flags&ResetFlag_Boot) {
        replace_flags|=ReplaceFlag_Autoboot;
    }

//    if(ts->timeline_mode!=BeebThreadTimelineMode_Replay) {
//        replace_flags|=BeebThreadReplaceFlag_ResetKeyState;
//    }

    m_current_config=loaded_config;

    tm now=GetLocalTimeNow();

    uint64_t num_2MHz_cycles=0;
    if(m_beeb) {
        num_2MHz_cycles=*m_beeb->GetNum2MHzCycles();
    }
    
//    if(ts->current_config.config.beeblink) {
//        if(!ts->beeblink_handler) {
//            std::string sender_id=strprintf("%" PRIu64,beeb_thread->m_uid);
//            ts->beeblink_handler=std::make_unique<BeebLinkHTTPHandler>(beeb_thread,
//                                                                       std::move(sender_id),
//                                                                       beeb_thread->m_message_list);
//
//            if(!ts->beeblink_handler->Init(&ts->msgs)) {
//                // Ugh. Just give up.
//                ts->beeblink_handler.reset();
//            }
//        }
//    } else {
//        ts->beeblink_handler.reset();
//    }

//    BBCMicro(const BBCMicroType *type,
//             const DiscInterfaceDef *def,
//             const std::vector<uint8_t> &nvram_contents,
//             const tm *rtc_time,
//             bool video_nula,
//             bool ext_mem,
//             bool power_on_tone,
//             BeebLinkHandler *beeblink_handler,
//             uint64_t initial_num_2MHz_cycles);

    delete m_beeb;
    m_beeb=nullptr;
    
    m_beeb=new BBCMicro(m_current_config.config.type,
                        m_current_config.config.disc_interface,
                        nvram_contents,
                        &now,
                        m_current_config.config.video_nula,
                        m_current_config.config.ext_mem,
                        true,
                        nullptr,
                        num_2MHz_cycles);
    
    m_beeb->SetOSROM(m_current_config.os);

    for(uint8_t i=0;i<16;++i) {
        if(m_current_config.config.roms[i].writeable) {
            if(!!m_current_config.roms[i]) {
                m_beeb->SetSidewaysRAM(i,m_current_config.roms[i]);
            } else {
                m_beeb->SetSidewaysRAM(i,nullptr);
            }
        } else {
            if(!!m_current_config.roms[i]) {
                m_beeb->SetSidewaysROM(i,m_current_config.roms[i]);
            } else {
                m_beeb->SetSidewaysROM(i,nullptr);
            }
        }
    }
    
//#if BBCMICRO_DEBUGGER
//    if(m_flags&BeebThreadHardResetFlag_Run) {
//        ts->beeb->DebugRun();
//    }
//#endif
}

bool SDLBeebWindow::DoBeebDisplayUI() {
    //bool opened=m_imgui_stuff->AreAnyDocksDocked();
    bool focus=false;
    
    double scale_x;
    if(m_settings.correct_aspect_ratio) {
        scale_x=1.2/1.25;
    } else {
        scale_x=1;
    }
    
    ImGuiWindowFlags flags=0;//ImGuiWindowFlags_NoTitleBar;
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
        
        if(ImGui::IsWindowAppearing()) {
            ImGui::FocusWindow(GImGui->CurrentWindow);
        }
        
        ImVec2 padding=GImGui->Style.WindowPadding;
        
        focus=ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        
        ImGuiStyleVarPusher vpusher(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));
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
        ImGui::Image(m_tv_texture.get(),size);
        
#if VIDEO_TRACK_METADATA
        
        //m_got_mouse_pixel_unit=false;
        
        //            if(ImGui::IsItemHovered()) {
        //                ImVec2 mouse_pos=ImGui::GetMousePos();
        //                mouse_pos-=screen_pos;
        //
        //                double tx=mouse_pos.x/size.x;
        //                double ty=mouse_pos.y/size.y;
        //
        //                if(tx>=0.&&tx<1.&&ty>=0.&&ty<1.) {
        //                    int x=(int)(tx*TV_TEXTURE_WIDTH);
        //                    int y=(int)(ty*TV_TEXTURE_HEIGHT);
        //
        //                    ASSERT(x>=0&&x<TV_TEXTURE_WIDTH);
        //                    ASSERT(y>=0&&y<TV_TEXTURE_HEIGHT);
        //
        //                    const VideoDataUnit *units=m_tv.GetTextureUnits();
        //                    m_mouse_pixel_unit=units[y*TV_TEXTURE_WIDTH+x];
        //                    m_got_mouse_pixel_unit=true;
        //                }
        //            }
#else
        (void)screen_pos;
#endif
    }
    ImGui::EndDock();
    
    return focus;
}

void SDLBeebWindow::DoMenuUI() {
    if(ImGui::BeginMainMenuBar()) {
        this->DoFileMenu();
        this->DoEditMenu();
        this->DoHardwareMenu();
        this->DoKeyboardMenu();
        this->DoToolsMenu();
        this->DoDebugMenu();
        ImGui::EndMainMenuBar();
    }
}

void SDLBeebWindow::DoFileMenu() {
    if(ImGui::BeginMenu("File")) {
        m_cc.DoMenuItemUI("hard_reset");

        if(ImGui::BeginMenu("Run")) {
            this->DoDiscImageSubMenu(0,true);

            ImGui::EndMenu();
        }

        ImGui::Separator();

        for(int drive=0;drive<NUM_DRIVES;++drive) {
            char title[100];
            snprintf(title,sizeof title,"Drive %d",drive);

//            std::unique_lock<Mutex> d_lock;
//            std::shared_ptr<const DiscImage> disc_image=m_beeb_thread->GetDiscImage(&d_lock,drive);
            
            std::shared_ptr<const DiscImage> disc_image;
            if(m_beeb) {
                disc_image=m_beeb->GetDiscImage(drive);
            }

            if(ImGui::BeginMenu(title)) {
                this->DoDiscDriveSubMenu(drive,disc_image);

                ImGui::EndMenu();
            }

            if(!!disc_image) {
                std::string name=disc_image->GetName();
                name=PathGetName(name);
                ImGui::MenuItem(name.c_str(),nullptr,false,false);
            }

            ImGui::Separator();
        }

        m_cc.DoMenuItemUI("save_default_nvram");

        ImGui::Separator();

        //m_cc.DoMenuItemUI("load_last_state");
        m_cc.DoMenuItemUI("save_state");
        ImGui::Separator();
        m_cc.DoMenuItemUI("save_config");
        ImGui::Separator();
        m_cc.DoMenuItemUI("exit");
        ImGui::EndMenu();
    }
}

void SDLBeebWindow::DoDiscDriveSubMenu(int drive,
                                       const std::shared_ptr<const DiscImage> &disc_image)
{
    if(!!disc_image) {
        std::string name=disc_image->GetName();
        if(!name.empty()) {
            if(ImGui::BeginMenu("Full path")) {
                ImGui::MenuItem(name.c_str(),nullptr,false,false);

                if(ImGui::MenuItem("Copy path to clipboard")) {
                    SDL_SetClipboardText(name.c_str());
                }

                ImGui::EndMenu();
            }
        }

        std::string desc=disc_image->GetDescription();
        if(!desc.empty()) {
            ImGui::MenuItem(("Info: "+desc).c_str(),nullptr,false,false);
        }

        std::string hash=disc_image->GetHash();
        if(!hash.empty()) {
            ImGui::MenuItem(("SHA1: "+hash).c_str(),nullptr,false,false);
        }

        std::string load_method=disc_image->GetLoadMethod();
        ImGui::MenuItem(("Loaded from: "+load_method).c_str(),nullptr,false,false);

        bool disc_protected=disc_image->IsWriteProtected();

        if(disc_protected) {
            // Write protection state is shown, but can't be changed.
            ImGui::MenuItem("Write protect",nullptr,&disc_protected,false);
        } else {
            bool drive_protected=m_beeb->IsDriveWriteProtected(drive);
            if(ImGui::MenuItem("Write protect",nullptr,&drive_protected)) {
                this->Execute(std::make_unique<SetDriveWriteProtectedCommand>(drive,drive_protected));
                //this->RecordSetDriveWriteProtected(drive,drive_protected);
                //m_beeb_thread->Send(std::make_shared<BeebThread::SetDriveWriteProtectedMessage>(drive,drive_protected));
            }
        }

        if(ImGui::BeginMenu("Eject")) {
            if(ImGui::MenuItem("Confirm")) {
                this->Execute(std::make_unique<EjectDiscCommand>(drive));
                //this->RecordEjectDisc(drive);
                //m_beeb_thread->Send(std::make_shared<BeebThread::EjectDiscMessage>(drive));
            }
            ImGui::EndMenu();
        }
    } else {
        ImGui::MenuItem("(empty)",NULL,false,false);
    }

    ImGui::Separator();

    this->DoDiscImageSubMenu(drive,false);

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
    }
}

class SDLBeebWindow::FileMenuItem {
public:
    // set if disk image should be loaded.
    bool load=false;

    // if non-empty, an item was selected.
    std::string path;

    // details of the disk type, if the new disk option was chosen.
    const Disc *new_disc_type=nullptr;
    std::vector<uint8_t> new_disc_data;

    explicit FileMenuItem(SelectorDialog *new_dialog,
                          SelectorDialog *open_dialog,
                          const char *new_title,
                          const char *open_title,
                          const char *recent_title,
                          Messages *msgs)
    {
        //bool recent_enabled=true;

        ImGuiIDPusher id_pusher(open_title);

        if(ImGui::MenuItem(open_title)) {
            if(open_dialog->Open(&this->path)) {
                m_used_dialog=open_dialog;
                this->load=true;
            }
        }

        if(ImGui::BeginMenu(new_title)) {
            this->DoBlankDiscsMenu(new_dialog,
                                   BLANK_DFS_DISCS,
                                   NUM_BLANK_DFS_DISCS,
                                   msgs);
            ImGui::Separator();

            this->DoBlankDiscsMenu(new_dialog,
                                   BLANK_ADFS_DISCS,
                                   NUM_BLANK_ADFS_DISCS,
                                   msgs);

            ImGui::EndMenu();
        }

        if(ImGuiRecentMenu(&this->path,recent_title,*open_dialog)) {
            this->load=true;
        }
    }

    void Success() {
        if(m_used_dialog) {
            m_used_dialog->AddLastPathToRecentPaths();
        }
    }
protected:
private:
    SelectorDialog *m_used_dialog=nullptr;

    void DoBlankDiscsMenu(SelectorDialog *dialog,
                          const Disc *discs,
                          size_t num_discs,
                          Messages *msgs)
    {
        for(size_t i=0;i<num_discs;++i) {
            const Disc *disc=&discs[i];

            if(ImGui::MenuItem(disc->name.c_str())) {
                std::string src_path=disc->GetAssetPath();

                if(!LoadFile(&this->new_disc_data,src_path,msgs,0)) {
                    return;
                }

                if(dialog->Open(&this->path)) {
                    this->new_disc_type=disc;
                    m_used_dialog=dialog;
                    this->load=true;
                }
            }
        }
    }
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

void SDLBeebWindow::DoDiscImageSubMenu(int drive,bool boot) {
    ASSERT(drive>=0&&drive<NUM_DRIVES);
    DriveState *d=&m_drives[drive];
    
    FileMenuItem file_item(&d->new_disc_image_file_dialog,
                           &d->open_disc_image_file_dialog,
                           "New disc image",
                           "Disc image...",
                           "Recent disc image",
                           &m_msg);
    if(file_item.load) {
        std::shared_ptr<MemoryDiscImage> new_disc_image;
        if(file_item.new_disc_type) {
            new_disc_image=MemoryDiscImage::LoadFromBuffer(file_item.path,
                                                           MemoryDiscImage::LOAD_METHOD_FILE,
                                                           file_item.new_disc_data.data(),
                                                           file_item.new_disc_data.size(),
                                                           *file_item.new_disc_type->geometry,
                                                           &m_msg);
        } else {
            new_disc_image=MemoryDiscImage::LoadFromFile(file_item.path,
                                                         &m_msg);
        }
        this->DoDiscImageSubMenuItem(drive,
                                     std::move(new_disc_image),
                                     &file_item,boot);
    }
    
    FileMenuItem direct_item(&d->new_direct_disc_image_file_dialog,
                             &d->open_direct_disc_image_file_dialog,
                             "New direct disc image",
                             "Direct disc image...",
                             "Recent direct disc image",
                             &m_msg);
    if(direct_item.load) {
        if(direct_item.new_disc_type) {
            if(!SaveFile(direct_item.new_disc_data,
                         direct_item.path,
                         &m_msg))
            {
                return;
            }
        }
        
        std::shared_ptr<DirectDiscImage> new_disc_image=DirectDiscImage::CreateForFile(direct_item.path,&m_msg);
        
        this->DoDiscImageSubMenuItem(drive,
                                     std::move(new_disc_image),
                                     &direct_item,boot);
    }
}

void SDLBeebWindow::DoDiscImageSubMenuItem(int drive,
                                           std::shared_ptr<DiscImage> disc_image,
                                           FileMenuItem *item,
                                           bool boot)
{
    if(!!disc_image) {
        this->Execute(std::make_unique<LoadDiscCommand>(drive,std::move(disc_image),true));
//        m_beeb_thread->Send(std::make_shared<BeebThread::LoadDiscMessage>(drive,
//                                                                          std::move(disc_image),
//                                                                          true));
        if(boot) {
            this->Execute(std::make_unique<HardResetAndReloadConfigCommand>(ResetFlag_Boot|ResetFlag_Run));
            //this->RecordHardResetAndReloadConfig(ResetFlag_Boot|ResetFlag_Run);
//            m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Boot|
//                                                                                              BeebThreadHardResetFlag_Run));
        }
        
        item->Success();
    }
}

void SDLBeebWindow::DoEditMenu() {
}

void SDLBeebWindow::DoHardwareMenu() {
    if(ImGui::BeginMenu("Hardware")) {
        m_cc.DoMenuItemUI("toggle_configurations");

        if(GetNumBeebConfigs()>0) {
            ImGui::Separator();

            //std::string config_name=this->GetConfigName();

            for(size_t config_idx=0;config_idx<GetNumBeebConfigs();++config_idx) {
                BeebConfig *config=GetBeebConfigByIndex(config_idx);

                if(ImGui::MenuItem(config->name.c_str(),nullptr,config->name==m_current_config.config.name)) {
                    BeebLoadedConfig tmp;
                    if(BeebLoadedConfig::Load(&tmp,*config,&m_msg)) {
                        this->Execute(std::make_unique<HardResetAndChangeConfigCommand>(0,std::move(tmp)));
                    }
                }
            }
        }

        ImGui::EndMenu();
    }
}

void SDLBeebWindow::DoKeyboardMenu() {
    if(ImGui::BeginMenu("Keyboard")) {
        m_cc.DoMenuItemUI("toggle_keyboard_layout");
        m_cc.DoMenuItemUI("toggle_command_keymaps");

        ImGui::Separator();

//        m_cc.DoMenuItemUI("toggle_prioritize_shortcuts");

        if(GetNumBeebKeymaps()>0) {
            ImGui::Separator();

            for(size_t i=0;i<GetNumBeebKeymaps();++i) {
                BeebKeymap *keymap=GetBeebKeymapByIndex(i);

                if(ImGui::MenuItem(GetKeymapUIName(*keymap).c_str(),
                                   nullptr,
                                   m_keymap==keymap))
                {
                    this->SetKeymap(keymap);
                    m_msg.i.f("Keymap: %s\n",m_keymap->GetName().c_str());
                    this->ShowPrioritizeCommandShortcutsStatus();
                }
            }
        }

        ImGui::EndMenu();
    }
}

void SDLBeebWindow::DoToolsMenu() {
    if(ImGui::BeginMenu("Tools")) {
        m_cc.DoMenuItemUI("toggle_emulator_options");
        m_cc.DoMenuItemUI("toggle_messages");
        m_cc.DoMenuItemUI("toggle_timeline");
        m_cc.DoMenuItemUI("toggle_saved_states");
        m_cc.DoMenuItemUI("toggle_beeblink_options");

        ImGui::Separator();

        if(ImGui::BeginMenu("LEDs")) {
            ImGuiMenuItemEnumValue("Auto hide",nullptr,&m_settings.leds_popup_mode,BeebWindowLEDsPopupMode_Auto);
            ImGuiMenuItemEnumValue("Always on",nullptr,&m_settings.leds_popup_mode,BeebWindowLEDsPopupMode_On);
            ImGuiMenuItemEnumValue("Always off",nullptr,&m_settings.leds_popup_mode,BeebWindowLEDsPopupMode_Off);
            
            ImGui::EndMenu();
        }

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

        // Is there somewhere better for this?
        m_cc.DoMenuItemUI("reset_default_nvram");

        ImGui::Separator();
        m_cc.DoMenuItemUI("clean_up_recent_files_lists");
        m_cc.DoMenuItemUI("reset_dock_windows");

        ImGui::EndMenu();
    }
}

void SDLBeebWindow::DoDebugMenu() {
#if ENABLE_DEBUG_MENU
    if(ImGui::BeginMenu("Debug")) {
//#if ENABLE_IMGUI_TEST
//        m_cc.DoMenuItemUI("toggle_dear_imgui_test");
//#endif
//        m_cc.DoMenuItemUI("toggle_event_trace");
        m_cc.DoMenuItemUI("toggle_date_rate");

//#if SYSTEM_WINDOWS
//        if(GetConsoleWindow()) {
//            m_cc.DoMenuItemUI("clear_console");
//            m_cc.DoMenuItemUI("print_separator");
//        }
//#endif

//#if VIDEO_TRACK_METADATA
//        m_cc.DoMenuItemUI("toggle_pixel_metadata");
//#endif

//#if BBCMICRO_DEBUGGER
//        ImGui::Separator();
//
//        m_cc.DoMenuItemUI("toggle_6502_debugger");
//        if(ImGui::BeginMenu("Memory Debug")) {
//            m_cc.DoMenuItemUI("toggle_memory_debugger1");
//            m_cc.DoMenuItemUI("toggle_memory_debugger2");
//            m_cc.DoMenuItemUI("toggle_memory_debugger3");
//            m_cc.DoMenuItemUI("toggle_memory_debugger4");
//            ImGui::EndMenu();
//        }
//        if(ImGui::BeginMenu("External Memory Debug")) {
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger1");
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger2");
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger3");
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger4");
//            ImGui::EndMenu();
//        }
//        if(ImGui::BeginMenu("Disassembly Debug")) {
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger1");
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger2");
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger3");
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger4");
//            ImGui::EndMenu();
//        }
//        m_cc.DoMenuItemUI("toggle_crtc_debugger");
//        m_cc.DoMenuItemUI("toggle_video_ula_debugger");
//        m_cc.DoMenuItemUI("toggle_system_via_debugger");
//        m_cc.DoMenuItemUI("toggle_user_via_debugger");
//        m_cc.DoMenuItemUI("toggle_nvram_debugger");
//        m_cc.DoMenuItemUI("toggle_sn76489_debugger");
//        m_cc.DoMenuItemUI("toggle_paging_debugger");
//        m_cc.DoMenuItemUI("toggle_breakpoints_debugger");
//        m_cc.DoMenuItemUI("toggle_stack_debugger");
//
//        ImGui::Separator();
//
//        m_cc.DoMenuItemUI("debug_stop");
//        m_cc.DoMenuItemUI("debug_run");
//        m_cc.DoMenuItemUI("debug_step_over");
//        m_cc.DoMenuItemUI("debug_step_in");
//
//#endif

        ImGui::Separator();

#if ENABLE_IMGUI_DEMO
        ImGui::MenuItem("ImGui demo...",NULL,&m_imgui_demo);
#endif
        ImGui::MenuItem("ImGui dock debug...",nullptr,&m_imgui_dock_debug);
#if STORE_DRAWLISTS
        ImGui::MenuItem("ImGui drawlists...",nullptr,&m_imgui_drawlists);
#endif
        ImGui::MenuItem("ImGui debug...",nullptr,&m_imgui_debug);
        ImGui::MenuItem("ImGui metrics...",nullptr,&m_imgui_metrics);

        ImGui::EndMenu();
    }
#endif
}

void SDLBeebWindow::DoPopupUI(uint64_t now,int output_width,int output_height) {
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

//    BeebThreadTimelineState timeline_state;
//    m_beeb_thread->GetTimelineState(&timeline_state);

    bool show_leds_popup=false;

//    bool pasting=m_beeb_thread->IsPasting();
//    bool copying=m_beeb_thread->IsCopying();
    uint32_t leds=0;
    if(m_beeb) {
        leds=m_beeb->GetLEDs();
    }
    
    if(ValueChanged(&m_leds,leds)||(m_leds&BBCMicroLEDFlags_AllDrives)) {
        if(m_settings.leds_popup_mode!=BeebWindowLEDsPopupMode_Off) {
            show_leds_popup=true;
        }
//    } else if(timeline_state.mode!=BeebThreadTimelineMode_None||copying||pasting) {
//        // The LEDs window always appears in this situation.
//        show_leds_popup=true;
    } else if(m_settings.leds_popup_mode==BeebWindowLEDsPopupMode_On) {
        show_leds_popup=true;
    }

    if(show_leds_popup) {
        if(!m_leds_popup_ui_active) {
            m_leds_popup_ticks=now;
        }

        m_leds_popup_ui_active=true;
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

//            switch(timeline_state.mode) {
//            case BeebThreadTimelineMode_None:
//                ImGuiLED(false,"Replay");
//                break;
//
//            case BeebThreadTimelineMode_Replay:
//                ImGuiLED(true,"Replay");
//                ImGui::SameLine();
//                if(ImGui::Button("Stop")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopReplayMessage>());
//                }
//                break;
//
//            case BeebThreadTimelineMode_Record:
//                colour_pusher.Push(ImGuiCol_CheckMark,ImVec4(1.f,0.f,0.f,1.f));
//                ImGuiLED(true,"Record");
//                ImGui::SameLine();
//                if(ImGuiConfirmButton("Stop")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopRecordingMessage>());
//                }
//                colour_pusher.Pop();
//                break;
//            }

//            ImGui::SameLine();
//            ImGuiLED(copying,"Copy");
//            if(copying) {
//                ImGui::SameLine();
//                if(ImGui::Button("Cancel")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopCopyMessage>());
//                }
//            }
//
//            ImGui::SameLine();
//            ImGuiLED(pasting,"Paste");
//            if(pasting) {
//                ImGui::SameLine();
//                if(ImGui::Button("Cancel")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());
//                }
//            }
        }
        ImGui::End();

        if(GetSecondsFromTicks(now-m_leds_popup_ticks)>LEDS_POPUP_TIME_SECONDS) {
            if(m_settings.leds_popup_mode!=BeebWindowLEDsPopupMode_On) {
                m_leds_popup_ui_active=false;
            }
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

void SDLBeebWindow::UpdateTitle(uint64_t ticks) {
    double secs_elapsed=GetSecondsFromTicks(ticks-m_last_title_update_ticks);
    if(secs_elapsed<TITLE_UPDATE_TIME_SECONDS) {
        return;
    }
    
    char title[1000];
    if(!m_beeb) {
        snprintf(title,sizeof title,"b2 [-]");
        m_last_title_update_2MHz_cycles=0;
    } else {
        double speed=0.0;
        {
            uint64_t num_2MHz_cycles=*m_beeb->GetNum2MHzCycles();
            uint64_t num_2MHz_cycles_elapsed=num_2MHz_cycles-m_last_title_update_2MHz_cycles;
            
            if(m_last_title_update_ticks!=0) {
                double hz=num_2MHz_cycles_elapsed/secs_elapsed;
                speed=hz/2.e6;
            }
            
            m_last_title_update_2MHz_cycles=num_2MHz_cycles;
            m_last_title_update_ticks=ticks;
        }
        
        snprintf(title,sizeof title,"b2 [%.2fx]",speed);
    }
    
    SDL_SetWindowTitle(m_window.get(),title);
    m_last_title_update_ticks=ticks;
}

void SDLBeebWindow::ShowPrioritizeCommandShortcutsStatus() {
    if(m_prefer_shortcuts) {
        m_msg.i.f("Prioritize command keys\n");
    } else {
        m_msg.i.f("Prioritize BBC keys\n");
    }
}

bool SDLBeebWindow::HandleBeebKey(const SDL_Keysym &keysym,bool state) {
    if(!m_keymap) {
        return false;
    }

    if(m_keymap->IsKeySymMap()) {
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
                    this->Execute(std::make_unique<KeySymCommand>(beeb_keysym,false));
                }

                m_beeb_keysyms_by_keycode.erase(it);
            }
        }

        const int8_t *beeb_syms=m_keymap->GetValuesForPCKey(pc_key|modifiers);
        if(!beeb_syms) {
            // If key+modifier isn't bound, just go for key on its
            // own (and the modifiers will be applied in the
            // emulated BBC).
            beeb_syms=m_keymap->GetValuesForPCKey(pc_key&~PCKeyModifier_All);
        }

        if(!beeb_syms) {
            return false;
        }

        for(const int8_t *beeb_sym=beeb_syms;*beeb_sym>=0;++beeb_sym) {
            if(state) {
                m_beeb_keysyms_by_keycode[pc_key].insert((BeebKeySym)*beeb_sym);
                this->Execute(std::make_unique<KeySymCommand>((BeebKeySym)*beeb_sym,state));
                //m_beeb_thread->Send(std::make_shared<BeebThread::KeySymMessage>((BeebKeySym)*beeb_sym,state));
            }
        }
    } else {
        const int8_t *beeb_keys=m_keymap->GetValuesForPCKey(keysym.scancode);
        if(!beeb_keys) {
            return false;
        }

        for(const int8_t *beeb_key=beeb_keys;*beeb_key>=0;++beeb_key) {
            this->Execute(std::make_unique<KeyCommand>((BeebKey)*beeb_key,state));
            //m_beeb_thread->Send(std::make_shared<BeebThread::KeyMessage>((BeebKey)*beeb_key,state));
        }
    }

    return true;
}

//void SDLBeebWindow::RecordKeySym(BeebKeySym beeb_key_sym,bool down) {
//    BeebKey beeb_key;
//    BeebShiftState shift_state;
//    if(GetBeebKeyComboForKeySym(&beeb_key,&shift_state,beeb_key_sym)) {
//        this->SetKeyState(beeb_key,down);
//        this->SetFakeShiftState(shift_state);
//    }
//}

//void SDLBeebWindow::RecordKey(BeebKey beeb_key,bool down) {
//    this->SetKeyState(beeb_key,down);
//}

//void SDLBeebWindow::RecordLoadDisc(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose) {
//    m_beeb->SetDiscImage(drive,std::move(disc_image));
//}

//void SDLBeebWindow::RecordEjectDisc(int drive) {
//    m_beeb->SetDiscImage(drive,nullptr);
//}

//void SDLBeebWindow::RecordHardResetAndReloadConfig(uint32_t reset_flags) {
//    BeebLoadedConfig reloaded_config;
//    if(!BeebLoadedConfig::Load(&reloaded_config,m_current_config.config,&m_msg)) {
//        return;
//    }
//
//    reloaded_config.ReuseROMs(m_current_config);
//
//    this->RecordHardResetAndChangeConfig(reset_flags,std::move(reloaded_config));
//}

//void SDLBeebWindow::RecordHardResetAndChangeConfig(uint32_t reset_flags,BeebLoadedConfig config) {
//
//}

void SDLBeebWindow::SetKeyState(BeebKey beeb_key,bool down) {
    ASSERT(!(beeb_key&0x80));
    
    if(!m_beeb) {
        return;
    }
    
    if(IsNumericKeypadKey(beeb_key)) {
        if(!m_beeb->HasNumericKeypad()) {
            return;
        }
    }
    
    m_real_key_states.SetKeyState(beeb_key,down);
    
    // If it's the shift key, override using fake shift flags or
    // boot flag.
    if(beeb_key==BeebKey_Shift) {
        if(m_auto_boot) {
            down=true;
        } else if(m_fake_shift_state==BeebShiftState_On) {
            down=true;
        } else if(m_fake_shift_state==BeebShiftState_Off) {
            down=false;
        }
    } else {
        if(m_auto_boot) {
            // this is recursive, but it only calls
            // ThreadSetKeyState for BeebKey_Shift, so it won't
            // end up here again...
            this->SetAutoBootState(false);
        } else {
            // no harm in skipping it.
        }
    }
    
    //m_effective_key_states.SetState(beeb_key,state);
    m_beeb->SetKeyState(beeb_key,down);
    
#if BBCMICRO_TRACE
    // TODO: trace
//    if(ts->trace_conditions.start==BeebThreadStartTraceCondition_NextKeypress) {
//        if(m_is_tracing.load(std::memory_order_acquire)) {
//            if(state) {
//                if(ts->trace_conditions.start_key<0||beeb_key==ts->trace_conditions.start_key) {
//                    ts->trace_conditions.start=BeebThreadStartTraceCondition_Immediate;
//                    this->ThreadBeebStartTrace(ts);
//                }
//            }
//        }
//    }
#endif
}

void SDLBeebWindow::SetFakeShiftState(BeebShiftState shift_state) {
    m_fake_shift_state=shift_state;
    
    this->UpdateShiftKeyState();
}

void SDLBeebWindow::SetAutoBootState(bool auto_boot) {
    m_auto_boot=auto_boot;
    
    this->UpdateShiftKeyState();
}

void SDLBeebWindow::UpdateShiftKeyState() {
    bool shift_really_down=m_real_key_states.GetKeyState(BeebKey_Shift);
    this->SetKeyState(BeebKey_Shift,shift_really_down);
}

void SDLBeebWindow::Exit() {
    SDL_Event event={};
    event.type=SDL_QUIT;

    SDL_PushEvent(&event);
}

void SDLBeebWindow::SaveDefaultNVRAM() {
    if(m_beeb) {
        std::vector<uint8_t> nvram=m_beeb->GetNVRAM();
        if(!nvram.empty()) {
            const BBCMicroType *type=m_beeb->GetType();
            SetDefaultNVRAMContents(type,std::move(nvram));
        }
    }
}

bool SDLBeebWindow::SaveDefaultNVRAMIsEnabled() const {
    if(m_beeb) {
        if(m_beeb->HasNVRAM()) {
            return true;
        }
    }
    
    return false;
}

void SDLBeebWindow::SaveConfig() {
    this->UpdateSettings();
    
    SaveGlobalConfig(m_window_placement_data,m_settings,&m_msg);
    
    m_msg.i.f("Configuration saved.\n");
}

void SDLBeebWindow::UpdateSettings() {
    m_settings.dock_config=m_imgui_stuff->SaveDockContext();

    if(m_keymap) {
        m_settings.keymap_name=m_keymap->GetName();
    } else {
        m_settings.keymap_name.clear();
    }

    m_settings.config_name=m_current_config.config.name;
}

bool SDLBeebWindow::Execute(std::unique_ptr<Command> command) {
    CommandPrepareResult result=command->Prepare(this);
    switch(result) {
        default:
            ASSERT(false);
        case CommandPrepareResult_Fail:
            return false;
            
        case CommandPrepareResult_Ignore:
            return false;
            
        case CommandPrepareResult_Execute:
            command->Execute(this);
            return true;
    }
}

void SDLBeebWindow::HardReset() {
    this->Execute(std::make_unique<HardResetAndReloadConfigCommand>(ResetFlag_Run));
}

std::unique_ptr<SettingsUI> SDLBeebWindow::CreateOptionsUI(SDLBeebWindow *beeb_window) {
    return std::make_unique<OptionsUI>(beeb_window);
}

template<BeebWindowPopupType POPUP_TYPE>
void SDLBeebWindow::TogglePopupCommand() {
    m_settings.popups^=(uint64_t)1<<POPUP_TYPE;
}

template<BeebWindowPopupType POPUP_TYPE>
bool SDLBeebWindow::IsPopupCommandTicked() const {
    return !!(m_settings.popups&(uint64_t)1<<POPUP_TYPE);
}

template<BeebWindowPopupType POPUP_TYPE>
ObjectCommandTable<SDLBeebWindow>::Initializer SDLBeebWindow::GetTogglePopupCommand() {
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

    return ObjectCommandTable<SDLBeebWindow>::Initializer(CommandDef(command_name,
                                                                     std::move(text)),
                                                          &SDLBeebWindow::TogglePopupCommand<POPUP_TYPE>,
                                                          &SDLBeebWindow::IsPopupCommandTicked<POPUP_TYPE>);
}

const ObjectCommandTable<SDLBeebWindow> SDLBeebWindow::ms_command_table("Beeb Window (ver 2)",{
    {{"hard_reset","Hard Reset"},&SDLBeebWindow::HardReset},
    //    {{"load_last_state","Load Last State"},&BeebWindow::LoadLastState,nullptr,&BeebWindow::IsLoadLastStateEnabled},
//    {{"save_state","Save State"},&BeebWindow::SaveState,nullptr,&BeebWindow::SaveStateIsEnabled},
    GetTogglePopupCommand<BeebWindowPopupType_Options>(),
    GetTogglePopupCommand<BeebWindowPopupType_Keymaps>(),
//    GetTogglePopupCommand<BeebWindowPopupType_Timeline>(),
//    GetTogglePopupCommand<BeebWindowPopupType_SavedStates>(),
//    GetTogglePopupCommand<BeebWindowPopupType_Messages>(),
//    GetTogglePopupCommand<BeebWindowPopupType_Configs>(),
//#if BBCMICRO_TRACE
//    GetTogglePopupCommand<BeebWindowPopupType_Trace>(),
//#endif
    GetTogglePopupCommand<BeebWindowPopupType_AudioCallback>(),
//    GetTogglePopupCommand<BeebWindowPopupType_CommandContextStack>(),
    GetTogglePopupCommand<BeebWindowPopupType_CommandKeymaps>(),
    {CommandDef("exit","Exit").MustConfirm(),&SDLBeebWindow::Exit},
//    {CommandDef("clean_up_recent_files_lists","Clean up recent files lists").MustConfirm(),&BeebWindow::CleanUpRecentFilesLists},
//    {CommandDef("reset_dock_windows","Reset dock windows").MustConfirm(),&BeebWindow::ResetDockWindows},
//#if SYSTEM_WINDOWS
//    {{"clear_console","Clear Win32 console"},&BeebWindow::ClearConsole},
//#endif
//    {{"print_separator","Print stdout separator"},&BeebWindow::PrintSeparator},
//    {{"paste","OSRDCH Paste"},&BeebWindow::Paste,&BeebWindow::IsPasteTicked},
//    {{"paste_return","OSRDCH Paste (+Return)"},&BeebWindow::PasteThenReturn,&BeebWindow::IsPasteTicked},
//    {{"toggle_copy_oswrch_text","OSWRCH Copy Text"},&BeebWindow::CopyOSWRCH<true>,&BeebWindow::IsCopyOSWRCHTicked},
//    {{"copy_basic","Copy BASIC listing"},&BeebWindow::CopyBASIC,&BeebWindow::IsCopyOSWRCHTicked,&BeebWindow::IsCopyBASICEnabled},
//#if VIDEO_TRACK_METADATA
//    GetTogglePopupCommand<BeebWindowPopupType_PixelMetadata>(),
//#endif
//#if ENABLE_IMGUI_TEST
//    GetTogglePopupCommand<BeebWindowPopupType_DearImguiTest>(),
//#endif
//#if BBCMICRO_DEBUGGER
//    GetTogglePopupCommand<BeebWindowPopupType_6502Debugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger1>(),
//    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger2>(),
//    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger3>(),
//    GetTogglePopupCommand<BeebWindowPopupType_MemoryDebugger4>(),
//    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger1>(),
//    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger2>(),
//    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger3>(),
//    GetTogglePopupCommand<BeebWindowPopupType_ExtMemoryDebugger4>(),
//    GetTogglePopupCommand<BeebWindowPopupType_DisassemblyDebugger1>(),
//    GetTogglePopupCommand<BeebWindowPopupType_DisassemblyDebugger2>(),
//    GetTogglePopupCommand<BeebWindowPopupType_DisassemblyDebugger3>(),
//    GetTogglePopupCommand<BeebWindowPopupType_DisassemblyDebugger4>(),
//    GetTogglePopupCommand<BeebWindowPopupType_CRTCDebugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_VideoULADebugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_SystemVIADebugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_UserVIADebugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_NVRAMDebugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_BeebLink>(),
//    GetTogglePopupCommand<BeebWindowPopupType_SN76489Debugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_PagingDebugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_BreakpointsDebugger>(),
//    GetTogglePopupCommand<BeebWindowPopupType_StackDebugger>(),
//
//    {CommandDef("debug_stop","Stop").Shortcut(SDLK_F5|PCKeyModifier_Shift),&BeebWindow::DebugStop,nullptr,&BeebWindow::DebugIsStopEnabled},
//    {CommandDef("debug_run","Run").Shortcut(SDLK_F5),&BeebWindow::DebugRun,nullptr,&BeebWindow::DebugIsRunEnabled},
//    {CommandDef("debug_step_over","Step Over").Shortcut(SDLK_F10),&BeebWindow::DebugStepOver,nullptr,&BeebWindow::DebugIsRunEnabled},
//    {CommandDef("debug_step_in","Step In").Shortcut(SDLK_F11),&BeebWindow::DebugStepIn,nullptr,&BeebWindow::DebugIsRunEnabled},
//#endif
    {CommandDef("save_default_nvram","Save default NVRAM"),&SDLBeebWindow::SaveDefaultNVRAM,nullptr,&SDLBeebWindow::SaveDefaultNVRAMIsEnabled},
//    {CommandDef("reset_default_nvram","Reset default NVRAM").MustConfirm(),&BeebWindow::ResetDefaultNVRAM,nullptr,&BeebWindow::SaveDefaultNVRAMIsEnabled},
    {CommandDef("save_config","Save config"),&SDLBeebWindow::SaveConfig},
//    {CommandDef("toggle_prioritize_shortcuts","Prioritize command keys"),&BeebWindow::TogglePrioritizeCommandShortcuts,&BeebWindow::IsPrioritizeCommandShortcutsTicked,nullptr},
});
