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
#include "CommandKeymapsUI.h"
#include "DearImguiTestUI.h"
#include "debugger.h"
#include "HTTPServer.h"
#include "DirectDiscImage.h"
#include "SavedStatesUI.h"
#include "BeebLinkUI.h"
#include "SettingsUI.h"
#include "discs.h"
#include "profiler.h"
#include "joysticks.h"
#include <stb_image_write.h>
#if SYSTEM_WINDOWS
#include <dwmapi.h>
#endif

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

static TimerDef g_HandleVBlank_timer_def("BeebWindow::HandleVBlank");
static TimerDef g_HandleVBlank_end_of_frame_timer_def("BeebWindow::HandleVBlank end of frame",
                                                      &g_HandleVBlank_timer_def);
static TimerDef g_HandleVBlank_start_of_frame_timer_def("BeebWindow::HandleVBlank start of frame",
                                                        &g_HandleVBlank_timer_def);
static TimerDef g_HandleVBlank_UpdateTVTexture_Consume_timer_def("UpdateTVTexture Consume",
                                                                 &g_HandleVBlank_end_of_frame_timer_def);
static TimerDef g_HandleVBlank_UpdateTVTexture_Copy_timer_def("UpdateTVTexture Copy",
                                                              &g_HandleVBlank_end_of_frame_timer_def);
static TimerDef g_HandleVBlank_RenderSDL_timer_def("Render SDL",
                                                   &g_HandleVBlank_end_of_frame_timer_def);
static TimerDef g_HandleVBlank_DoImGui_timer_def("DoImGui",
                                                 &g_HandleVBlank_end_of_frame_timer_def);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static CommandTable2 g_beeb_window_command_table("Beeb Window");
#if SYSTEM_WINDOWS
static Command2 g_toggle_console_command = Command2(&g_beeb_window_command_table, "toggle_console", "Show Win32 console").WithTick();
static Command2 g_clear_console_command(&g_beeb_window_command_table, "clear_console", "Clear Win32 console");
static Command2 g_print_separator_command(&g_beeb_window_command_table, "print_separator", "Print stdout separator");
#endif
static Command2 g_hard_reset_command(&g_beeb_window_command_table, "hard_reset", "Hard Reset");
static Command2 g_save_state_command(&g_beeb_window_command_table, "save_state", "Save State");
static Command2 g_exit_command = Command2(&g_beeb_window_command_table, "exit", "Exit").MustConfirm();
static Command2 g_clean_up_recent_files_lists_command = Command2(&g_beeb_window_command_table, "clean_up_recent_files_lists", "Clean up recent files lists").MustConfirm();
static Command2 g_reset_dock_windows_command = Command2(&g_beeb_window_command_table, "reset_dock_windows", "Reset dock windows").MustConfirm();
static Command2 g_paste_command(&g_beeb_window_command_table, "paste", "OSRDCH Paste");
static Command2 g_paste_return_command(&g_beeb_window_command_table, "paste_return", "OSRDCH Paste (+Return)");
static Command2 g_toggle_copy_oswrch_text_command = Command2(&g_beeb_window_command_table, "toggle_copy_oswrch_text", "Copy OSWRCH text output").WithTick();
static Command2 g_copy_basic_command = Command2(&g_beeb_window_command_table, "copy_basic", "Copy BASIC listing");
static Command2 g_copy_translation_pass_through = Command2(&g_beeb_window_command_table, "copy_translation_none", "No translation").WithTick().WithExtraText("Copy text");
static Command2 g_copy_translation_only_gbp = Command2(&g_beeb_window_command_table, "copy_translation_only_gbp", "Translate " POUND_SIGN_UTF8 " only").WithTick().WithExtraText("Copy text");
static Command2 g_copy_translation_SAA5050 = Command2(&g_beeb_window_command_table, "copy_translation_SAA5050", "Translate Mode 7 chars").WithTick().WithExtraText("Copy text");
static Command2 g_copy_toggle_handle_delete = Command2(&g_beeb_window_command_table, "copy_toggle_handle_delete", "Handle delete").WithTick().WithExtraText("Copy text");
static Command2 g_printer_translation_pass_through = Command2(&g_beeb_window_command_table, "printer_translation_none", "No translation").WithTick().WithExtraText("Copy printer");
static Command2 g_printer_translation_only_gbp = Command2(&g_beeb_window_command_table, "printer_translation_only_gbp", "Translate " POUND_SIGN_UTF8 " only").WithTick().WithExtraText("Copy printer");
static Command2 g_printer_translation_SAA5050 = Command2(&g_beeb_window_command_table, "printer_translation_SAA5050", "Translate Mode 7 chars").WithTick().WithExtraText("Copy printer");
static Command2 g_printer_toggle_handle_delete = Command2(&g_beeb_window_command_table, "printer_toggle_handle_delete", "Handle delete").WithTick().WithExtraText("Copy printer");
static Command2 g_parallel_printer_command = Command2(&g_beeb_window_command_table, "parallel_printer", "Parallel printer").WithTick();
static Command2 g_reset_printer_buffer_command = Command2(&g_beeb_window_command_table, "reset_printer_buffer", "Reset printer buffer").MustConfirm();
static Command2 g_copy_printer_buffer_command = Command2(&g_beeb_window_command_table, "copy_printer_buffer", "Copy printer buffer");
static Command2 g_save_printer_buffer_command = Command2(&g_beeb_window_command_table, "save_printer_buffer", "Save printer buffer...");
static Command2 g_debug_stop_command = Command2(&g_beeb_window_command_table, "debug_stop", "Stop").WithShortcut(SDLK_F5 | PCKeyModifier_Shift).VisibleIf(BBCMICRO_DEBUGGER);
static Command2 g_debug_run_command = Command2(&g_beeb_window_command_table, "debug_run", "Run").WithShortcut(SDLK_F5).VisibleIf(BBCMICRO_DEBUGGER);
static Command2 g_save_default_nvram_command = Command2(&g_beeb_window_command_table, "save_default_nvram", "Save CMOS/EEPROM contents");
static Command2 g_reset_default_nvram_command = Command2(&g_beeb_window_command_table, "reset_default_nvram", "Reset CMOS/EEPROM").MustConfirm();
static Command2 g_save_config_command = Command2(&g_beeb_window_command_table, "save_config", "Save config");
static Command2 g_toggle_prioritize_shortcuts_command = Command2(&g_beeb_window_command_table, "toggle_prioritize_shortcuts", "Prioritize command keys").WithTick();
static Command2 g_save_screenshot_command = Command2(&g_beeb_window_command_table, "save_screenshot", "Save screenshot");
static Command2 g_copy_screenshot_command = Command2(&g_beeb_window_command_table, "copy_screenshot", "Copy screenshot");
#if ENABLE_SDL_FULL_SCREEN
static Command2 g_toggle_full_screen_command = Command2(&g_beeb_window_command_table, "toggle_full_screen", "Full screen").WithTick();
#endif
static Command2 g_new_window_command = Command2(&g_beeb_window_command_table, "new_window", "New window");
static Command2 g_clone_window_command = Command2(&g_beeb_window_command_table, "clone_window", "Clone window");
static Command2 g_close_window_command = Command2(&g_beeb_window_command_table, "close_window", "Close window");
static Command2 g_toggle_capture_mouse_command = Command2(&g_beeb_window_command_table, "toggle_capture_mouse", "Capture mouse").WithTick().AlwaysPrioritized();
static Command2 g_toggle_capture_mouse_on_click_command = Command2(&g_beeb_window_command_table, "toggle_capture_mouse_on_click", "Capture on click").WithTick();

struct PopupMetadata {
    Command2 command;
    std::function<std::unique_ptr<SettingsUI>(BeebWindow *)> create_fun_1;
    std::function<std::unique_ptr<SettingsUI>(BeebWindow *, ImGuiStuff *)> create_fun_2;
};

static std::unique_ptr<SettingsUI> CreatePopup(const PopupMetadata &popup, BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) {
    if (!!popup.create_fun_1) {
        return popup.create_fun_1(beeb_window);
    } else if (!!popup.create_fun_2) {
        return popup.create_fun_2(beeb_window, imgui_stuff);
    } else {
        return nullptr;
    }
}

static PopupMetadata g_popups[BeebWindowPopupType_MaxValue];
static bool g_popups_visibility_checked = false;

static void InitialiseTogglePopupCommand(BeebWindowPopupType type, const char *name, const char *text, std::function<std::unique_ptr<SettingsUI>(BeebWindow *)> create_fun) {
    PopupMetadata *p = &g_popups[type];
    p->command = Command2(&g_beeb_window_command_table, name, text).WithTick();
    p->create_fun_1 = std::move(create_fun);
}

static void InitialiseTogglePopupCommand(BeebWindowPopupType type, const char *name, const char *text, std::function<std::unique_ptr<SettingsUI>(BeebWindow *, ImGuiStuff *)> create_fun_2) {
    PopupMetadata *p = &g_popups[type];
    p->command = Command2(&g_beeb_window_command_table, name, text).WithTick();
    p->create_fun_2 = std::move(create_fun_2);
}

static bool InitialiseTogglePopupCommands() {
    InitialiseTogglePopupCommand(BeebWindowPopupType_Keymaps, "toggle_keyboard_layout", "Keyboard Layouts", &CreateKeymapsUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_CommandKeymaps, "toggle_command_keymaps", "Command Keys", &CreateCommandKeymapsUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_Options, "toggle_emulator_options", "Options", &BeebWindow::CreateOptionsUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_Messages, "toggle_messages", "Messages", &CreateMessagesUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_Timeline, "toggle_timeline", "Timeline", &BeebWindow::CreateTimelineUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SavedStates, "toggle_saved_states", "Saved States", &BeebWindow::CreateSavedStatesUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_Configs, "toggle_configurations", "Configs", &CreateConfigsUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_Trace, "toggle_event_trace", "Tracing", &CreateTraceUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_AudioCallback, "toggle_date_rate", "Performance", &CreateDataRateUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_PixelMetadata, "toggle_pixel_metadata", "Pixel Metadata", &CreatePixelMetadataDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_DearImguiTest, "toggle_dear_imgui_test", "Dear ImGui Test", &CreateDearImguiTestUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_6502Debugger, "toggle_6502_debugger", "Host 6502 Debug", &CreateHost6502DebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_Parasite6502Debugger, "toggle_parasite_6502_debugger", "Parasite 6502 Debug", &CreateParasite6502DebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_MemoryDebugger1, "toggle_memory_debugger1", "Host Memory Debug 1", &CreateHostMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_MemoryDebugger2, "toggle_memory_debugger2", "Host Memory Debug 2", &CreateHostMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_MemoryDebugger3, "toggle_memory_debugger3", "Host Memory Debug 3", &CreateHostMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_MemoryDebugger4, "toggle_memory_debugger4", "Host Memory Debug 4", &CreateHostMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ExtMemoryDebugger1, "toggle_ext_memory_debugger1", "External Memory Debug 1", &CreateExtMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ExtMemoryDebugger2, "toggle_ext_memory_debugger2", "External Memory Debug 2", &CreateExtMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ExtMemoryDebugger3, "toggle_ext_memory_debugger3", "External Memory Debug 3", &CreateExtMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ExtMemoryDebugger4, "toggle_ext_memory_debugger4", "External Memory Debug 4", &CreateExtMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_DisassemblyDebugger1, "toggle_disassembly_debugger1", "Host Disassembly Debug 1",
                                 [](BeebWindow *beeb_window) {
                                     return CreateHostDisassemblyDebugWindow(beeb_window, true);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_DisassemblyDebugger2, "toggle_disassembly_debugger2", "Host Disassembly Debug 2",
                                 [](BeebWindow *beeb_window) {
                                     return CreateHostDisassemblyDebugWindow(beeb_window, false);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_DisassemblyDebugger3, "toggle_disassembly_debugger3", "Host Disassembly Debug 3",
                                 [](BeebWindow *beeb_window) {
                                     return CreateHostDisassemblyDebugWindow(beeb_window, false);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_DisassemblyDebugger4, "toggle_disassembly_debugger4", "Host Disassembly Debug 4",
                                 [](BeebWindow *beeb_window) {
                                     return CreateHostDisassemblyDebugWindow(beeb_window, false);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_CRTCDebugger, "toggle_crtc_debugger", "CRTC Debug", &CreateCRTCDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_VideoULADebugger, "toggle_video_ula_debugger", "Video ULA Debug", &CreateVideoULADebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SystemVIADebugger, "toggle_system_via_debugger", "System VIA Debug", &CreateSystemVIADebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_UserVIADebugger, "toggle_user_via_debugger", "User VIA Debug", &CreateUserVIADebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_NVRAMDebugger, "toggle_nvram_debugger", "NVRAM Debug", &CreateNVRAMDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SN76489Debugger, "toggle_sn76489_debugger", "SN76489 Debug", &CreateSN76489DebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_PagingDebugger, "toggle_paging_debugger", "Paging Debug", &CreatePagingDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_BreakpointsDebugger, "toggle_breakpoints_debugger", "Breakpoints", &CreateBreakpointsDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_StackDebugger, "toggle_stack_debugger", "Stack", &CreateHostStackDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteStackDebugger, "toggle_parasite_stack_debugger", "Parasite Stack", &CreateParasiteStackDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteMemoryDebugger1, "toggle_parasite_memory_debugger1", "Parasite Memory Debug 1", &CreateParasiteMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteMemoryDebugger2, "toggle_parasite_memory_debugger2", "Parasite Memory Debug 2", &CreateParasiteMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteMemoryDebugger3, "toggle_parasite_memory_debugger3", "Parasite Memory Debug 3", &CreateParasiteMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteMemoryDebugger4, "toggle_parasite_memory_debugger4", "Parasite Memory Debug 4", &CreateParasiteMemoryDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteDisassemblyDebugger1, "toggle_parasite_disassembly_debugger1", "Parasite Disassembly Debug 1",
                                 [](BeebWindow *beeb_window) {
                                     return CreateParasiteDisassemblyDebugWindow(beeb_window, true);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteDisassemblyDebugger2, "toggle_parasite_disassembly_debugger2", "Parasite Disassembly Debug 2",
                                 [](BeebWindow *beeb_window) {
                                     return CreateParasiteDisassemblyDebugWindow(beeb_window, false);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteDisassemblyDebugger3, "toggle_parasite_disassembly_debugger3", "Parasite Disassembly Debug 3",
                                 [](BeebWindow *beeb_window) {
                                     return CreateParasiteDisassemblyDebugWindow(beeb_window, false);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_ParasiteDisassemblyDebugger4, "toggle_parasite_disassembly_debugger4", "Parasite Disassembly Debug 4",
                                 [](BeebWindow *beeb_window) {
                                     return CreateParasiteDisassemblyDebugWindow(beeb_window, false);
                                 });
    InitialiseTogglePopupCommand(BeebWindowPopupType_TubeDebugger, "toggle_tube_debugger", "Tube Debug", &CreateTubeDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ADCDebugger, "toggle_adc_debugger", "ADC Debug", &CreateADCDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_BeebLink, "toggle_beeblink_options", "BeebLink Options", &CreateBeebLinkUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_DigitalJoystickDebugger, "toggle_digital_joystick_debugger", "Digital Joystick Debug", &CreateDigitalJoystickDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ImGuiDebug, "toggle_imgui_debug", "Imgui debug", &BeebWindow::CreateImGuiDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_KeyboardDebug, "toggle_keyboard_debug", "Keyboard debug", &CreateKeyboardDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SystemDebug, "toggle_system_debug", "System debug", &CreateSystemDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_MouseDebug, "toggle_mouse_debug", "Mouse debug", &CreateMouseDebugWindow);
    return true;
}

static const bool g_toggle_popup_commands_initialised = InitialiseTogglePopupCommands();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(OUTPUT);
LOG_EXTERN(OUTPUTND);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if RMT_ENABLED
static size_t g_num_BeebWindow_inits = 0;
#if RMT_USE_OPENGL
static int g_unbind_opengl = 0;
#endif
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const double MESSAGES_POPUP_TIME_SECONDS = 2.5;
static const double LEDS_POPUP_TIME_SECONDS = 1.;
static constexpr double CORRECT_ASPECT_RATIO_X_SCALE = 1.2 / 1.25;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_DISC_IMAGE = "disc_image";
//static const std::string RECENT_PATHS_RAM="ram";
static const std::string RECENT_PATHS_NVRAM = "nvram";
static const std::string RECENT_PATHS_SCREENSHOT = "screenshot";
static const std::string RECENT_PATHS_PRINTER = "printer";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// GetWindowData has a loop (!) with strcmp in it (!) so the data name
// wants to be short.
const char BeebWindow::SDL_WINDOW_DATA_NAME[] = "D";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::DriveState::DriveState()
    : new_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE)
    , open_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE)
    , new_direct_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE)
    , open_direct_disc_image_file_dialog(RECENT_PATHS_DISC_IMAGE) {
    this->new_disc_image_file_dialog.AddFilter("BBC disc images", DISC_IMAGE_EXTENSIONS);

    {
        std::vector<std::string> extensions = DISC_IMAGE_EXTENSIONS;
        extensions.push_back(".zip");

        this->open_disc_image_file_dialog.AddFilter("BBC disc images", extensions);
        this->open_disc_image_file_dialog.AddAllFilesFilter();
    }

    this->new_direct_disc_image_file_dialog.AddFilter("BBC disc images", DISC_IMAGE_EXTENSIONS);

    this->open_direct_disc_image_file_dialog.AddFilter("BBC disc images", DISC_IMAGE_EXTENSIONS);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebWindow::ImGuiDebugUI : public SettingsUI {
  public:
    explicit ImGuiDebugUI(BeebWindow *beeb_window);

    void DoImGui() override;

    bool OnClose() override;

  protected:
  private:
    BeebWindow *m_beeb_window = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::ImGuiDebugUI::ImGuiDebugUI(BeebWindow *beeb_window)
    : m_beeb_window(beeb_window) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::ImGuiDebugUI::DoImGui() {
    m_beeb_window->m_imgui_stuff->DoDebugGui();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::ImGuiDebugUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebWindow::OptionsUI : public SettingsUI {
  public:
    explicit OptionsUI(BeebWindow *beeb_window);

    void DoImGui() override;

    bool OnClose() override;

  protected:
  private:
    BeebWindow *m_beeb_window = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::OptionsUI::OptionsUI(BeebWindow *beeb_window)
    : m_beeb_window(beeb_window) {
    this->SetDefaultSize(ImVec2(450, 450));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::OptionsUI::DoImGui() {
    const std::shared_ptr<BeebThread> &beeb_thread = m_beeb_window->m_beeb_thread;
    BeebWindowSettings *settings = &m_beeb_window->m_settings;

    //    {
    //        bool paused=m_beeb_window->m_beeb_thread->IsPaused();
    //        if(ImGui::Checkbox("Paused",&paused)) {
    //            beeb_thread->Send(std::make_shared<BeebThread::PauseMessage>(paused));
    //        }
    //    }

    {
        float speed_scale = beeb_thread->GetSpeedScale();
        bool limit_speed = beeb_thread->IsSpeedLimited();

        if (ImGui::Checkbox("Limit Speed", &limit_speed)) {
            beeb_thread->Send(std::make_shared<BeebThread::SetSpeedLimitedMessage>(limit_speed));
        }

        if (limit_speed) {
            ImGui::SameLine();

            bool changed = false;

            if (ImGui::Button("1x")) {
                speed_scale = 1.f;
                changed = true;
            }

            if (ImGui::SliderFloat("Speed scale", &speed_scale, 0.f, 2.f)) {
                changed = true;
            }

            if (changed) {
                beeb_thread->Send(std::make_shared<BeebThread::SetSpeedScaleMessage>(speed_scale));
            }
        }
    }

    ImGui::NewLine();

    {
        ImGuiHeader("Display");

        ImGui::Checkbox("Correct aspect ratio", &settings->correct_aspect_ratio);

        if (ImGui::Checkbox("Filter display", &settings->display_filter)) {
            m_beeb_window->RequestRecreateTexture();
        }

        ImGui::Checkbox("Auto scale", &settings->display_auto_scale);

        ImGui::DragFloat("Manual scale", &settings->display_manual_scale, .01f, 0.f, 10.f);

        ImGui::Checkbox("Emulate interlace", &settings->display_interlace);

#if 1 //BUILD_TYPE_Debug
        if (ImGui::Checkbox("Threaded texture update", &m_beeb_window->m_update_tv_texture_thread_enabled)) {
            ResetTimerDefs();
        }
#endif
    }

    ImGui::NewLine();

    {
        ImGuiHeader("Screenshot");
        ImGuiIDPusher pusher(1);

        ImGui::Checkbox("Correct aspect ratio", &settings->screenshot_correct_aspect_ratio);
#if HAVE_SDL_SOFTSTRETCHLINEAR
        ImGui::Checkbox("Bilinear filtering", &settings->screenshot_filter);
#endif
        ImGui::Checkbox("Last completed frame", &settings->screenshot_last_vsync);
    }

    ImGui::NewLine();

    {
        ImGuiHeader("Sound");

        if (ImGui::SliderFloat("BBC volume", &settings->bbc_volume, MIN_DB, MAX_DB, "%.1f dB")) {
            beeb_thread->SetBBCVolume(settings->bbc_volume);
        }

        if (ImGui::SliderFloat("Disc volume", &settings->disc_volume, MIN_DB, MAX_DB, "%.1f dB")) {
            beeb_thread->SetDiscVolume(settings->disc_volume);
        }

        if (ImGui::Checkbox("Power-on tone", &settings->power_on_tone)) {
            beeb_thread->SetPowerOnTone(settings->power_on_tone);
        }
    }

    ImGui::NewLine();

    {
        ImGuiHeader("UI");

        unsigned font_size = m_beeb_window->m_imgui_stuff->GetFontSizePixels();
        if (ImGuiInputUInt("GUI Font Size", &font_size, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue)) {
            m_beeb_window->m_imgui_stuff->SetFontSizePixels(font_size);
            settings->gui_font_size = m_beeb_window->m_imgui_stuff->GetFontSizePixels();
        }
    }

    ImGui::NewLine();

    {
        ImGuiHeader("HTTP Server");
        int port = GetHTTPServerListenPort();
        if (port == 0) {
            ImGui::TextUnformatted("HTTP server not running");
            if (ImGui::Button("Start HTTP server")) {
                StartHTTPServer(&m_beeb_window->m_msg);
            }
        } else {
            ImGui::Text("HTTP server listening on port %d (0x%x)", port, port);
            if (ImGui::Button("Stop HTTP server")) {
                StopHTTPServer();
            }
        }
    }

    ImGui::NewLine();

#if BBCMICRO_DEBUGGER
    {
        ImGuiHeader("Debug Options");

        std::shared_ptr<const BBCMicroReadOnlyState> beeb_state;
        m_beeb_window->m_beeb_thread->DebugGetState(&beeb_state, nullptr);

        bool teletext_debug = beeb_state->saa5050.debug;
        if (ImGui::Checkbox("Teletext debug", &teletext_debug)) {
            m_beeb_window->m_beeb_thread->Send(
                std::make_shared<BeebThread::CallbackMessage>([teletext_debug](BBCMicro *m) -> void {
                    m->SetTeletextDebug(teletext_debug);
                }));
        }

        ImGui::Checkbox("Show TV beam position", &m_beeb_window->m_tv.show_beam_position);
        if (ImGui::Checkbox("Test pattern", &m_beeb_window->m_test_pattern)) {
            if (m_beeb_window->m_test_pattern) {
                m_beeb_window->m_tv.FillWithTestPattern();
            }
        }

        ImGui::Checkbox("Fill window (overrides auto scale/correct aspect ratio)", &m_beeb_window->m_display_fill);

        ImGui::Checkbox("1.0 usec", &m_beeb_window->m_tv.show_usec_markers);
        ImGui::SameLine();
        ImGui::Checkbox("0.5 usec", &m_beeb_window->m_tv.show_half_usec_markers);

        ImGui::Checkbox("6845 rows", &m_beeb_window->m_tv.show_6845_row_markers);
        ImGui::SameLine();
        ImGui::Checkbox("6845 DISPEN", &m_beeb_window->m_tv.show_6845_dispen_markers);
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

BeebWindow::BeebWindow(BeebWindowInitArguments init_arguments)
    : m_init_arguments(std::move(init_arguments)) {
    m_name = m_init_arguments.name;

    m_message_list = std::make_shared<MessageList>("BeebWindow");
    m_msg.SetMessageList(m_message_list);

    m_beeb_thread = std::make_shared<BeebThread>(m_message_list,
                                                 m_init_arguments.sound_device,
                                                 m_init_arguments.sound_spec.freq,
                                                 m_init_arguments.sound_spec.samples,
                                                 m_init_arguments.default_config,
                                                 std::vector<BeebThread::TimelineEventList>());

    if (init_arguments.use_settings) {
        m_settings = init_arguments.settings;
    } else {
        m_settings = BeebWindows::defaults;
    }

    m_beeb_thread->SetBBCVolume(m_settings.bbc_volume);
    m_beeb_thread->SetDiscVolume(m_settings.disc_volume);
    m_beeb_thread->SetPowerOnTone(m_settings.power_on_tone);

    m_blend_amt = 1.f;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::~BeebWindow() {
    m_beeb_thread->Stop();

    if (m_update_tv_texture_thread.joinable()) {
        {
            LockGuard<Mutex> lock(m_update_tv_texture_state.mutex);

            m_update_tv_texture_state.stop = true;
        }

        m_update_tv_texture_state.update_cv.notify_one();

        m_update_tv_texture_thread.join();
    }

    // Clear these explicitly before destroying the dear imgui stuff
    // and shutting down SDL.
    for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
        m_popups[i] = nullptr;
    }

    delete m_imgui_stuff;
    m_imgui_stuff = nullptr;

    if (m_tv_texture) {
        SDL_DestroyTexture(m_tv_texture);
    }

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
    }

#if RMT_ENABLED
    if (g_num_BeebWindow_inits > 0) {
        --g_num_BeebWindow_inits;
    }

    if (g_num_BeebWindow_inits == 0) {
#if RMT_USE_OPENGL
        if (g_unbind_opengl) {
            rmt_UnbindOpenGL();
            g_unbind_opengl = 0;
        }
#endif
    }
#endif

    if (m_sound_device != 0) {
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
    m_name = std::move(name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::GetBeebKeyState(BeebKey key) const {
    if (key < 0) {
        return false;
    } else if (key == BeebKey_Break) {
        return false;
    } else {
        return m_beeb_thread->GetKeyState(key);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLFocusGainedEvent() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLFocusLostEvent() {
    for (uint8_t i = 0; i < NUM_BEEB_JOYSTICKS; ++i) {
        auto message = std::make_shared<BeebThread::JoystickButtonMessage>(i, false);
        m_beeb_thread->Send(std::move(message));
    }

    this->SetCaptureMouse(false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLKeyEvent(const SDL_KeyboardEvent &event) {

    if (m_imgui_stuff) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if (event.repeat) {
                // Don't set again if it's just key repeat. If the flag is
                // still set from last time, that's fine; if it's been reset,
                // there'll be a reason, so don't set it again.
            } else {
                if (m_imgui_stuff->AddKeyEvent(event.keysym.scancode, true)) {
                    m_sdl_keyboard_events.push_back(event);
                }
            }
            break;

        case SDL_KEYUP:
            if (m_imgui_stuff->AddKeyEvent(event.keysym.scancode, false)) {
                m_sdl_keyboard_events.push_back(event);
            }
            break;
        }
    } else {
        m_sdl_keyboard_events.push_back(event);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLMouseButtonEvent(const SDL_MouseButtonEvent &event) {
    if (m_is_mouse_captured) {
        uint8_t mask = 0;

        switch (event.button) {
        case 1:
            mask = BBCMicroMouseButton_Left;
            break;

        case 2:
            mask = BBCMicroMouseButton_Middle;
            break;

        case 3:
            mask = BBCMicroMouseButton_Right;
            break;
        }

        if (mask != 0) {
            m_beeb_thread->Send(std::make_shared<BeebThread::MouseButtonsMessage>(mask, event.state == SDL_PRESSED ? mask : (uint8_t)0));
        }
    } else if (m_imgui_stuff) {
        m_imgui_stuff->AddMouseButtonEvent(event.button, event.type == SDL_MOUSEBUTTONDOWN);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLControllerAxisMotionEvent(const SDL_ControllerAxisEvent &event) {
    JoystickResult jr = ControllerAxisMotion(event.which, event.axis, event.value);
    this->HandleJoystickResult(jr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLControllerButtonEvent(const SDL_ControllerButtonEvent &event) {
    JoystickResult jr = ControllerButton(event.which, event.button, event.state == SDL_PRESSED);
    this->HandleJoystickResult(jr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleJoystickResult(const JoystickResult &jr) {
    if (jr.channel >= 0) {
        auto message = std::make_shared<BeebThread::AnalogueChannelMessage>(jr.channel, jr.channel_value);
        m_beeb_thread->Send(std::move(message));
    }

    if (jr.digital_joystick_index == 0 || jr.digital_joystick_index == 1) {
        auto message = std::make_shared<BeebThread::JoystickButtonMessage>(jr.digital_joystick_index, jr.digital_state.bits.fire0);
        m_beeb_thread->Send(std::move(message));
    } else if (jr.digital_joystick_index == 2) {
        auto message = std::make_shared<BeebThread::DigitalJoystickStateMessage>(jr.digital_joystick_index, jr.digital_state);
        m_beeb_thread->Send(std::move(message));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HandleBeebKey(const SDL_Keysym &keysym, bool state) {
    const BeebKeymap *keymap = m_settings.keymap;
    if (!keymap) {
        return false;
    }

    if (keymap->IsKeySymMap()) {
        uint32_t pc_key = (uint32_t)keysym.sym;
        if (pc_key & PCKeyModifier_All) {
            // Bleargh... can't handle this one.
            return false;
        }

        uint32_t modifiers = GetPCKeyModifiersFromSDLKeymod(keysym.mod);

        if (!state) {
            auto &&it = m_beeb_keysyms_by_keycode.find(pc_key);

            if (it != m_beeb_keysyms_by_keycode.end()) {
                for (BeebKeySym beeb_keysym : it->second) {
                    m_beeb_thread->Send(std::make_shared<BeebThread::KeySymMessage>(beeb_keysym, false));
                }

                m_beeb_keysyms_by_keycode.erase(it);
            }
        }

        const int8_t *beeb_syms = keymap->GetValuesForPCKey(pc_key | modifiers);
        if (!beeb_syms) {
            // If key+modifier isn't bound, just go for key on its
            // own (and the modifiers will be applied in the
            // emulated BBC).
            beeb_syms = keymap->GetValuesForPCKey(pc_key & ~PCKeyModifier_All);
        }

        if (!beeb_syms) {
            return false;
        }

        for (const int8_t *beeb_sym = beeb_syms; *beeb_sym >= 0; ++beeb_sym) {
            if (state) {
                m_beeb_keysyms_by_keycode[pc_key].insert((BeebKeySym)*beeb_sym);
                m_beeb_thread->Send(std::make_shared<BeebThread::KeySymMessage>((BeebKeySym)*beeb_sym, state));
            }
        }
    } else {
        const int8_t *beeb_keys = keymap->GetValuesForPCKey(keysym.scancode);
        if (!beeb_keys) {
            return false;
        }

        for (const int8_t *beeb_key = beeb_keys; *beeb_key >= 0; ++beeb_key) {
            m_beeb_thread->Send(std::make_shared<BeebThread::KeyMessage>((BeebKey)*beeb_key, state));
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

void BeebWindow::HandleSDLMouseWheelEvent(const SDL_MouseWheelEvent &event) {
    if (m_imgui_stuff) {
#if SDL_COMPILEDVERSION < SDL_VERSIONNUM(2, 0, 18)
        m_imgui_stuff->AddMouseWheelEvent((float)event.x, (float)event.y);
#else
        m_imgui_stuff->AddMouseWheelEvent(event.preciseX, event.preciseY);
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event) {
    if (m_is_mouse_captured) {
        if (event.xrel != 0 || event.yrel != 0) {
            m_beeb_thread->Send(std::make_shared<BeebThread::MouseMotionMessage>(event.xrel, event.yrel));
        }
    } else if (m_imgui_stuff) {
        m_imgui_stuff->AddMouseMotionEvent(event.x, event.y);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLTextInput(const char *text) {
    m_imgui_stuff->AddInputCharactersUTF8(text);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class FileMenuItem {
  public:
    // set if disk image should be loaded.
    bool load = false;

    // if non-empty, an item was selected.
    std::string path;

    // details of the disk type, if the new disk option was chosen.
    const Disc *new_disc_type = nullptr;
    std::vector<uint8_t> new_disc_data;

    explicit FileMenuItem(SelectorDialog *new_dialog,
                          SelectorDialog *open_dialog,
                          const char *new_title,
                          const char *open_title,
                          const char *recent_title,
                          Messages *msgs) {
        //bool recent_enabled=true;

        ImGuiIDPusher id_pusher(open_title);

        if (ImGui::MenuItem(open_title)) {
            if (open_dialog->Open(&this->path)) {
                m_used_dialog = open_dialog;
                this->load = true;
            }
        }

        if (ImGui::BeginMenu(new_title)) {
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

        if (ImGuiRecentMenu(&this->path, recent_title, *open_dialog)) {
            this->load = true;
        }
    }

    void Success() {
        if (m_used_dialog) {
            m_used_dialog->AddLastPathToRecentPaths();
        }
    }

  protected:
  private:
    SelectorDialog *m_used_dialog = nullptr;

    void DoBlankDiscsMenu(SelectorDialog *dialog,
                          const Disc *discs,
                          size_t num_discs,
                          Messages *msgs) {
        for (size_t i = 0; i < num_discs; ++i) {
            const Disc *disc = &discs[i];

            if (ImGui::MenuItem(disc->name.c_str())) {
                std::string src_path = disc->GetAssetPath();

                if (!LoadFile(&this->new_disc_data, src_path, msgs, 0)) {
                    return;
                }

                if (dialog->Open(&this->path)) {
                    this->new_disc_type = disc;
                    m_used_dialog = dialog;
                    this->load = true;
                }
            }
        }
    }
};

static size_t CleanUpRecentPaths(const std::string &tag, bool (*exists_fn)(const std::string &)) {
    size_t n = 0;

    if (RecentPaths *rp = GetRecentPathsByTag(tag)) {
        size_t i = 0;

        while (i < rp->GetNumPaths()) {
            if ((*exists_fn)(rp->GetPathByIndex(i))) {
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

// True if this keyboard event probably represents an interesting text input
// event for Dear ImGui purposes. (Such events should not be treated as shortcut
// key presses if a widget is currently wanting text input. Let the widget have
// the event.)
//
// (This is a bit of a hack! It'd be nice to have the input systems properly
// unified, but the Dear ImGui side doesn't quite support everything that b2
// would want.)
static bool IsProbablyInterestingTextInputEvent(const SDL_KeyboardEvent &event) {
    switch (event.keysym.sym) {
    default:
        return true;

    case SDLK_F1:
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
    case SDLK_F5:
    case SDLK_F6:
    case SDLK_F7:
    case SDLK_F8:
    case SDLK_F9:
    case SDLK_F10:
    case SDLK_F11:
    case SDLK_F12:
    case SDLK_F13:
    case SDLK_F14:
    case SDLK_F15:
    case SDLK_F16:
    case SDLK_F17:
    case SDLK_F18:
    case SDLK_F19:
    case SDLK_F20:
    case SDLK_F21:
    case SDLK_F22:
    case SDLK_F23:
    case SDLK_F24:
        return false;
    }
}

bool BeebWindow::DoImGui(uint64_t ticks) {
    int output_width, output_height;
    SDL_GetRendererOutputSize(m_renderer, &output_width, &output_height);

    SettingsUI *active_popup = nullptr;

    // Set if the BBC display panel has focus. This isn't entirely regular,
    // because the BBC display panel is handled by separate code - this will
    // probably get fixed eventually.
    //
    // The BBC display panel never has any dear imgui text widgets in it.
    bool beeb_focus = false;

    this->DoMenuUI();

    bool close_window = false;
    this->DoCommands(&close_window);

    ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGuiDockNodeFlags dock_space_flags = (ImGuiDockNodeFlags_PassthruCentralNode |
                                           ImGuiDockNodeFlags_NoDockingOverCentralNode);
    ImGuiID dock_space_id = ImGui::DockSpaceOverViewport(viewport, dock_space_flags);

    if (ImGuiDockNode *central_node = ImGui::DockBuilderGetCentralNode(dock_space_id)) {
        ImGui::SetNextWindowPos(central_node->Pos);
        ImGui::SetNextWindowSize(central_node->Size);

        beeb_focus = this->DoBeebDisplayUI();

        this->DoPopupUI(ticks, output_width, output_height);
    }

#if ENABLE_IMGUI_DEMO
    if (m_imgui_demo) {
        ImGui::ShowDemoWindow();
    }
#endif

#if STORE_DRAWLISTS
    if (m_imgui_drawlists) {
        m_imgui_stuff->DoStoredDrawListWindow();
    }
#endif

    if (m_imgui_metrics) {
        ImGui::ShowMetricsWindow();
    }

    active_popup = this->DoSettingsUI();

    // Handle input as appropriate.
    //
    // TODO - could the command key stuff use dear imgui functionality instead?
    {
        const ImGuiIO &io = ImGui::GetIO();

        for (const SDL_KeyboardEvent &event : m_sdl_keyboard_events) {
            uint32_t keycode = 0;
            bool state = false;
            bool is_valid_shortcut = true;
            if (event.type == SDL_KEYDOWN) {
                keycode = (uint32_t)event.keysym.sym | GetPCKeyModifiersFromSDLKeymod(event.keysym.mod);
                state = true;

                if (io.WantTextInput) {
                    // In this case, only allow function key shortcuts though.
                    // Whatever is wanting the text input will probably
                    // want everything else.
                    is_valid_shortcut = !IsProbablyInterestingTextInputEvent(event);
                }
            }

            const std::vector<Command2 *> *beeb_window_commands = nullptr;
            const std::vector<Command2 *> *active_popup_commands = nullptr;
            if (keycode != 0) {
                if (is_valid_shortcut) {
                    beeb_window_commands = g_beeb_window_command_table.GetCommandsForPCKey(keycode);

                    if (active_popup) {
                        if (active_popup->command_table) {
                            active_popup_commands = active_popup->command_table->GetCommandsForPCKey(keycode);
                        }
                    }
                }
            }

            if (beeb_focus) {
                // 1. Handle always-prioritized shortcuts, or all shortcuts if
                // all shortcuts prioritized
                //
                // 2. Handle Beeb window
                //
                // 3. Handle shortcuts, if shortcuts not prioritized
                //
                // No need to skip always-prioritized commands in step 3, as
                // there's no harm in actioning a command twice.

                bool handled = false;

                if (beeb_window_commands) {
                    for (Command2 *command : *beeb_window_commands) {
                        if (m_settings.prefer_shortcuts || command->IsAlwaysPrioritized()) {
                            m_cst.ActionCommand(command);
                            handled = true;
                        }
                    }
                }

                if (active_popup_commands) {
                    for (Command2 *command : *active_popup_commands) {
                        if (m_settings.prefer_shortcuts || command->IsAlwaysPrioritized()) {
                            active_popup->cst->ActionCommand(command);
                            handled = true;
                        }
                    }
                }

                if (!handled) {
                    handled = this->HandleBeebKey(event.keysym, state);
                }

                if (!m_settings.prefer_shortcuts) {
                    m_cst.ActionCommands(beeb_window_commands);
                    if (active_popup_commands) {
                        active_popup->cst->ActionCommands(active_popup_commands);
                    }
                }
            } else {
                m_cst.ActionCommands(beeb_window_commands);

                if (active_popup_commands) {
                    active_popup->cst->ActionCommands(active_popup_commands);
                }
            }
        }
    }

    m_sdl_keyboard_events.clear();

    return !close_window; // sigh, inverted logic
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BeebConfig *FindBeebConfigByName(const std::string &name) {
    for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
        BeebConfig *config = BeebWindows::GetConfigByIndex(i);
        if (config->name == name) {
            return config;
        }
    }

    return nullptr;
}

void BeebWindow::DoCopyModeCommands(BeebWindowSettings::CopySettings *settings,
                                    bool enabled,
                                    const Command2 &pass_through,
                                    const Command2 &only_gbp,
                                    const Command2 &SAA5050,
                                    const Command2 &toggle_handle_delete) {
    m_cst.SetTicked(pass_through, settings->convert_mode == BBCUTF8ConvertMode_PassThrough);
    m_cst.SetTicked(only_gbp, settings->convert_mode == BBCUTF8ConvertMode_OnlyGBP);
    m_cst.SetTicked(SAA5050, settings->convert_mode == BBCUTF8ConvertMode_SAA5050);
    m_cst.SetTicked(toggle_handle_delete, settings->handle_delete);

    m_cst.SetEnabled(pass_through, enabled);
    m_cst.SetEnabled(only_gbp, enabled);
    m_cst.SetEnabled(SAA5050, enabled);
    m_cst.SetEnabled(toggle_handle_delete, enabled);

    if (m_cst.WasActioned(pass_through)) {
        settings->convert_mode = BBCUTF8ConvertMode_PassThrough;
    }

    if (m_cst.WasActioned(only_gbp)) {
        settings->convert_mode = BBCUTF8ConvertMode_OnlyGBP;
    }

    if (m_cst.WasActioned(SAA5050)) {
        settings->convert_mode = BBCUTF8ConvertMode_SAA5050;
    }

    if (m_cst.WasActioned(toggle_handle_delete)) {
        settings->handle_delete = !settings->handle_delete;
    }
}

void BeebWindow::DoCommands(bool *close_window) {
    if (m_cst.WasActioned(g_hard_reset_command)) {
        this->HardReset();
    }

    bool can_clone = m_beeb_thread->GetBBCMicroCloneImpediments() == 0;

    m_cst.SetEnabled(g_save_state_command, can_clone);
    if (m_cst.WasActioned(g_save_state_command)) {
        m_beeb_thread->Send(std::make_shared<BeebThread::SaveStateMessage>(true));
    }

#if SYSTEM_WINDOWS
    m_cst.SetEnabled(g_toggle_console_command, CanDetachFromWindowsConsole());
    m_cst.SetTicked(g_toggle_console_command, HasWindowsConsole());
    if (m_cst.WasActioned(g_toggle_console_command)) {
        if (HasWindowsConsole()) {
            FreeWindowsConsole();
        } else {
            AllocWindowsConsole();
        }
    }

    if (m_cst.WasActioned(g_clear_console_command)) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(h, &csbi)) {
            COORD coord = {0, 0};
            DWORD num_chars = csbi.dwSize.X * csbi.dwSize.Y, num_written;
            FillConsoleOutputAttribute(h, csbi.wAttributes, num_chars, coord, &num_written);
            FillConsoleOutputCharacter(h, ' ', num_chars, coord, &num_written);
            SetConsoleCursorPosition(h, coord);
        }
    }

    if (m_cst.WasActioned(g_print_separator_command)) {
        printf("--------------------------------------------------\n");
    }
#endif

    if (m_cst.WasActioned(g_exit_command)) {
        this->SaveSettings();

        SDL_Event event = {};
        event.type = SDL_QUIT;

        SDL_PushEvent(&event);
    }

    if (m_cst.WasActioned(g_clean_up_recent_files_lists_command)) {
        size_t n = 0;

        n += CleanUpRecentPaths(RECENT_PATHS_DISC_IMAGE, &PathIsFileOnDisk);
        n += CleanUpRecentPaths(RECENT_PATHS_NVRAM, &PathIsFileOnDisk);

        if (n > 0) {
            m_msg.i.f("Removed %zu items\n", n);
        }
    }

    if (m_cst.WasActioned(g_reset_dock_windows_command)) {
        //m_imgui_stuff->ResetDockContext();
    }

    m_cst.SetTicked(g_paste_command, m_beeb_thread->IsPasting());
    if (m_cst.WasActioned(g_paste_command)) {
        this->DoPaste(false);
    }

    m_cst.SetTicked(g_paste_return_command, m_cst.GetTicked(g_paste_command));
    if (m_cst.WasActioned(g_paste_return_command)) {
        this->DoPaste(true);
    }

    const bool is_copying = m_beeb_thread->IsCopying();
    m_cst.SetTicked(g_toggle_copy_oswrch_text_command, is_copying);
    if (m_cst.WasActioned(g_toggle_copy_oswrch_text_command)) {
        if (m_beeb_thread->IsCopying()) {
            m_beeb_thread->Send(std::make_shared<BeebThread::StopCopyMessage>());
        } else {
            m_beeb_thread->Send(std::make_shared<BeebThread::StartCopyMessage>([this](std::vector<uint8_t> data) {
                this->SetClipboardFromBBCASCII(data, m_settings.text_copy_settings);
            },
                                                                               false)); //false=not Copy BASIC
        }
    }

    DoCopyModeCommands(&m_settings.text_copy_settings,
                       !is_copying,
                       g_copy_translation_pass_through,
                       g_copy_translation_only_gbp,
                       g_copy_translation_SAA5050,
                       g_copy_toggle_handle_delete);

    m_cst.SetEnabled(g_copy_basic_command, !m_beeb_thread->IsPasting());
    if (m_cst.WasActioned(g_copy_basic_command)) {
        if (m_beeb_thread->IsCopying()) {
            m_beeb_thread->Send(std::make_shared<BeebThread::StopCopyMessage>());
        } else {
            m_beeb_thread->Send(std::make_shared<BeebThread::StartCopyMessage>([this](std::vector<uint8_t> data) {
                this->SetClipboardFromBBCASCII(data, m_settings.text_copy_settings);
            },
                                                                               true)); //true=Copy BASIC
        }
    }

    m_cst.SetTicked(g_parallel_printer_command, m_beeb_thread->IsParallelPrinterEnabled());
    if (m_cst.WasActioned(g_parallel_printer_command)) {
        m_beeb_thread->Send(std::make_shared<BeebThread::SetPrinterEnabledMessage>(!m_cst.GetTicked(g_parallel_printer_command)));
    }

    const bool any_printer_data = m_beeb_thread->GetPrinterDataSizeBytes() > 0;
    m_cst.SetEnabled(g_reset_printer_buffer_command, any_printer_data);
    if (m_cst.WasActioned(g_reset_printer_buffer_command)) {
        m_beeb_thread->Send(std::make_shared<BeebThread::ResetPrinterBufferMessage>());
    }

    m_cst.SetEnabled(g_copy_printer_buffer_command, any_printer_data);
    if (m_cst.WasActioned(g_copy_printer_buffer_command)) {
        std::vector<uint8_t> data = m_beeb_thread->GetPrinterData();

        this->SetClipboardFromBBCASCII(data, m_settings.printer_copy_settings);
    }

    m_cst.SetEnabled(g_save_printer_buffer_command, any_printer_data);
    if (m_cst.WasActioned(g_save_printer_buffer_command)) {
        std::vector<uint8_t> data = m_beeb_thread->GetPrinterData();

        SaveFileDialog fd(RECENT_PATHS_PRINTER);

        fd.AddFilter("Data", {".dat"});

        std::string path;
        if (fd.Open(&path)) {
            SaveFile(data, path, &m_msg);
        }
    }

    DoCopyModeCommands(&m_settings.printer_copy_settings,
                       true, //no reason not to have these permanently enabled?
                       g_printer_translation_pass_through,
                       g_printer_translation_only_gbp,
                       g_printer_translation_SAA5050,
                       g_printer_toggle_handle_delete);

#if BBCMICRO_DEBUGGER
    m_cst.SetEnabled(g_debug_run_command, this->DebugIsHalted());
    if (m_cst.WasActioned(g_debug_run_command)) {
        m_beeb_thread->Send(std::make_shared<BeebThread::CallbackMessage>([](BBCMicro *m) -> void {
            m->DebugRun();
        }));
    }
#endif

#if BBCMICRO_DEBUGGER
    m_cst.SetEnabled(g_debug_stop_command, !m_cst.GetEnabled(g_debug_run_command));
    if (m_cst.WasActioned(g_debug_stop_command)) {
        m_beeb_thread->Send(std::make_shared<BeebThread::CallbackMessage>([](BBCMicro *m) -> void {
            m->DebugHalt("manual stop");
        }));
    }
#endif

    m_cst.SetEnabled(g_save_default_nvram_command, m_beeb_thread->HasNVRAM());
    if (m_cst.WasActioned(g_save_default_nvram_command)) {
        if (BeebConfig *config = FindBeebConfigByName(this->GetConfigName())) {
            config->nvram = m_beeb_thread->GetNVRAM();
        }
    }

    m_cst.SetEnabled(g_reset_default_nvram_command, m_cst.GetEnabled(g_save_default_nvram_command));
    if (m_cst.WasActioned(g_reset_default_nvram_command)) {
        if (BeebConfig *config = FindBeebConfigByName(this->GetConfigName())) {
            config->ResetNVRAM();
        }
    }

    if (m_cst.WasActioned(g_save_config_command)) {
        this->SaveConfig();
    }

    m_cst.SetTicked(g_toggle_prioritize_shortcuts_command, m_settings.prefer_shortcuts);
    if (m_cst.WasActioned(g_toggle_prioritize_shortcuts_command)) {
        m_settings.prefer_shortcuts = !m_settings.prefer_shortcuts;

        this->ShowPrioritizeCommandShortcutsStatus();
    }

    if (m_cst.WasActioned(g_save_screenshot_command)) {
        SaveFileDialog fd(RECENT_PATHS_SCREENSHOT);

        fd.AddFilter("PNG", {".png"});

        std::string path;
        if (fd.Open(&path)) {
            SDLUniquePtr<SDL_Surface> screenshot = this->CreateScreenshot(SDL_PIXELFORMAT_RGB24);
            if (!!screenshot) {
                SaveSDLSurface(screenshot.get(), path, &m_msg);
            }
        }
    }

    if (m_cst.WasActioned(g_copy_screenshot_command)) {
        // TODO this constant should be in platform-specific code.
#if SYSTEM_WINDOWS
        const SDL_PixelFormatEnum ideal_clipboard_format = SDL_PIXELFORMAT_XRGB8888;
#else
        const SDL_PixelFormatEnum ideal_clipboard_format = SDL_PIXELFORMAT_RGB24;
#endif
        SDLUniquePtr<SDL_Surface> screenshot = this->CreateScreenshot(ideal_clipboard_format);
        if (!!screenshot) {
            SetClipboardImage(screenshot.get(), &m_msg);
        }
    }

#if ENABLE_SDL_FULL_SCREEN
    m_cst.SetTicked(g_toggle_full_screen_command, this->IsWindowFullScreen());
    if (m_cst.WasActioned(g_toggle_full_screen_command)) {
        bool is_full_screen = this->IsWindowFullScreen();
        this->SetWindowFullScreen(!is_full_screen);
    }
#endif

    if (m_cst.WasActioned(g_new_window_command)) {
        PushNewWindowMessage(this->GetNewWindowInitArguments());
    }

    m_cst.SetEnabled(g_clone_window_command, can_clone);
    if (m_cst.WasActioned(g_clone_window_command)) {
        BeebWindowInitArguments init_arguments = this->GetNewWindowInitArguments();

        if (m_settings.keymap) {
            init_arguments.keymap_name = m_settings.keymap->GetName();
        }

        init_arguments.settings = m_settings;
        init_arguments.use_settings = true;

        m_beeb_thread->Send(std::make_shared<BeebThread::CloneWindowMessage>(init_arguments));
    }

    if (m_cst.WasActioned(g_close_window_command)) {
        *close_window = true;
    }

    m_cst.SetEnabled(g_toggle_capture_mouse_command, m_beeb_thread->HasMouse());
    m_cst.SetTicked(g_toggle_capture_mouse_command, m_is_mouse_captured);
    if (m_cst.WasActioned(g_toggle_capture_mouse_command)) {
        this->SetCaptureMouse(!m_is_mouse_captured);
    }

    m_cst.SetEnabled(g_toggle_capture_mouse_on_click_command, m_beeb_thread->HasMouse());
    m_cst.SetTicked(g_toggle_capture_mouse_on_click_command, m_settings.capture_mouse_on_click);
    if (m_cst.WasActioned(g_toggle_capture_mouse_on_click_command)) {
        m_settings.capture_mouse_on_click = !m_settings.capture_mouse_on_click;
    }

    for (int type = 0; type < BeebWindowPopupType_MaxValue; ++type) {
        const uint64_t mask = (uint64_t)1 << type;

        PopupMetadata *popup_metadata = &g_popups[type];

        if (m_cst.WasActioned(popup_metadata->command)) {
            m_settings.popups ^= mask;
        }
        m_cst.SetTicked(popup_metadata->command, !!(m_settings.popups & mask));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoMenuUI() {
    if (ImGui::BeginMainMenuBar()) {
        this->DoFileMenu();
        this->DoEditMenu();
        this->DoHardwareMenu();
        this->DoKeyboardMenu();
        this->DoJoysticksMenu();
        this->DoMouseMenu();
        this->DoPrinterMenu();
        this->DoToolsMenu();
        this->DoDebugMenu();
        this->DoWindowMenu();
        ImGui::EndMainMenuBar();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SettingsUI *BeebWindow::DoSettingsUI() {
    SettingsUI *active_popup = nullptr;

    for (int type = 0; type < BeebWindowPopupType_MaxValue; ++type) {
        const uint64_t mask = (uint64_t)1 << type;

        PopupMetadata *popup_metadata = &g_popups[type];

        if (m_settings.popups & mask) {
            if (!m_popups[type]) {
                m_popups[type] = CreatePopup(*popup_metadata, this, m_imgui_stuff);

                if (m_popups[type]) {
                    m_popups[type]->SetName(popup_metadata->command.GetText());
                }
            }

            SettingsUI *popup = m_popups[type].get();

            //ImGui::SetNextDock(ImGuiDockSlot_None);
            //ImVec2 default_pos = ImVec2(10.f, 30.f);
            //ImVec2 default_size = ImGui::GetIO().DisplaySize * .4f;

            bool opened = true;
            ImGuiWindowFlags extra_flags = 0;
            if (popup) {
                extra_flags = (ImGuiWindowFlags)popup->GetExtraImGuiWindowFlags();
                ImGui::SetNextWindowSize(popup->GetDefaultSize(), ImGuiCond_FirstUseEver);
            }

            if (ImGui::Begin(popup_metadata->command.GetText().c_str(), &opened, extra_flags)) {
                m_settings.popups |= mask;

                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                    active_popup = popup;
                }

                if (popup) {
                    popup->DoImGui();
                } else {
                    ImGui::TextWrapped("This version of b2 does not support this type of window.");
                }
            }
            ImGui::End();

            if (!opened) {
                m_settings.popups &= ~mask;

                // Leave the deletion until the next frame -
                // references to its textures might still be queued up
                // in the dear imgui drawlists.
            }
        } else {
            if (m_popups[type]) {
                if (m_popups[type]->OnClose()) {
                    this->SaveConfig();
                }

                m_popups[type] = nullptr;
            }
        }
    }

    if (ValueChanged(&m_msg_last_num_errors_printed, m_message_list->GetNumErrorsPrinted())) {
        m_settings.popups |= 1 << BeebWindowPopupType_Messages;
    }

    return active_popup;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoPopupUI(uint64_t now, int output_width, int output_height) {
    (void)output_width;

    if (ValueChanged(&m_msg_last_num_messages_printed, m_message_list->GetNumMessagesPrinted())) {
        m_messages_popup_ui_active = true;
        m_messages_popup_ticks = now;
    }

    if (m_messages_popup_ui_active) {
        ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar |
                                  //ImGuiWindowFlags_ShowBorders|
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

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

        if (ImGui::Begin("Recent Messages", &m_messages_popup_ui_active, flags)) {
            m_message_list->ForEachMessage(15, [](MessageList::Message *m) {
                if (!m->seen) {
                    ImGuiMessageListMessage(m);
                }
            });
        }
        ImGui::End();

        if (GetSecondsFromTicks(now - m_messages_popup_ticks) > MESSAGES_POPUP_TIME_SECONDS) {
            m_message_list->ForEachMessage([](MessageList::Message *m) {
                m->seen = true;
            });

            m_messages_popup_ui_active = false;
        }
    }

    BeebThreadTimelineState timeline_state;
    m_beeb_thread->GetTimelineState(&timeline_state);

    bool show_leds_popup = false;

    bool pasting = m_beeb_thread->IsPasting();
    bool copying = m_beeb_thread->IsCopying();
    if (ValueChanged(&m_leds, m_beeb_thread->GetLEDs()) || (m_leds & BBCMicroLEDFlags_AllDrives)) {
        if (m_settings.leds_popup_mode != BeebWindowLEDsPopupMode_Off) {
            show_leds_popup = true;
        }
    } else if (timeline_state.mode != BeebThreadTimelineMode_None || copying || pasting) {
        // The LEDs window always appears in this situation.
        show_leds_popup = true;
    } else if (m_settings.leds_popup_mode == BeebWindowLEDsPopupMode_On) {
        show_leds_popup = true;
    }

    if (show_leds_popup) {
        m_leds_popup_ticks = now;
        m_leds_popup_ui_active = true;
    }

    if (m_leds_popup_ui_active) {
        ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar |
                                  //ImGuiWindowFlags_ShowBorders|
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::SetNextWindowPos(ImVec2(10.f, output_height - 50.f));

        if (ImGui::Begin("LEDs", &m_leds_popup_ui_active, flags)) {
            ImGuiStyleColourPusher colour_pusher;
            colour_pusher.Push(ImGuiCol_CheckMark, ImVec4(1.f, 0.f, 0.f, 1.f));

            ImGuiLED(!!(m_leds & BBCMicroLEDFlag_CapsLock), "Caps Lock");

            ImGui::SameLine();
            ImGuiLED(!!(m_leds & BBCMicroLEDFlag_ShiftLock), "Shift Lock");

            for (int i = 0; i < NUM_DRIVES; ++i) {
                ImGui::SameLine();
                ImGuiLEDf(!!(m_leds & (uint32_t)BBCMicroLEDFlag_Drive0 << i), "Drive %d", i);
            }

            colour_pusher.Push(ImGuiCol_CheckMark, ImVec4(0.f, 1.f, 0.f, 1.f));

            switch (timeline_state.mode) {
            case BeebThreadTimelineMode_None:
                ImGuiLED(false, "Replay");
                break;

            case BeebThreadTimelineMode_Replay:
                ImGuiLED(true, "Replay");
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    m_beeb_thread->Send(std::make_shared<BeebThread::StopReplayMessage>());
                }
                break;

            case BeebThreadTimelineMode_Record:
                colour_pusher.Push(ImGuiCol_CheckMark, ImVec4(1.f, 0.f, 0.f, 1.f));
                ImGuiLED(true, "Record");
                ImGui::SameLine();
                if (ImGuiConfirmButton("Stop")) {
                    m_beeb_thread->Send(std::make_shared<BeebThread::StopRecordingMessage>());
                }
                colour_pusher.Pop();
                break;
            }

            ImGui::SameLine();
            ImGuiLED(copying, "Copy");
            if (copying) {
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    m_beeb_thread->Send(std::make_shared<BeebThread::StopCopyMessage>());
                }
            }

            ImGui::SameLine();
            ImGuiLED(pasting, "Paste");
            if (pasting) {
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    m_beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());
                }
            }
        }
        ImGui::End();

        if (GetSecondsFromTicks(now - m_leds_popup_ticks) > LEDS_POPUP_TIME_SECONDS) {
            if (m_settings.leds_popup_mode != BeebWindowLEDsPopupMode_On) {
                m_leds_popup_ui_active = false;
            }
        }
    }

    std::vector<std::shared_ptr<JobQueue::Job>> jobs = BeebWindows::GetJobs();
    if (!jobs.empty()) {
        bool open = false;

        for (const std::shared_ptr<JobQueue::Job> &job : jobs) {
            if (!job->HasImGui()) {
                continue;
            }

            if (open) {
                ImGui::Separator();
            } else {
                ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar |
                                          //ImGuiWindowFlags_ShowBorders|
                                          ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoFocusOnAppearing);

                ImGui::SetNextWindowPos(ImVec2(10.f, 30.f));

                open = true;

                if (!ImGui::Begin("Jobs", nullptr, flags)) {
                    goto jobs_imgui_done;
                }
            }

            job->DoImGui();

            if (ImGui::Button("Cancel")) {
                job->Cancel();
            }
        }

    jobs_imgui_done:
        if (open) {
            ImGui::End();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoFileMenu() {
    if (ImGui::BeginMenu("File")) {
        m_cst.DoMenuItem(g_hard_reset_command);

        if (ImGui::BeginMenu("Run")) {
            this->DoDiscImageSubMenu(0, true);

            ImGui::EndMenu();
        }

        ImGui::Separator();

        for (int drive = 0; drive < NUM_DRIVES; ++drive) {
            char title[100];
            snprintf(title, sizeof title, "Drive %d", drive);

            UniqueLock<Mutex> d_lock;
            std::shared_ptr<const DiscImage> disc_image = m_beeb_thread->GetDiscImage(&d_lock, drive);

            if (ImGui::BeginMenu(title)) {
                this->DoDiscDriveSubMenu(drive, disc_image);

                ImGui::EndMenu();
            }

            if (!!disc_image) {
                std::string name = disc_image->GetName();
                name = PathGetName(name);
                ImGui::MenuItem(name.c_str(), nullptr, false, false);
            }

            ImGui::Separator();
        }

        m_cst.DoMenuItem(g_save_default_nvram_command);

        ImGui::Separator();

        m_cst.DoMenuItem(g_save_state_command);

        ImGui::Separator();
        m_cst.DoMenuItem(g_save_config_command);
        m_cst.DoMenuItem(g_save_screenshot_command);
        ImGui::Separator();
        m_cst.DoMenuItem(g_exit_command);
        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoDiscDriveSubMenu(int drive,
                                    const std::shared_ptr<const DiscImage> &disc_image) {
    if (!!disc_image) {
        std::string name = disc_image->GetName();
        if (!name.empty()) {
            if (ImGui::BeginMenu("Full path")) {
                ImGui::MenuItem(name.c_str(), nullptr, false, false);

                if (ImGui::MenuItem("Copy path to clipboard")) {
                    SDL_SetClipboardText(name.c_str());
                }

                ImGui::EndMenu();
            }
        }

        std::string desc = disc_image->GetDescription();
        if (!desc.empty()) {
            ImGui::MenuItem(("Info: " + desc).c_str(), nullptr, false, false);
        }

        std::string hash = disc_image->GetHash();
        if (!hash.empty()) {
            ImGui::MenuItem(("SHA1: " + hash).c_str(), nullptr, false, false);
        }

        std::string load_method = disc_image->GetLoadMethod();
        ImGui::MenuItem(("Loaded from: " + load_method).c_str(), nullptr, false, false);

        bool disc_protected = disc_image->IsWriteProtected();

        if (disc_protected) {
            // Write protection state is shown, but can't be changed.
            ImGui::MenuItem("Write protect", nullptr, &disc_protected, false);
        } else {
            bool drive_protected = m_beeb_thread->IsDriveWriteProtected(drive);
            if (ImGui::MenuItem("Write protect", nullptr, &drive_protected)) {
                m_beeb_thread->Send(std::make_shared<BeebThread::SetDriveWriteProtectedMessage>(drive, drive_protected));
            }
        }

        if (ImGui::BeginMenu("Eject")) {
            if (ImGui::MenuItem("Confirm")) {
                m_beeb_thread->Send(std::make_shared<BeebThread::EjectDiscMessage>(drive));
            }
            ImGui::EndMenu();
        }

    } else {
        ImGui::MenuItem("(empty)", NULL, false, false);
    }

    ImGui::Separator();

    this->DoDiscImageSubMenu(drive, false);

    if (!!disc_image) {
        ImGui::Separator();

        if (disc_image->CanSave()) {
            if (ImGui::MenuItem("Save")) {
                disc_image->SaveToFile(disc_image->GetName(), m_msg);
            }
        }

        if (ImGui::MenuItem("Save copy as...")) {
            SaveFileDialog fd(RECENT_PATHS_DISC_IMAGE);

            disc_image->AddFileDialogFilter(&fd);
            fd.AddAllFilesFilter();

            std::string path;
            if (fd.Open(&path)) {
                if (disc_image->SaveToFile(path, m_msg)) {
                    fd.AddLastPathToRecentPaths();
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoDiscImageSubMenu(int drive, bool boot) {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);
    DriveState *d = &m_drives[drive];

    FileMenuItem direct_item(&d->new_direct_disc_image_file_dialog,
                             &d->open_direct_disc_image_file_dialog,
                             "New disc image",
                             "Disc image...",
                             "Recent disc image",
                             &m_msg);
    if (direct_item.load) {
        if (direct_item.new_disc_type) {
            if (!SaveFile(direct_item.new_disc_data,
                          direct_item.path,
                          &m_msg)) {
                return;
            }
        }

        std::shared_ptr<DirectDiscImage> new_disc_image = DirectDiscImage::CreateForFile(direct_item.path, &m_msg);

        this->DoDiscImageSubMenuItem(drive,
                                     std::move(new_disc_image),
                                     &direct_item, boot);
    }

    FileMenuItem file_item(&d->new_disc_image_file_dialog,
                           &d->open_disc_image_file_dialog,
                           "New in-memory disc image",
                           "In-memory disc image...",
                           "Recent in-memory disc image",
                           &m_msg);
    if (file_item.load) {
        std::shared_ptr<MemoryDiscImage> new_disc_image;
        if (file_item.new_disc_type) {
            new_disc_image = MemoryDiscImage::LoadFromBuffer(file_item.path,
                                                             MemoryDiscImage::LOAD_METHOD_FILE,
                                                             file_item.new_disc_data.data(),
                                                             file_item.new_disc_data.size(),
                                                             *file_item.new_disc_type->geometry,
                                                             &m_msg);
        } else {
            new_disc_image = MemoryDiscImage::LoadFromFile(file_item.path,
                                                           &m_msg);
        }
        this->DoDiscImageSubMenuItem(drive,
                                     std::move(new_disc_image),
                                     &file_item, boot);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoDiscImageSubMenuItem(int drive,
                                        std::shared_ptr<DiscImage> disc_image,
                                        FileMenuItem *item,
                                        bool boot) {
    if (!!disc_image) {
        m_beeb_thread->Send(std::make_shared<BeebThread::LoadDiscMessage>(drive,
                                                                          std::move(disc_image),
                                                                          true));
        if (boot) {
            m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Boot |
                                                                                              BeebThreadHardResetFlag_Run));
        }

        item->Success();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoEditMenu() {
    if (ImGui::BeginMenu("Edit")) {
        m_cst.DoMenuItem(g_toggle_copy_oswrch_text_command);
        m_cst.DoMenuItem(g_copy_basic_command);
        if (ImGui::BeginMenu("Copy options")) {
            m_cst.DoMenuItem(g_copy_translation_pass_through);
            m_cst.DoMenuItem(g_copy_translation_only_gbp);
            m_cst.DoMenuItem(g_copy_translation_SAA5050);
            ImGui::Separator();
            m_cst.DoMenuItem(g_copy_toggle_handle_delete);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        m_cst.DoMenuItem(g_copy_screenshot_command);
        ImGui::Separator();
        m_cst.DoMenuItem(g_paste_command);
        m_cst.DoMenuItem(g_paste_return_command);

        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoHardwareMenu() {
    if (ImGui::BeginMenu("Hardware")) {
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Configs].command);

        ImGui::Separator();

        std::string config_name = this->GetConfigName();

        for (size_t config_idx = 0; config_idx < BeebWindows::GetNumConfigs(); ++config_idx) {
            BeebConfig *config = BeebWindows::GetConfigByIndex(config_idx);

            if (ImGui::MenuItem(config->name.c_str(), nullptr, config->name == config_name)) {
                this->HardReset(*config);
            }
        }

        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoKeyboardMenu() {
    if (ImGui::BeginMenu("Keyboard")) {
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Keymaps].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_CommandKeymaps].command);

        ImGui::Separator();

        m_cst.DoMenuItem(g_toggle_prioritize_shortcuts_command); //.DoMenuItemUI("toggle_prioritize_shortcuts");

        ImGui::Separator();

        for (size_t i = 0; i < BeebWindows::GetNumBeebKeymaps(); ++i) {
            BeebKeymap *keymap = BeebWindows::GetBeebKeymapByIndex(i);

            if (ImGui::MenuItem(GetKeymapUIName(*keymap).c_str(),
                                nullptr,
                                m_settings.keymap == keymap)) {
                this->SetCurrentKeymap(keymap);
                m_msg.i.f("Keymap: %s\n", m_settings.keymap->GetName().c_str());
                this->ShowPrioritizeCommandShortcutsStatus();
            }
        }

        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoMouseMenu() {
    if (ImGui::BeginMenu("Mouse")) {
        m_cst.DoMenuItem(g_toggle_capture_mouse_command);
        m_cst.DoMenuItem(g_toggle_capture_mouse_on_click_command);
        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoJoysticksMenu() {
    if (ImGui::BeginMenu("Joysticks")) {
        DoJoysticksMenuImGui(&m_msg);
        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoPrinterMenu() {
    if (ImGui::BeginMenu("Printer")) {
        m_cst.DoMenuItem(g_parallel_printer_command);

        ImGui::Separator();

        char size_str[MAX_UINT64_THOUSANDS_SIZE];
        GetThousandsString(size_str, m_beeb_thread->GetPrinterDataSizeBytes());

        char label[100];
        snprintf(label, sizeof label, "Printer data: %s bytes", size_str);

        ImGui::MenuItem(label, nullptr, nullptr, false);

        ImGui::Separator();

        m_cst.DoMenuItem(g_reset_printer_buffer_command);

        m_cst.DoMenuItem(g_copy_printer_buffer_command);

        if (ImGui::BeginMenu("Copy options")) {
            m_cst.DoMenuItem(g_printer_translation_pass_through);
            m_cst.DoMenuItem(g_printer_translation_only_gbp);
            m_cst.DoMenuItem(g_printer_translation_SAA5050);
            ImGui::Separator();
            m_cst.DoMenuItem(g_printer_toggle_handle_delete);
            ImGui::EndMenu();
        }

        m_cst.DoMenuItem(g_save_printer_buffer_command);

        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoToolsMenu() {
    if (ImGui::BeginMenu("Tools")) {
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Options].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Messages].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Timeline].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SavedStates].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_BeebLink].command);

        ImGui::Separator();

        if (ImGui::BeginMenu("LEDs")) {
            ImGuiMenuItemEnumValue("Auto hide", nullptr, &m_settings.leds_popup_mode, BeebWindowLEDsPopupMode_Auto);
            ImGuiMenuItemEnumValue("Always on", nullptr, &m_settings.leds_popup_mode, BeebWindowLEDsPopupMode_On);
            ImGuiMenuItemEnumValue("Always off", nullptr, &m_settings.leds_popup_mode, BeebWindowLEDsPopupMode_Off);

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
        m_cst.DoMenuItem(g_reset_default_nvram_command);

        ImGui::Separator();
        m_cst.DoMenuItem(g_clean_up_recent_files_lists_command);
        m_cst.DoMenuItem(g_reset_dock_windows_command);

        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoDebugMenu() {
#if ENABLE_DEBUG_MENU
    if (ImGui::BeginMenu("Debug")) {
#if ENABLE_IMGUI_TEST
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DearImguiTest].command);
#endif
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Trace].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_AudioCallback].command);

#if VIDEO_TRACK_METADATA
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_PixelMetadata].command);
#endif

#if BBCMICRO_DEBUGGER
        ImGui::Separator();

        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SystemDebug].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_6502Debugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Parasite6502Debugger].command);

        if (ImGui::BeginMenu("Memory Debug")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger1].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger2].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger3].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger4].command);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Parasite Memory Debug")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger1].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger2].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger3].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger4].command);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Disassembly Debug")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger1].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger2].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger3].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger4].command);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Parasite Disassembly Debug")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger1].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger2].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger3].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger4].command);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("External Memory Debug")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger1].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger2].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger3].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger4].command);
            ImGui::EndMenu();
        }
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_CRTCDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_VideoULADebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SystemVIADebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_UserVIADebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_NVRAMDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SN76489Debugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ADCDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_PagingDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_BreakpointsDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_StackDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteStackDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_TubeDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DigitalJoystickDebugger].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_KeyboardDebug].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MouseDebug].command);

        ImGui::Separator();

        m_cst.DoMenuItem(g_debug_stop_command);
        m_cst.DoMenuItem(g_debug_run_command);

#endif

        ImGui::Separator();

#if SYSTEM_WINDOWS
        m_cst.DoMenuItem(g_toggle_console_command);

        if (HasWindowsConsole()) {
            m_cst.DoMenuItem(g_clear_console_command);

            m_cst.DoMenuItem(g_print_separator_command);
        }

        ImGui::Separator();
#endif

#if ENABLE_IMGUI_DEMO
        ImGui::MenuItem("ImGui demo...", NULL, &m_imgui_demo);
#endif
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ImGuiDebug].command);
#if STORE_DRAWLISTS
        ImGui::MenuItem("ImGui drawlists...", nullptr, &m_imgui_drawlists);
#endif
        ImGui::MenuItem("ImGui metrics...", nullptr, &m_imgui_metrics);

        ImGui::EndMenu();
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoWindowMenu() {
    if (ImGui::BeginMenu("Window")) {
        {
            char name[100];
            strlcpy(name, m_name.c_str(), sizeof name);

            if (ImGui::InputText("Name", name, sizeof name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                BeebWindows::SetBeebWindowName(this, name);
            }
        }

#if ENABLE_SDL_FULL_SCREEN
        m_cst.DoMenuItem(g_toggle_full_screen_command); //.DoMenuItemUI("toggle_full_screen");
#endif

        ImGui::Separator();

        m_cst.DoMenuItem(g_new_window_command);
        m_cst.DoMenuItem(g_clone_window_command);
        ImGui::Separator();
        m_cst.DoMenuItem(g_close_window_command);

        ImGui::EndMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::UpdateTVTextureThread(UpdateTVTextureThreadState *state) {
    UniqueLock<Mutex> lock(state->mutex);

    while (!state->stop) {
        state->update_cv.wait(lock);

        if (state->update) {
            lock.unlock();

            {
                PROFILE_SCOPE(PROFILER_COLOUR_CHOCOLATE, "ConsumeTVTexture");

                state->update_num_units_consumed = ConsumeTVTexture(state->update_video_output,
                                                                    state->update_tv,
                                                                    state->update_inhibit);
            }

            {
                PROFILE_SCOPE(PROFILER_COLOUR_CORAL, "CopyTexturePixels");

                ASSERT(state->update_dest_pitch >= 0);
                state->update_tv->CopyTexturePixels(state->update_dest_pixels,
                                                    (size_t)state->update_dest_pitch);
            }

            lock.lock();

            state->done = true;

            state->done_cv.notify_one();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebWindow::ConsumeTVTexture(OutputDataBuffer<VideoDataUnit> *video_output, TVOutput *tv, bool inhibit_update) {
    //OutputDataBuffer<VideoDataUnit> *video_output=m_beeb_thread->GetVideoOutput();

    //uint64_t num_units=(uint64_t)(GetSecondsFromTicks(vblank_record->num_ticks)*1e6)*2;
    //uint64_t num_units_left=num_units;

    const VideoDataUnit *a, *b;
    size_t na, nb;

    size_t num_units_consumed = 0;

    bool update = true;
#if BBCMICRO_DEBUGGER
    if (inhibit_update) {
        update = false;
    }
#else
    (void)inhibit_update;
#endif

    if (video_output->GetConsumerBuffers(&a, &na, &b, &nb)) {
        if (!update) {
            // Discard...
            video_output->Consume(na + nb);
        } else {
            size_t num_left;
            const size_t MAX_UPDATE_SIZE = 200;

            tv->PrepareForUpdate();

            // A.
            num_left = na;
            while (num_left > 0) {
                size_t n = num_left;
                if (n > MAX_UPDATE_SIZE) {
                    n = MAX_UPDATE_SIZE;
                }

                tv->Update(a, n);

                a += n;
                video_output->Consume(n);
                num_left -= n;
            }

            // B.
            num_left = nb;
            while (num_left > 0) {
                size_t n = num_left;
                if (n > MAX_UPDATE_SIZE) {
                    n = MAX_UPDATE_SIZE;
                }

                tv->Update(b, n);

                b += n;
                video_output->Consume(n);
                num_left -= n;
            }
        }

        num_units_consumed += na + nb;
    }

    return num_units_consumed;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::InhibitUpdateTVTexture() const {
#if BBCMICRO_DEBUGGER

    if (m_test_pattern) {
        return true;
    }

#endif

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::BeginUpdateTVTexture(bool threaded, void *dest_pixels, int dest_pitch) {
    ASSERT(dest_pitch > 0);
    if (threaded) {
        {
            UniqueLock<Mutex> lock(m_update_tv_texture_state.mutex);
            m_update_tv_texture_state.done = false;
            m_update_tv_texture_state.update = true;
            m_update_tv_texture_state.update_video_output = m_beeb_thread->GetVideoOutput();
            m_update_tv_texture_state.update_tv = &m_tv;
            m_update_tv_texture_state.update_inhibit = this->InhibitUpdateTVTexture();
            m_update_tv_texture_state.update_dest_pixels = dest_pixels;
            m_update_tv_texture_state.update_dest_pitch = (size_t)dest_pitch;
        }
        m_update_tv_texture_state.update_cv.notify_one();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::EndUpdateTVTexture(bool threaded, VBlankRecord *vblank_record, void *dest_pixels, int dest_pitch) {
    Timer tmr(&g_HandleVBlank_UpdateTVTexture_Consume_timer_def);

    ASSERT(dest_pitch > 0);

    if (threaded) {
        UniqueLock<Mutex> lock(m_update_tv_texture_state.mutex);

        while (!m_update_tv_texture_state.done) {
            m_update_tv_texture_state.done_cv.wait(lock);
        }

        vblank_record->num_video_units = m_update_tv_texture_state.update_num_units_consumed;
    } else {
        m_tv.SetInterlace(m_settings.display_interlace);

        bool inhibit_update = this->InhibitUpdateTVTexture();
        size_t num_units_consumed = this->ConsumeTVTexture(m_beeb_thread->GetVideoOutput(),
                                                           &m_tv,
                                                           inhibit_update);

        vblank_record->num_video_units = num_units_consumed;

        m_tv.CopyTexturePixels(dest_pixels, (size_t)dest_pitch);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::VBlankRecord *BeebWindow::NewVBlankRecord(uint64_t ticks) {
    VBlankRecord *vblank_record;

    if (m_vblank_records.size() < NUM_VBLANK_RECORDS) {
        m_vblank_records.emplace_back();
        vblank_record = &m_vblank_records.back();
    } else {
        vblank_record = &m_vblank_records[m_vblank_index];
        m_vblank_index = (m_vblank_index + 1) % NUM_VBLANK_RECORDS;
    }

    vblank_record->num_ticks = ticks - m_last_vblank_ticks;
    m_last_vblank_ticks = ticks;

    return vblank_record;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::DoBeebDisplayUI() {
    //bool opened=m_imgui_stuff->AreAnyDocksDocked();
    bool focus = false;

    double scale_x;
    if (m_settings.correct_aspect_ratio) {
        scale_x = CORRECT_ASPECT_RATIO_X_SCALE;
    } else {
        scale_x = 1;
    }

    //ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar;
    ImGuiWindowFlags flags = (ImGuiWindowFlags_NoDocking |
                              ImGuiWindowFlags_NoTitleBar |
                              ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoBringToFrontOnFocus |
                              ImGuiWindowFlags_NoNavFocus |
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 pos;
    ImVec2 size;
    if (!m_settings.display_auto_scale) {
        pos.x = 0.f;
        pos.y = 0.f;

        size.x = (float)(m_settings.display_manual_scale * TV_TEXTURE_WIDTH * scale_x);
        size.y = (float)(m_settings.display_manual_scale * TV_TEXTURE_HEIGHT);

        flags |= ImGuiWindowFlags_HorizontalScrollbar;

        ImGui::SetNextWindowContentSize(size);

        flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    if (ImGui::Begin("Display", nullptr, flags)) {

        if (ImGui::IsWindowAppearing()) {
            ImGui::FocusWindow(GImGui->CurrentWindow);
        }

        ImVec2 padding = GImGui->Style.WindowPadding;

        focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        if (m_recreate_tv_texture) {
            this->RecreateTexture();
            m_recreate_tv_texture = false;
        }

        ImGuiStyleVarPusher vpusher(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        if (m_tv_texture) {
#if BBCMICRO_DEBUGGER
            if (m_display_fill) {
                pos = {0.f, 0.f};
                size = ImGui::GetWindowSize();
            } else //<--note
#endif             //<--note
            {      //<--note
                ImVec2 window_size = ImGui::GetWindowSize() - padding * 2.f;

                if (m_settings.display_auto_scale) {
                    double tv_aspect = (TV_TEXTURE_WIDTH * scale_x) / TV_TEXTURE_HEIGHT;

                    double width = window_size.x;
                    double height = width / tv_aspect;

                    if (height > window_size.y) {
                        height = window_size.y;
                        width = height * tv_aspect;
                    }

                    size.x = (float)width;
                    size.y = (float)height;

                    pos = (window_size - size) * .5f;

                    // Don't fight any half pixel offset.
                    pos.x = (float)(int)pos.x;
                    pos.y = (float)(int)pos.y;
                }
            }

            ImGui::SetCursorPos(pos);
            ImVec2 screen_pos = ImGui::GetCursorScreenPos();
            ImGui::Image(m_tv_texture, size);

            if (m_settings.capture_mouse_on_click) {
                if (ImGui::IsItemClicked()) {
                    this->SetCaptureMouse(true);
                }
            }

#if VIDEO_TRACK_METADATA

            m_got_mouse_pixel_unit = false;

            if (ImGui::IsItemHovered()) {
                ImVec2 mouse_pos = ImGui::GetMousePos();
                mouse_pos -= screen_pos;

                double tx = mouse_pos.x / size.x;
                double ty = mouse_pos.y / size.y;

                if (tx >= 0. && tx < 1. && ty >= 0. && ty < 1.) {
                    int x = (int)(tx * TV_TEXTURE_WIDTH);
                    int y = (int)(ty * TV_TEXTURE_HEIGHT);

                    ASSERT(x >= 0 && x < TV_TEXTURE_WIDTH);
                    ASSERT(y >= 0 && y < TV_TEXTURE_HEIGHT);

                    const VideoDataUnit *units = m_tv.GetTextureUnits();
                    m_mouse_pixel_unit = units[y * TV_TEXTURE_WIDTH + x];
                    m_got_mouse_pixel_unit = true;
                }
            }
#else
            (void)screen_pos;
#endif
        }
    }
    ImGui::End();

    return focus;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HandleVBlank(uint64_t ticks) {
    PROFILE_SCOPE(PROFILER_COLOUR_DEEP_PINK, "HandleVBlank");
    ImGuiContextSetter setter(m_imgui_stuff);

    Timer HandleVBlank_timer(&g_HandleVBlank_timer_def);

    bool keep_window = true;

    // don't use m_update_tv_texture_thread_enabled directly - it might change
    // during the DoImGui call.
    bool threaded_update = m_update_tv_texture_thread_enabled;

    {
        Timer tmr2(&g_HandleVBlank_start_of_frame_timer_def);

        //if (m_pushed_window_padding) {
        //    ImGui::PopStyleVar(1);
        //}

        //ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

        m_imgui_stuff->NewFrame();

        // Show/hide popup commands in the UI. Try to create each popup in turn,
        // and hide it if the attempt fails! A bit of a cheap hack, but it saves
        // on having to get the #ifs perfectly consistent.
        if (!g_popups_visibility_checked) {
            for (PopupMetadata &popup : g_popups) {
                popup.command.VisibleIf(!!CreatePopup(popup, this, m_imgui_stuff));
            }
            g_popups_visibility_checked = true;
        }

        //m_pushed_window_padding = true;
    }

    {
        Timer HandleVBlank_end_of_frame_timer(&g_HandleVBlank_end_of_frame_timer_def);

        VBlankRecord *vblank_record = this->NewVBlankRecord(ticks);

        void *dest_pixels = nullptr;
        int dest_pitch = 0;
        if (m_tv_texture) {
            SDL_LockTexture(m_tv_texture, nullptr, &dest_pixels, &dest_pitch);
        }

        this->BeginUpdateTVTexture(threaded_update, dest_pixels, dest_pitch);

        {
            PROFILE_SCOPE(PROFILER_COLOUR_INDIAN_RED, "DoImGui");
            Timer tmr3(&g_HandleVBlank_DoImGui_timer_def);

            if (!this->DoImGui(ticks)) {
                keep_window = false;
            }

            m_imgui_stuff->RenderImGui();
        }

        SDL_RenderClear(m_renderer);

        this->EndUpdateTVTexture(threaded_update, vblank_record, dest_pixels, dest_pitch);

        if (dest_pixels) {
            SDL_UnlockTexture(m_tv_texture);
        }

        //        {
        //            Timer tmr(&g_HandleVBlank_UpdateTVTexture_Copy_timer_def);
        //
        //            if(m_tv_texture) {
        //                void *dest_pixels;
        //                int dest_pitch;
        //                if(SDL_LockTexture(m_tv_texture,nullptr,&dest_pixels,&dest_pitch)==0) {
        //                    m_tv.CopyTexturePixels(dest_pixels,(size_t)dest_pitch);
        //                    SDL_UnlockTexture(m_tv_texture);
        //                }
        //            }
        //        }

        {
            PROFILE_SCOPE(PROFILER_COLOUR_LIGHT_GREEN, "RenderSDL");
            Timer HandleVBlank_RenderSDL_timer(&g_HandleVBlank_RenderSDL_timer_def);

            m_imgui_stuff->RenderSDL();

            SDL_RenderPresent(m_renderer);
        }
    }

    return keep_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::Init() {
    bool good = this->InitInternal();

    if (good) {
        // Insert pre-init messages in their proper place. Then discard
        // them - there's no point keeping them around.
        if (m_init_arguments.preinit_message_list) {
            m_message_list->InsertMessages(*m_init_arguments.preinit_message_list);

            m_init_arguments.preinit_message_list = nullptr;
        }

        return true;
    } else {
        std::shared_ptr<MessageList> msg = MessageList::stdio;

        if (m_init_arguments.preinit_message_list) {
            msg = m_init_arguments.preinit_message_list;
        } else if (m_init_arguments.initiating_window_id != 0) {
            if (BeebWindow *initiating_window = BeebWindows::FindBeebWindowBySDLWindowID(m_init_arguments.initiating_window_id)) {
                msg = initiating_window->GetMessageList();
            }
        }

        msg->InsertMessages(*m_message_list);

        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SaveSettings() {
#if ENABLE_SDL_FULL_SCREEN
    m_settings.full_screen = this->IsWindowFullScreen();
#endif

    BeebWindows::defaults = m_settings;
    BeebWindows::default_config_name = m_init_arguments.default_config.config.name;

    this->SavePosition();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SavePosition() {
#if SYSTEM_WINDOWS

    uint32_t flags = SDL_GetWindowFlags(m_window);
    if (flags & SDL_WINDOW_FULLSCREEN) {
        // Don't overwrite any previously saved size.
    } else {
        if (m_hwnd) {
            std::vector<uint8_t> placement_data;
            placement_data.resize(sizeof(WINDOWPLACEMENT));

            auto wp = (WINDOWPLACEMENT *)placement_data.data();
            memset(wp, 0, sizeof *wp);
            wp->length = sizeof *wp;

            if (GetWindowPlacement((HWND)m_hwnd, wp)) {
                //LOGF(OUTPUT, "%s: flags=0x%x showCmd=%u MinPosition=(%ld,%ld) MaxPosition=(%ld,%ld) NormalPosition=(%ld,%ld)-(%ld,%ld) (%ldx%ld)\n",
                //     __func__,
                //     wp->flags,
                //     wp->showCmd,
                //     wp->ptMinPosition.x, wp->ptMinPosition.y,
                //     wp->ptMaxPosition.x, wp->ptMaxPosition.y,
                //     wp->rcNormalPosition.left, wp->rcNormalPosition.top, wp->rcNormalPosition.right, wp->rcNormalPosition.bottom,
                //     wp->rcNormalPosition.right - wp->rcNormalPosition.left, wp->rcNormalPosition.bottom - wp->rcNormalPosition.top);

                BeebWindows::SetLastWindowPlacementData(std::move(placement_data));
            }
        }
    }

#elif SYSTEM_OSX

    SaveCocoaFrameUsingName(m_nswindow, m_init_arguments.frame_name);

#else

    std::vector<uint8_t> buf = BeebWindows::GetLastWindowPlacementData();
    if (buf.size() != sizeof(WindowPlacementData)) {
        buf.clear();
        buf.resize(sizeof(WindowPlacementData));
        new (buf.data()) WindowPlacementData;
    }

    auto wp = (WindowPlacementData *)buf.data();

    uint32_t flags = SDL_GetWindowFlags(m_window);

    wp->maximized = !!(flags & SDL_WINDOW_MAXIMIZED);

    if (flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) {
        // Don't update the size in this case.
    } else {
        SDL_GetWindowPosition(m_window, &wp->x, &wp->y);
        SDL_GetWindowSize(m_window, &wp->width, &wp->height);
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

    m_sound_device = m_init_arguments.sound_device;
    ASSERT(m_init_arguments.sound_spec.freq > 0);

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
    m_window = SDL_CreateWindow("",
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                TV_TEXTURE_WIDTH + (int)(IMGUI_DEFAULT_STYLE.WindowPadding.x * 2.f),
                                TV_TEXTURE_HEIGHT + (int)(IMGUI_DEFAULT_STYLE.WindowPadding.y * 2.f),
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!m_window) {
        m_msg.e.f("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    m_renderer = SDL_CreateRenderer(m_window, -1, 0);
    if (!m_renderer) {
        m_msg.e.f("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowData(m_window, SDL_WINDOW_DATA_NAME, this);

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    SDL_GetWindowWMInfo(m_window, &wmi);

    bool reset_windows = m_init_arguments.reset_windows;
    m_init_arguments.reset_windows = false;

#if SYSTEM_WINDOWS

    m_hwnd = wmi.info.win.window;

    // 33 = window corner preference -
    // https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
    //
    // 1 = don't round -
    // https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_window_corner_preference
    uint32_t wcp = 1;
    DwmSetWindowAttribute((HWND)m_hwnd, 33, &wcp, sizeof wcp);

    if (!reset_windows) {
        if (m_hwnd) {
            if (m_init_arguments.placement_data.size() == sizeof(WINDOWPLACEMENT)) {
                auto wp = (const WINDOWPLACEMENT *)m_init_arguments.placement_data.data();

                SetWindowPlacement((HWND)m_hwnd, wp);
            }
        }
    }

#elif SYSTEM_OSX

    m_nswindow = wmi.info.cocoa.window;

    if (!reset_windows) {
        SetCocoaFrameUsingName(m_nswindow, m_init_arguments.frame_name);
    }

#else

    if (!reset_windows) {
        if (m_init_arguments.placement_data.size() == sizeof(WindowPlacementData)) {
            auto wp = (const WindowPlacementData *)m_init_arguments.placement_data.data();

            SDL_RestoreWindow(m_window);

            if (wp->x != INT_MIN && wp->y != INT_MIN) {
                SDL_SetWindowPosition(m_window, wp->x, wp->y);
            }

            if (wp->width > 0 && wp->height > 0) {
                SDL_SetWindowSize(m_window, wp->width, wp->height);
            }

            if (wp->maximized) {
                SDL_MaximizeWindow(m_window);
            }
        }
    }

#endif

#if ENABLE_SDL_FULL_SCREEN
    if (!reset_windows) {
        this->SetWindowFullScreen(m_settings.full_screen);
    }
#endif

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(m_renderer, &info) < 0) {
        m_msg.e.f("SDL_GetRendererInfo failed: %s\n", SDL_GetError());
        return false;
    }

#if RMT_ENABLED
    if (g_num_BeebWindow_inits == 0) {
#if RMT_USE_OPENGL
        if (strcmp(info.name, "opengl") == 0) {
            rmt_BindOpenGL();
            g_unbind_opengl = 1;
        }
#endif
    }
    ++g_num_BeebWindow_inits;
#endif

    if (!this->RecreateTexture()) {
        return false;
    }

    m_imgui_stuff = new ImGuiStuff(m_renderer);
    if (!m_imgui_stuff->Init(ImGuiConfigFlags_DockingEnable)) {
        m_msg.e.f("failed to initialise ImGui\n");
        return false;
    }

    m_imgui_stuff->SetFontSizePixels(m_settings.gui_font_size);

    if (!m_beeb_thread->Start()) {
        m_msg.e.f("Failed to start BBC\n"); //: %s",BeebThread_GetError(m_beeb_thread));
        return false;
    }

    if (!!m_init_arguments.initial_state) {
        // Load initial state.
        m_beeb_thread->Send(std::make_shared<BeebThread::LoadStateMessage>(m_init_arguments.initial_state,
                                                                           false));

        for (int i = 0; i < NUM_DRIVES; ++i) {
            ASSERT(!m_init_arguments.init_disc_images[i]);
            m_init_arguments.init_disc_images[i].reset();
        }

        ASSERT(!m_init_arguments.boot);
        m_init_arguments.boot = false;
    } else {
        m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(0, m_init_arguments.default_config));

        // If there were any discs mounted, or there's any booting needed,
        // another reboot will be necessary. This can't all be done with one
        // HardReset message, because until the first one there's no BBCMicro
        // object. <<rolleyes smiley>>
        uint32_t flags = 0;

        // Mount initial discs.
        for (int i = 0; i < NUM_DRIVES; ++i) {
            if (!!m_init_arguments.init_disc_images[i]) {
                auto message = std::make_shared<BeebThread::LoadDiscMessage>(i,
                                                                             std::move(m_init_arguments.init_disc_images[i]),
                                                                             true);
                m_beeb_thread->Send(std::move(message));
            }
        }

        if (m_init_arguments.boot) {
            flags |= BeebThreadHardResetFlag_Boot;
            m_init_arguments.boot = false;
        }

        if (flags != 0) {
            m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(flags, m_init_arguments.default_config));
        }
    }

    //    if(!m_init_arguments.initially_paused) {
    //        m_beeb_thread->Send(std::make_shared<BeebThread::PauseMessage>(false));
    //    }

    if (reset_windows) {
        // TODO...
    }

    m_display_size_options.push_back("Auto");

    for (size_t i = 1; i < 4; ++i) {
        char name[100];
        snprintf(name, sizeof name, "%zux (%zux%zu)", i, i * TV_TEXTURE_WIDTH, i * TV_TEXTURE_HEIGHT);

        m_display_size_options.push_back(name);
    }

    if (!m_init_arguments.keymap_name.empty()) {
        m_settings.keymap = BeebWindows::FindBeebKeymapByName(m_init_arguments.keymap_name);
    }

    if (!m_settings.keymap) {
        m_settings.keymap = BeebWindows::GetDefaultBeebKeymap();
    }

    if (SDL_GL_GetCurrentContext()) {
        if (SDL_GL_SetSwapInterval(0) != 0) {
            m_msg.i.f("failed to set GL swap interval to 0: %s\n", SDL_GetError());
        }
    }

    //if (!m_settings.dock_config.empty()) {
    //    if (!m_imgui_stuff->LoadDockContext(m_settings.dock_config)) {
    //        m_msg.w.f("failed to load dock config\n");
    //    }
    //}

    {
        Uint32 format;
        int width, height;
        SDL_QueryTexture(m_tv_texture, &format, nullptr, &width, &height);
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

    MUTEX_SET_NAME(m_update_tv_texture_state.mutex, "UpdateTVTextureMutex");
    m_update_tv_texture_thread = std::thread([this]() {
        SetCurrentThreadName("UpdateTVTextureThread");
        UpdateTVTextureThread(&m_update_tv_texture_state);
    });

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::ThreadFillAudioBuffer(SDL_AudioDeviceID audio_device_id, float *mix_buffer, size_t num_samples) {
    if (!m_beeb_thread->IsStarted()) {
        return;
    }

    if (m_sound_device != audio_device_id) {
        return;
    }

    m_beeb_thread->AudioThreadFillAudioBuffer(mix_buffer, num_samples, false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::UpdateTitle() {
    if (!m_beeb_thread->IsStarted()) {
        return;
    }

    char title[1000];

#if GOT_CRTDBG
    size_t malloc_bytes = 0, malloc_count = 0;
    {
        _CrtMemState mem_state;
        _CrtMemCheckpoint(&mem_state);

        for (int i = 0; i < _MAX_BLOCKS; ++i) {
            malloc_bytes += mem_state.lSizes[i];
            malloc_count += mem_state.lCounts[i];
        }
    }
#endif

    double speed = 0.0;
    {
        CycleCount num_cycles = m_beeb_thread->GetEmulatedCycles();
        CycleCount num_cycles_elapsed = {num_cycles.n - m_last_title_update_cycles.n};

        uint64_t now = GetCurrentTickCount();
        double secs_elapsed = GetSecondsFromTicks(now - m_last_title_update_ticks);

        if (m_last_title_update_ticks != 0) {
            double hz = num_cycles_elapsed.n / secs_elapsed;
            speed = hz / (double)CYCLES_PER_SECOND;
        }

        m_last_title_update_cycles = num_cycles;
        m_last_title_update_ticks = now;
    }

    // try to smooth the value a bit.
    double smoothed_speed;
    if (m_last_title_speed > 0.) {
        smoothed_speed = m_last_title_speed + (speed - m_last_title_speed) * .75;
    } else {
        smoothed_speed = speed;
    }

    const char *mouse_capture_state = "";
    if (m_is_mouse_captured) {
        mouse_capture_state = " (Mouse Captured)";
    }

    snprintf(title, sizeof title, "%s [%.3fx]%s", m_name.c_str(), smoothed_speed, mouse_capture_state);

    m_last_title_speed = speed;

    SDL_SetWindowTitle(m_window, title);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::BeebKeymapWillBeDeleted(BeebKeymap *keymap) {
    if (m_settings.keymap == keymap) {
        m_settings.keymap = BeebWindows::GetDefaultBeebKeymap();
    }
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

    if (m_vblank_records.size() < NUM_VBLANK_RECORDS) {
        records = m_vblank_records;
    } else {
        ASSERT(m_vblank_index < m_vblank_records.size());
        auto &&it = m_vblank_records.begin() + (ptrdiff_t)m_vblank_index;

        records.reserve(m_vblank_records.size());
        records.insert(records.end(), it, m_vblank_records.end());
        records.insert(records.end(), m_vblank_records.begin(), it);
    }

    return records;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebKeymap *BeebWindow::GetCurrentKeymap() const {
    return m_settings.keymap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SetCurrentKeymap(const BeebKeymap *keymap) {
    m_settings.keymap = keymap;
    m_settings.prefer_shortcuts = m_settings.keymap->GetPreferShortcuts();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if VIDEO_TRACK_METADATA
const VideoDataUnit *BeebWindow::GetVideoDataUnitForMousePixel() const {
    if (m_got_mouse_pixel_unit) {
        return &m_mouse_pixel_unit;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SettingsUI *BeebWindow::GetPopupByType(BeebWindowPopupType type) const {
    ASSERT(type >= 0 && type < BeebWindowPopupType_MaxValue);
    return m_popups[type].get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HardReset(const BeebConfig &config) {
    BeebLoadedConfig tmp;

    if (BeebLoadedConfig::Load(&tmp, config, &m_msg)) {
        m_init_arguments.default_config = std::move(tmp);

        auto message = std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(0, m_init_arguments.default_config);

        m_beeb_thread->Send(std::move(message));

        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &BeebWindow::GetConfigName() const {
    return m_init_arguments.default_config.config.name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::Launch(const BeebWindowLaunchArguments &arguments) {
    std::shared_ptr<MemoryDiscImage> disc_image = MemoryDiscImage::LoadFromFile(arguments.file_path, &m_msg);
    if (!disc_image) {
        return;
    }

    m_beeb_thread->Send(std::make_shared<BeebThread::LoadDiscMessage>(0, std::move(disc_image), true));
    m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Boot | BeebThreadHardResetFlag_Run));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateOptionsUI(BeebWindow *beeb_window) {
    return std::make_unique<OptionsUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateTimelineUI(BeebWindow *beeb_window) {
    return ::CreateTimelineUI(beeb_window, beeb_window->m_renderer);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateSavedStatesUI(BeebWindow *beeb_window) {
    return ::CreateSavedStatesUI(beeb_window, beeb_window->m_renderer);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> BeebWindow::CreateImGuiDebugWindow(BeebWindow *beeb_window) {
    return std::make_unique<ImGuiDebugUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HandleVBlank(VBlankMonitor *vblank_monitor, void *display_data, uint64_t ticks) {
    // There's an API for exactly this on Windows. But it's probably
    // better to have the same code on every platform. 99% of the time
    // (and possibly even more often than that...) this will get the
    // right display.
    int wx, wy;
    SDL_GetWindowPosition(m_window, &wx, &wy);

    int ww, wh;
    SDL_GetWindowSize(m_window, &ww, &wh);

    void *dd = vblank_monitor->GetDisplayDataForPoint(wx + ww / 2, wy + wh / 2);
    if (dd != display_data) {
        return true;
    }

    return this->HandleVBlank(ticks);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindowInitArguments BeebWindow::GetNewWindowInitArguments() const {
    BeebWindowInitArguments ia = m_init_arguments;

    // Propagate current name, not original name.
    ia.name = m_name;

    // Caller will choose the initial state.
    ia.initial_state = nullptr;

    // Feed any output to this window's message list.
    ia.initiating_window_id = SDL_GetWindowID(m_window);

    // New window is parent of whatever.
    //ia.parent_timeline_event_id=0;//m_beeb_thread->GetParentTimelineEventId();

    return ia;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HardReset() {
    // Fetch config from the global list again.
    for (size_t config_idx = 0; config_idx < BeebWindows::GetNumConfigs(); ++config_idx) {
        BeebConfig *config = BeebWindows::GetConfigByIndex(config_idx);

        if (config->name == m_init_arguments.default_config.config.name) {
            this->HardReset(*config);
            return;
        }
    }

    // Something went wrong. Just reuse the current config, whatever it is.
    m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Run));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::RequestRecreateTexture() {
    m_recreate_tv_texture = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::RecreateTexture() {
    if (m_tv_texture) {
        SDL_DestroyTexture(m_tv_texture);
        m_tv_texture = nullptr;
    }

    SetRenderScaleQualityHint(m_settings.display_filter);

    m_tv_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, TV_TEXTURE_WIDTH, TV_TEXTURE_HEIGHT);
    if (!m_tv_texture) {
        m_msg.e.f("Failed to create TV texture: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoPaste(bool add_return) {
    if (m_beeb_thread->IsPasting()) {
        m_beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());
    } else {
        // Get UTF-8 clipboard.
        std::vector<uint8_t> utf8;
        {
            char *tmp = SDL_GetClipboardText();
            if (!tmp) {
                m_msg.e.f("Clipboard error: %s\n", SDL_GetError());
                return;
            }

            utf8.resize(strlen(tmp));
            memcpy(utf8.data(), tmp, utf8.size());

            SDL_free(tmp);
            tmp = nullptr;

            if (utf8.empty()) {
                return;
            }
        }

        // Convert UTF-8 into BBC-friendly ASCII.
        std::string ascii;
        {
            uint32_t bad_codepoint;
            const uint8_t *bad_char_start;
            int bad_char_len;
            if (!GetBBCASCIIFromUTF8(&ascii, utf8, &bad_codepoint, &bad_char_start, &bad_char_len)) {
                if (bad_codepoint == 0) {
                    m_msg.e.f("Clipboard contents are not valid UTF-8 text\n");
                } else {
                    m_msg.e.f("Invalid character: ");

                    if (bad_codepoint >= 32) {
                        m_msg.e.f("'%.*s', ", bad_char_len, bad_char_start);
                    }

                    m_msg.e.f("%u (0x%X)\n", bad_codepoint, bad_codepoint);
                }

                return;
            }
        }

        FixBBCASCIINewlines(&ascii);

        if (add_return) {
            ascii.push_back(13);
        }

        m_beeb_thread->Send(std::make_shared<BeebThread::StartPasteMessage>(std::move(ascii)));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SetClipboardFromBBCASCII(const std::vector<uint8_t> &data, const BeebWindowSettings::CopySettings &settings) const {
    std::string utf8 = GetUTF8FromBBCASCII(data, settings.convert_mode, settings.handle_delete);

    int rc = SDL_SetClipboardText(utf8.c_str());
    if (rc != 0) {
        m_msg.e.f("Failed to copy to clipboard: %s\n", SDL_GetError());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebWindow::DebugStepOver(uint32_t dso) {
    m_beeb_thread->Send(std::make_shared<BeebThread::CallbackMessage>([dso](BBCMicro *m) -> void {
        m->DebugStepOver(dso);
        m->DebugRun();
    }));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebWindow::DebugStepIn(uint32_t dso) {
    m_beeb_thread->Send(std::make_shared<BeebThread::CallbackMessage>([dso](BBCMicro *m) -> void {
        m->DebugStepIn(dso);
        m->DebugRun();
    }));
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
    return m_beeb_thread->DebugIsHalted();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SaveConfig() {
    this->SaveSettings();

    SaveGlobalConfig(&m_msg);

    m_msg.i.f("Configuration saved.\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SetCaptureMouse(bool capture_mouse) {
    if (!m_beeb_thread->HasMouse()) {
        return;
    }

    if (!capture_mouse && m_is_mouse_captured) {
        // Buttons up.
        m_beeb_thread->Send(std::make_shared<BeebThread::MouseButtonsMessage>((uint8_t)(BBCMicroMouseButton_Left | BBCMicroMouseButton_Middle | BBCMicroMouseButton_Right), (uint8_t)0));
    }

    SDL_SetRelativeMouseMode(capture_mouse ? SDL_TRUE : SDL_FALSE);
    m_is_mouse_captured = capture_mouse;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Creates a 24 bpp R8_G8_B8 surface. This format coexists nicely with
// stbi_image_write, which is a bit inflexible in terms of input format.
//
// Output formats can be:
//
// - SDL_PIXELFORMAT_RGB24 = R8G8B8 - for stb_image 3-component RGB writing
// - SDL_PIXELFORMAT_XRGB8888 = B8G8R8X8 - for Windows clipboard, 32 bpp opaque
//   bitmap, skipping a final 32 bpp->24 bpp step
// - SDL_PIXELFORMAT_BGR24 = B8G8R8 - for Windows clipboard, 24 bpp opaque
//   bitmap. Not very compelling as there's an extra unnecessary 32 bpp->24 bpp
//   step

SDLUniquePtr<SDL_Surface> BeebWindow::CreateScreenshot(SDL_PixelFormatEnum pixel_format) const {
    ASSERT(pixel_format == SDL_PIXELFORMAT_RGB24 ||
           pixel_format == SDL_PIXELFORMAT_BGR24 ||
           pixel_format == SDL_PIXELFORMAT_XRGB8888);
    UniqueLock<Mutex> lock;
    uint32_t *tv_pixels;
    if (m_settings.screenshot_last_vsync) {
        tv_pixels = m_tv.GetLastVSyncTexturePixels(&lock);
    } else {
        tv_pixels = m_tv.GetTexturePixels(nullptr);
    }

    // temporary surface referring to the 32 bpp BGRA actual pixel data in the TVOutput
    // object.
    SDLUniquePtr<SDL_Surface> src_surface(SDL_CreateRGBSurfaceFrom(tv_pixels,
                                                                   TV_TEXTURE_WIDTH, TV_TEXTURE_HEIGHT,
                                                                   32,
                                                                   TV_TEXTURE_WIDTH * 4,
                                                                   0x00ff0000,
                                                                   0x0000ff00,
                                                                   0x000000ff,
                                                                   0x00000000));

    if (m_settings.screenshot_correct_aspect_ratio) {
        SDLUniquePtr<SDL_Surface> surface(SDL_CreateRGBSurface(0,
                                                               int(TV_TEXTURE_WIDTH * CORRECT_ASPECT_RATIO_X_SCALE), TV_TEXTURE_HEIGHT,
                                                               32,
                                                               src_surface->format->Rmask,
                                                               src_surface->format->Gmask,
                                                               src_surface->format->Bmask,
                                                               src_surface->format->Amask));
        int blit_result;
        if (m_settings.screenshot_filter) {
            blit_result =
#if HAVE_SDL_SOFTSTRETCHLINEAR
                SDL_SoftStretchLinear
#else
                SDL_BlitScaled
#endif
                (src_surface.get(), nullptr, surface.get(), nullptr);
        } else {
            blit_result = SDL_BlitScaled(src_surface.get(), nullptr, surface.get(), nullptr);
        }

        if (blit_result != 0) {
            m_msg.e.f("Failed to resize image: %s\n", SDL_GetError());
            return nullptr;
        }

        src_surface = std::move(surface);
    }

    if (src_surface->format->format == (Uint32)pixel_format) {
        return src_surface;
    } else {
        std::unique_ptr<SDL_Surface, SDL_Deleter> surface(SDL_CreateRGBSurfaceWithFormat(0, src_surface->w, src_surface->h, 24, pixel_format));
        if (SDL_BlitSurface(src_surface.get(), nullptr, surface.get(), nullptr) != 0) {
            m_msg.e.f("Failed to copy image: %s\n", SDL_GetError());
            return nullptr;
        }

        return surface;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ENABLE_SDL_FULL_SCREEN
bool BeebWindow::IsWindowFullScreen() const {
    uint32_t flags = SDL_GetWindowFlags(m_window);
    return !!(flags & SDL_WINDOW_FULLSCREEN);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ENABLE_SDL_FULL_SCREEN
void BeebWindow::SetWindowFullScreen(bool is_full_screen) {
    uint32_t flags;
    if (is_full_screen) {
        flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else {
        flags = 0;
    }
    SDL_SetWindowFullscreen(m_window, flags);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::ShowPrioritizeCommandShortcutsStatus() {
    if (m_settings.prefer_shortcuts) {
        m_msg.i.f("Prioritize command keys\n");
    } else {
        m_msg.i.f("Prioritize BBC keys\n");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
