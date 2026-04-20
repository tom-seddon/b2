#include <shared/system.h>
#include "json.h"
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
#include <beeb/MemoryDiscImage.h>
#include "LoadMemoryDiscImage.h"
#include "BeebWindows.h"
#include <beeb/BBCMicro.h>
#include <SDL_syswm.h>
#include "VBlankMonitor.h"
#include "b2.h"
#include "SymbolTable.h"
#include "native_ui.h"
#include <inttypes.h>
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
#include <http/HTTPServer.h>
#include <beeb/DirectDiscImage.h>
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
#include <shared/file_io.h>
#include "SymbolTable.h"
#include <shared/strings.h>
#include <shared/metrics.h>
#include <beeb/uef.h>

#ifdef IMGUI_ENABLE_TEST_ENGINE
#ifdef KeyPress
// X.h nonsense.
#undef KeyPress
#endif

#include <imgui_test_engine/imgui_te_engine.h>
#include <imgui_test_engine/imgui_te_context.h>
#endif

#ifdef _MSC_VER
#include <crtdbg.h>
#ifdef _DEBUG
#define GOT_CRTDBG 1
#endif
#endif

#include <shared/enum_def.h>
#include "BeebWindow.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SDL_VERSION_ATLEAST(2, 0, 16)
#define HAVE_SDL_SOFTSTRETCHLINEAR 1
#else
#define HAVE_SDL_SOFTSTRETCHLINEAR 0
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static TimerDef g_HandleVBlank_timer_def("BeebWindow::HandleVBlank");
//static TimerDef g_HandleVBlank_end_of_frame_timer_def("BeebWindow::HandleVBlank end of frame",
//                                                      &g_HandleVBlank_timer_def);
//static TimerDef g_HandleVBlank_start_of_frame_timer_def("BeebWindow::HandleVBlank start of frame",
//                                                        &g_HandleVBlank_timer_def);
//static TimerDef g_HandleVBlank_UpdateTVTexture_Consume_timer_def("UpdateTVTexture Consume",
//                                                                 &g_HandleVBlank_end_of_frame_timer_def);
//static TimerDef g_HandleVBlank_UpdateTVTexture_Copy_timer_def("UpdateTVTexture Copy",
//                                                              &g_HandleVBlank_end_of_frame_timer_def);
//static TimerDef g_HandleVBlank_RenderSDL_timer_def("Render SDL",
//                                                   &g_HandleVBlank_end_of_frame_timer_def);
//static TimerDef g_HandleVBlank_DoImGui_timer_def("DoImGui",
//                                                 &g_HandleVBlank_end_of_frame_timer_def);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static CommandTable2 g_beeb_window_command_table("beeb_window", "Beeb Window");
#if SYSTEM_WINDOWS
static Command2 g_toggle_console_command = Command2(&g_beeb_window_command_table, "toggle_console", "Show Win32 console").WithTick();
static Command2 g_clear_console_command(&g_beeb_window_command_table, "clear_console", "Clear Win32 console");
static Command2 g_print_separator_command(&g_beeb_window_command_table, "print_separator", "Print stdout separator");
#endif
static Command2 g_hard_reset_command = Command2(&g_beeb_window_command_table, "hard_reset", "Power-on Reset").MustConfirm();
static Command2 g_hard_reset_multi_os_bank_0_command = Command2(&g_beeb_window_command_table, "hard_reset_multi_os_bank_0", "Multi-OS bank 0").WithExtraText(g_hard_reset_command.GetText()).WithTick();
static Command2 g_hard_reset_multi_os_bank_1_command = Command2(&g_beeb_window_command_table, "hard_reset_multi_os_bank_1", "Multi-OS bank 1").WithExtraText(g_hard_reset_command.GetText()).WithTick();
static Command2 g_hard_reset_multi_os_bank_2_command = Command2(&g_beeb_window_command_table, "hard_reset_multi_os_bank_2", "Multi-OS bank 2").WithExtraText(g_hard_reset_command.GetText()).WithTick();
static Command2 g_hard_reset_multi_os_bank_3_command = Command2(&g_beeb_window_command_table, "hard_reset_multi_os_bank_3", "Multi-OS bank 3").WithExtraText(g_hard_reset_command.GetText()).WithTick();
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
static Command2 g_debug_stop_command = Command2(&g_beeb_window_command_table, "debug_stop", "Stop").WithShortcut(SDLK_F5 | (uint32_t)PCKeyModifier_Shift).VisibleIf(BBCMICRO_DEBUGGER);
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
static Command2 g_load_window_layout_command = Command2(&g_beeb_window_command_table, "load_window_layout", "Load window layout...");
static Command2 g_save_window_layout_command = Command2(&g_beeb_window_command_table, "save_window_layout", "Save window layout...");
static Command2 g_toggle_capture_mouse_command = Command2(&g_beeb_window_command_table, "toggle_capture_mouse", "Capture mouse").WithTick().AlwaysPrioritized();
static Command2 g_toggle_capture_mouse_on_click_command = Command2(&g_beeb_window_command_table, "toggle_capture_mouse_on_click", "Capture on click").WithTick();
static Command2 g_clear_symbols_command = Command2(&g_beeb_window_command_table, "clear_symbols", "Clear symbols").MustConfirm().VisibleIf(BBCMICRO_DEBUGGER);
static Command2 g_reload_all_symbols_command = Command2(&g_beeb_window_command_table, "reload_all_symbols", "Reload all symbols").VisibleIf(BBCMICRO_DEBUGGER);
//static Command2 g_load_project_command = Command2(&g_beeb_window_command_table, "load_project", "Load project...").VisibleIf(BBCMICRO_DEBUGGER);
//static Command2 g_save_project_command = Command2(&g_beeb_window_command_table, "save_project", "Save project").VisibleIf(BBCMICRO_DEBUGGER);
//static Command2 g_save_project_as_command = Command2(&g_beeb_window_command_table, "save_project_as", "Save project as...").VisibleIf(BBCMICRO_DEBUGGER);
static Command2 g_eject_tape_command = Command2(&g_beeb_window_command_table, "eject_tape", "Eject tape");
static Command2 g_load_tape_command = Command2(&g_beeb_window_command_table, "load_tape", "Load tape");

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
    InitialiseTogglePopupCommand(BeebWindowPopupType_Configs, "toggle_configurations", "Configs", &BeebWindow::CreateConfigsUI);
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
    InitialiseTogglePopupCommand(BeebWindowPopupType_PagingBrowserDebugger, "toggle_paging_browser_debugger", "Big Pages Debug", &CreatePagingBrowserDebugWindow);
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
    InitialiseTogglePopupCommand(BeebWindowPopupType_ImGuiDebug, "toggle_imgui_debug", "ImGui Debug", &BeebWindow::CreateImGuiDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_KeyboardDebug, "toggle_keyboard_debug", "Keyboard Debug", &CreateKeyboardDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SystemDebug, "toggle_system_debug", "System Debug", &CreateSystemDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_MouseDebug, "toggle_mouse_debug", "Mouse Debug", &CreateMouseDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_WD1770Debug, "toggle_wd1770_debug", "WD1770 Debug", &CreateWD1770DebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_DiskDriveDebug, "toggle_disk_drive_debug", "Disk Drive Debug", &CreateDiskDriveDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_HardDiskDebug, "toggle_hard_disk_debug", "Hard Disk Debug", &CreateHardDiskDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SCSIDebug, "toggle_scsi_debug", "SCSI Debug", &CreateSCSIDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SerialDebug, "toggle_serial_debug", "Serial Debug", &CreateSerialDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SymbolGroupManagement, "toggle_symbol_group_management", "Symbols", &CreateSymbolGroupManagementWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_SymbolGroupBrowser, "toggle_symbol_browser_debug", "Browse Symbols", &CreateSymbolBrowserWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_MutexStats, "toggle_mutex_stats", "Mutex Stats", &CreateMutexStatsUI);
    InitialiseTogglePopupCommand(BeebWindowPopupType_ElectronULADebug, "toggle_electron_ula_debug", "Electron ULA Debug", &CreateElectronULADebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_TapeDebug, "toggle_tape_debug", "Tape Debug", &CreateTapeDebugWindow);
    InitialiseTogglePopupCommand(BeebWindowPopupType_Plus1Debug, "toggle_plus1_debug", "Plus 1 Debug", &CreatePlus1DebugWindow);
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const Guid AUTODETECT_SYMBOL_PARSER_SELECTOR_GUID{0x8F, 0x3F, 0x81, 0xDE, 0x5D, 0x1B, 0x49, 0x9F, 0x83, 0xB0, 0xCB, 0xE5, 0x78, 0xA8, 0xB4, 0xD0};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const Guid SAVE_PRINTER_DATA_SELECTOR_GUID{0xC8, 0x23, 0x72, 0x73, 0x34, 0x60, 0x48, 0x94, 0x8D, 0x84, 0xD4, 0xE0, 0x61, 0xAA, 0xC7, 0x79};
const Guid SAVE_SCREENSHOT_SELECTOR_GUID{0x86, 0x14, 0x49, 0x92, 0xE5, 0x36, 0x4D, 0x99, 0xBE, 0xF1, 0x4A, 0xCA, 0x8B, 0x26, 0x05, 0x13};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const Guid NEW_DISK_IMAGE_SELECTOR_GUID{0x72, 0x23, 0xE7, 0xC3, 0x78, 0xA0, 0x41, 0x0C, 0xB9, 0x63, 0x90, 0x2F, 0x24, 0x25, 0x01, 0xD7};
static RecentPaths g_disk_image_recent_paths("disc_image");
const Guid OPEN_DISK_IMAGE_SELECTOR_GUID{0x4c, 0xed, 0x04, 0x1d, 0x00, 0xf1, 0x46, 0x2f, 0x88, 0x0e, 0xc2, 0x39, 0x95, 0xd0, 0x38, 0xde};
const Guid SAVE_DISK_IMAGE_COPY_SELECTOR_GUID{0x3e, 0x34, 0x69, 0xad, 0xf8, 0xc6, 0x44, 0x79, 0xbe, 0x5c, 0x7d, 0x2a, 0xa3, 0x6a, 0xce, 0x64};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static RecentPaths g_tape_recent_paths("tape");
#if ENABLE_TAPE
const Guid OPEN_TAPE_FILE_SELECTOR_GUID{0x52, 0xf5, 0x5e, 0x8f, 0xad, 0xf9, 0x4f, 0x33, 0xa7, 0xa4, 0xb1, 0xba, 0x28, 0xfb, 0x35, 0x95};
#endif
const FileDialog::Filter TAPE_FILE_FILTER{"UEF File", {".uef"}};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const Guid OPEN_WINDOW_LAYOUT_SELECTOR_GUID{0xFF, 0x3E, 0x9F, 0xBB, 0xBE, 0x25, 0x48, 0xB0, 0xAA, 0x74, 0x03, 0x21, 0xB5, 0x4F, 0xCF, 0x83};
const Guid SAVE_WINDOW_LAYOUT_SELECTOR_GUID{0x9D, 0x61, 0x95, 0x0E, 0xF8, 0x19, 0x4A, 0x33, 0xB5, 0x2F, 0x9E, 0xB2, 0x15, 0xED, 0xD6, 0xF9};
static RecentPaths g_window_layout_recent_paths("window_layout");
const FileDialog::Filter WINDOW_LAYOUT_FILTER{"b2 Window Layout", {".b2_layout"}};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// GetWindowData has a loop (!) with strcmp in it (!) so the data name
// wants to be short.
const char BeebWindow::SDL_WINDOW_DATA_NAME[] = "D";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void to_json(nlohmann::json &j, const BeebWindowPopupFlags &flags) {
    j = nlohmann::json::array();
    for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
        if (flags.flags[i]) {
            j.push_back(GetBeebWindowPopupTypeEnumName(i));
        }
    }
}

void from_json(const nlohmann::json &j, BeebWindowPopupFlags &flags) {
    flags = {};
    if (j.is_array()) {
        for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
            if (std::find(j.begin(), j.end(), GetBeebWindowPopupTypeEnumName(i)) != j.end()) {
                flags.flags[i] = true;
            }
        }
    }
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

static void ImGuiVolume(const char *caption, float *volume, bool *mute, const std::shared_ptr<BeebThread> &beeb_thread, void (BeebThread::*set_volume_mfn)(float, bool)) {
    ImGuiIDPusher pusher(caption);

    bool changed = false;

    if (ImGui::SliderFloat(caption, volume, MIN_DB, MAX_DB, "%.1f dB")) {
        changed = true;
    }

    ImGui::SameLine();

    if (ImGui::Checkbox("Mute", mute)) {
        changed = true;
    }

    if (changed) {
        (*beeb_thread.*set_volume_mfn)(*volume, *mute);
    }
}

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
        ImGuiHeader("Speed");

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

        ImGui::Checkbox("Background economy mode", &settings->background_economy_mode);
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

        ImGui::Checkbox("Hide CRTC cursor when unfocused", &settings->hide_cursor_when_unfocused);

#if 1 //BUILD_TYPE_Debug
        if (ImGui::Checkbox("Threaded texture update", &m_beeb_window->m_update_tv_texture_thread_enabled)) {
            if (!!m_beeb_window->m_metric_set) {
                m_beeb_window->m_metric_set->ResetTimerDefs();
            }
        }
#endif
    }

    ImGui::NewLine();

    {
        ImGuiHeader("LEDs");

        int mode = settings->leds_popup_mode;

        ImGui::RadioButton("Auto hide", &mode, BeebWindowLEDsPopupMode_Auto);
        ImGui::RadioButton("Always on", &mode, BeebWindowLEDsPopupMode_On);
        ImGui::RadioButton("Always off", &mode, BeebWindowLEDsPopupMode_Off);

        settings->leds_popup_mode = (BeebWindowLEDsPopupMode)mode;

        ImGui::SliderFloat("BG opacity", &settings->leds_popup_alpha, 0.f, 1.f);
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

        ImGuiVolume("BBC volume", &settings->bbc_volume, &settings->bbc_mute, beeb_thread, &BeebThread::SetBBCVolume);

        ImGuiVolume("Disc volume", &settings->disc_volume, &settings->disc_mute, beeb_thread, &BeebThread::SetDiscVolume);

        if (ImGui::Checkbox("Power-on tone", &settings->power_on_tone)) {
            beeb_thread->SetPowerOnTone(settings->power_on_tone);
        }

        if (ImGui::Checkbox("Low pass filter", &settings->low_pass_filter)) {
            beeb_thread->SetLowPassFilter(settings->low_pass_filter);
        }

        // 20,000 Hz should hopefully be more than enough for the b2 user
        // demographic: people, mostly older.
        if (ImGui::SliderInt("Low pass filter cutoff", &settings->low_pass_filter_cutoff_hz, 100, 20000, "%d Hz")) {
            beeb_thread->SetLowPassFilterCutoff(settings->low_pass_filter_cutoff_hz);
        }
    }

    ImGui::NewLine();

    {
        ImGuiHeader("UI");

        float scale;

        scale = m_beeb_window->m_imgui_stuff->GetScale();
        if (ImGui::InputFloat("GUI Scale", &scale, 0.f, 0.f)) {
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                m_beeb_window->m_imgui_stuff->SetScale(scale);
            }
        }

        if (ImGui::Checkbox("Pixel font", &settings->gui_pixel_font)) {
            m_beeb_window->m_imgui_stuff->SetPixelFont(settings->gui_pixel_font);
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
        ImGuiHeader("Debugger Options");

        ImGui::TextUnformatted("Syntax");
        ImGuiRadioButton(&m_beeb_window->m_settings.debugger_syntax, DebuggerSyntax_BBCBASIC, "BBC BASIC");
        ImGuiRadioButton(&m_beeb_window->m_settings.debugger_syntax, DebuggerSyntax_Assembler, "Assembler");
        ImGuiRadioButton(&m_beeb_window->m_settings.debugger_syntax, DebuggerSyntax_C, "C");

        ImGui::Checkbox("Show memory-mapped I/O", &m_beeb_window->m_settings.debugger_show_mmio);
    }

    {
        ImGuiHeader("Debug Options");

        ImGui::Checkbox("Show extra C++ debug UI", &m_beeb_window->m_settings.extra_debug_ui);

        std::shared_ptr<const BBCMicroReadOnlyState> beeb_state;
        m_beeb_window->m_beeb_thread->DebugGetState(&beeb_state, nullptr);

        bool teletext_debug = beeb_state->saa5050.debug;
        if (ImGui::Checkbox("Teletext debug", &teletext_debug)) {
            m_beeb_window->m_beeb_thread->Send(
                std::make_shared<BeebThread::CallbackMessage>([teletext_debug](BBCMicro *m) -> void {
                    m->SetTeletextDebug(teletext_debug);
                }));
        }

        bool teletext_dim_flash = beeb_state->saa5050.dim_flash;
        if (ImGui::Checkbox("Show teletext flash as dim", &teletext_dim_flash)) {
            m_beeb_window->m_beeb_thread->Send(
                std::make_shared<BeebThread::CallbackMessage>([teletext_dim_flash](BBCMicro *m) -> void {
                    m->SetTeletextDimFlash(teletext_dim_flash);
                }));
        }

        ImGui::Checkbox("Show TV beam position", &m_beeb_window->m_tv.show_beam_position);
        if (ImGui::Checkbox("Test pattern", &m_beeb_window->m_test_pattern)) {
            if (m_beeb_window->m_test_pattern) {
                m_beeb_window->m_tv.FillWithTestPattern();
            }
        }

        ImGui::Checkbox("Fill window (overrides auto scale/correct aspect ratio)", &m_beeb_window->m_display_fill);

        ImGui::Checkbox("1.0 " MICROSECONDS_UTF8, &m_beeb_window->m_tv.show_usec_markers);
        ImGui::SameLine();
        ImGui::Checkbox("0.5 " MICROSECONDS_UTF8, &m_beeb_window->m_tv.show_half_usec_markers);

        ImGui::Checkbox("6845 rows", &m_beeb_window->m_tv.show_6845_row_markers);
        ImGui::SameLine();
        ImGui::Checkbox("6845 DISPEN", &m_beeb_window->m_tv.show_6845_dispen_markers);

        ImGui::TextUnformatted("RAM errors");

        uint8_t ram_and, ram_or;
        beeb_state->DebugGetMemoryFaultMasks(&ram_and, &ram_or);

        bool changed = false;

        for (int bit_index = 0; bit_index < 8; ++bit_index) {
            ImGuiIDPusher pusher(bit_index);

            if (bit_index > 0) {
                ImGui::SameLine();
            }

            int bit = 7 - bit_index;
            uint8_t mask = 1 << bit;

            bool and_ = !!(ram_and & mask);
            bool or_ = !!(ram_or & mask);

            int value;
            if (!and_ && !or_) {
                value = 0;
            } else if (and_ && !or_) {
                value = 2;
            } else {
                value = 1;
            }

            char caption[2] = {};
            caption[0] = "01-"[value];

            if (ImGui::Button(caption)) {
                value = (value + 1) % 3;
                changed = true;

                if (value == 0) {
                    ram_and &= ~mask;
                    ram_or &= ~mask;
                } else if (value == 1) {
                    ram_and &= ~mask;
                    ram_or |= mask;
                } else {
                    ram_and |= mask;
                    ram_or &= ~mask;
                }
            }

            ImGui::SameLine();
            ImGui::Text("%d", bit);
        }

        if (changed) {
            m_beeb_window->m_beeb_thread->Send(
                std::make_shared<BeebThread::CallbackMessage>(
                    std::function<void(BBCMicro *)>(),
                    [ram_and, ram_or](BBCMicro *m) -> void {
                        m->SetMemoryAccessErrorMasks(ram_and, ram_or);
                    }));
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

class BeebWindow::CopyOSWRCHCallback : public OSWRCHCallback {
  public:
    CopyOSWRCHCallback() {
        MUTEX_SET_NAME(m_mutex, "CopyOSWRCHCallback");
    }

    void ThreadOnOSWRCH(BeebThread *beeb_thread, uint8_t a) override {
        (void)beeb_thread;

        LockGuard<Mutex> lock(m_mutex);

        if (m_capturing) {
            m_data.push_back(a);
        }
    }

    void TakeDataAndStopCapturing(std::vector<uint8_t> *data) {
        LockGuard<Mutex> lock(m_mutex);

        *data = std::move(m_data);
        m_capturing = false;
    }

  protected:
  private:
    Mutex m_mutex;
    std::vector<uint8_t> m_data;
    bool m_capturing = true;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindow::BeebWindow(BeebWindowInitArguments init_arguments)
    : m_init_arguments(std::move(init_arguments))
#if BBCMICRO_DEBUGGER
    , m_symbol_table(std::make_unique<SymbolTable>())
#endif
{
    m_name = m_init_arguments.name;

    m_metric_set = MetricSet::Create(m_name);
    m_HandleVBlank_timer_def = MetricSet::CreateTimerDef(m_metric_set, "BeebWindow::HandleVBlank");
    m_HandleVBlank_end_of_frame_timer_def = MetricSet::CreateTimerDef(m_metric_set, "BeebWindow::HandleVBlank end of frame", m_HandleVBlank_timer_def);
    m_HandleVBlank_start_of_frame_timer_def = MetricSet::CreateTimerDef(m_metric_set, "BeebWindow::HandleVBlank start of frame", m_HandleVBlank_timer_def);
    m_HandleVBlank_UpdateTVTexture_Consume_timer_def = MetricSet::CreateTimerDef(m_metric_set, "UpdateTVTexture Consume", m_HandleVBlank_end_of_frame_timer_def);
    m_HandleVBlank_UpdateTVTexture_Copy_timer_def = MetricSet::CreateTimerDef(m_metric_set, "UpdateTVTexture Copy", m_HandleVBlank_end_of_frame_timer_def);
    m_HandleVBlank_RenderSDL_timer_def = MetricSet::CreateTimerDef(m_metric_set, "Render SDL", m_HandleVBlank_end_of_frame_timer_def);
    m_HandleVBlank_DoImGui_timer_def = MetricSet::CreateTimerDef(m_metric_set, "DoImGui", m_HandleVBlank_end_of_frame_timer_def);

    if (m_init_arguments.app_handler->IsHeadless()) {
        m_message_list = MessageList::stdio;
    } else {
        m_message_list = std::make_shared<MessageList>("BeebWindow");
    }
    m_msg.SetMessageList(m_message_list);

    if (init_arguments.verbose) {
        m_message_list->SetFlags(m_message_list->GetFlags() | MessageListFlags_Stdio);
    }

    uint32_t sound_device = m_init_arguments.sound_device;
    if (m_init_arguments.app_handler->IsHeadless()) {
        sound_device = 0;
    }

    m_beeb_thread = std::make_shared<BeebThread>(m_message_list,
                                                 m_metric_set,
                                                 sound_device,
                                                 m_init_arguments.sound_spec.freq,
                                                 m_init_arguments.sound_spec.samples,
                                                 m_init_arguments.default_config,
                                                 std::vector<BeebThread::TimelineEventList>(),
                                                 false);

    if (init_arguments.use_settings) {
        m_settings = init_arguments.settings;
    } else {
        m_settings = BeebWindows::defaults;
    }

#if BBCMICRO_DEBUGGER
    // Load symbol table from persistent data if available
    if (!m_settings.symbol_table_data.is_null()) {
        try {
            if (!m_symbol_table->LoadFromJSON(m_settings.symbol_table_data, &m_msg)) {
                m_msg.w.f("Failed to load persistent symbol table data\n");
            }
        } catch (const std::exception &e) {
            m_msg.w.f("Failed to load persistent symbol table data: %s\n", e.what());
        }
    }
#endif

    m_beeb_thread->SetBBCVolume(m_settings.bbc_volume, m_settings.bbc_mute);
    m_beeb_thread->SetDiscVolume(m_settings.disc_volume, m_settings.disc_mute);
    m_beeb_thread->SetPowerOnTone(m_settings.power_on_tone);
    m_beeb_thread->SetLowPassFilter(m_settings.low_pass_filter);
    m_beeb_thread->SetLowPassFilterCutoff(m_settings.low_pass_filter_cutoff_hz);
    m_beeb_thread->Send(std::make_shared<BeebThread::SetSpeedLimitedMessage>(m_init_arguments.limit_speed));

    m_blend_amt = 1.f;

    if (!m_init_arguments.app_handler->IsHeadless()) {
        if (m_init_arguments.app_handler->IsDearImGuiTestEngineEnabled()) {
            m_settings.extra_debug_ui = true;
            m_imgui_test_engine_ui = true;
        }
    }
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

    //    if (m_sound_device != 0) {
    //        SDL_UnlockAudioDevice(m_sound_device);
    //    }
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
    m_imgui_stuff->AddFocusEvent(true);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLFocusLostEvent() {
    for (uint8_t i = 0; i < NUM_BEEB_JOYSTICKS; ++i) {
        auto message = std::make_shared<BeebThread::JoystickButtonMessage>(i, false);
        m_beeb_thread->Send(std::move(message));
    }

    this->SetCaptureMouse(false);

    m_imgui_stuff->AddFocusEvent(false);
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
        m_imgui_stuff->AddMouseButtonEvent(event.which, event.button, event.type == SDL_MOUSEBUTTONDOWN);
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

SDL_Window *BeebWindow::GetSDLWindow() const {
    return m_window;
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
        m_imgui_stuff->AddMouseWheelEvent(event.which, (float)event.x, (float)event.y);
#else
        m_imgui_stuff->AddMouseWheelEvent(event.which, event.preciseX, event.preciseY);
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
        m_imgui_stuff->AddMouseMotionEvent(event.which, event.x, event.y);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::HandleSDLTextInput(const char *text) {
    m_imgui_stuff->AddInputCharactersUTF8(text);
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
    ImVec2 display_size = m_imgui_stuff->GetDisplaySize();

    SettingsUI *active_popup = nullptr;

    // Set if the BBC display panel has Dear ImGui focus. This isn't entirely
    // regular, because the BBC display panel is handled by separate code - this
    // will probably get fixed eventually.
    //
    // The BBC display panel never has any dear imgui text widgets in it.
    bool beeb_got_imgui_focus = false;

    this->DoMenuUI();

    bool close_window = false;
    this->DoCommands(&close_window);

    ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGuiDockNodeFlags dock_space_flags = (ImGuiDockNodeFlags_PassthruCentralNode |
                                           ImGuiDockNodeFlags_NoDockingOverCentralNode);
    ImGuiID dock_space_id = ImGui::DockSpaceOverViewport(0, viewport, dock_space_flags);

    active_popup = this->DoSettingsUI();

    if (ImGuiDockNode *central_node = ImGui::DockBuilderGetCentralNode(dock_space_id)) {
        ImGui::SetNextWindowPos(central_node->Pos);
        ImGui::SetNextWindowSize(central_node->Size);

        beeb_got_imgui_focus = this->DoBeebDisplayUI();

        this->DoPopupUI(ticks, display_size);
    }

#if ENABLE_IMGUI_DEMO
    if (m_imgui_demo_ui) {
        ImGui::ShowDemoWindow();
    }
#endif

#if STORE_DRAWLISTS
    if (m_imgui_drawlists_ui) {
        m_imgui_stuff->DoStoredDrawListWindow();
    }
#endif

    if (m_imgui_metrics_ui) {
        ImGui::ShowMetricsWindow();
    }

#ifdef IMGUI_ENABLE_TEST_ENGINE
    if (m_imgui_test_engine_ui) {
        m_imgui_stuff->DoTestEngineWindow(nullptr);
    }
#endif

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

            if (beeb_got_imgui_focus) {
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

                if (!handled) {
                    if (!m_settings.prefer_shortcuts) {
                        m_cst.ActionCommands(beeb_window_commands);
                        if (active_popup_commands) {
                            active_popup->cst->ActionCommands(active_popup_commands);
                        }
                    }
                }
            } else {
                m_cst.ActionCommands(beeb_window_commands);

                if (active_popup_commands) {
                    active_popup->cst->ActionCommands(active_popup_commands);
                }
            }
        }

        if (!beeb_got_imgui_focus && m_beeb_got_imgui_focus) {
            m_beeb_thread->Send(std::make_shared<BeebThread::AllKeysUpMessage>());
        }
    }

    m_sdl_keyboard_events.clear();
    m_beeb_got_imgui_focus = beeb_got_imgui_focus;

    // Check if the BBC display actually has system keyboard focus.
    bool beeb_actually_got_focus = false;
    if (SDL_GetKeyboardFocus() == m_window) {
        beeb_actually_got_focus = beeb_got_imgui_focus;
    }
    m_beeb_thread->SetShowCursor(beeb_actually_got_focus || !m_settings.hide_cursor_when_unfocused);

#ifdef IMGUI_ENABLE_TEST_ENGINE
    if (ImGuiTestEngine *test_engine = m_imgui_stuff->GetTestEngine()) {
        switch (m_test_engine_state) {
        case DearImGuiTestEngineState_None:
            if (m_test_engine_ready_counter > 0) {
                --m_test_engine_ready_counter;
            }
            if (m_test_engine_ready_counter == 0) {
                //if (m_beeb_thread->IsStarted()){
                m_init_arguments.app_handler->DearImGuiTestEngineDidBecomeReady(this, m_imgui_stuff);
                m_test_engine_state = DearImGuiTestEngineState_Initialised;
            }
            break;

        case DearImGuiTestEngineState_Initialised:
            if (ImGuiTestEngine_IsTestQueueEmpty(test_engine)) {
                if (m_init_arguments.app_handler->ShouldQuitWhenTestQueueEmpty()) {
                    this->Exit();
                    m_test_engine_state = DearImGuiTestEngineState_Done;
                }
            }
            break;

        case DearImGuiTestEngineState_Done:
            break;
        }
    }
#endif

    return !close_window; // sigh, inverted logic
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BeebConfig *FindMutableBeebConfigByName(const std::string &name) {
    for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
        BeebConfig *config = BeebWindows::GetMutableConfigByIndex(i);
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
        this->HardResetWithMultiOSBank(-1);
    }

    if (m_cst.WasActioned(g_hard_reset_multi_os_bank_0_command)) {
        this->HardResetWithMultiOSBank(0);
    }

    if (m_cst.WasActioned(g_hard_reset_multi_os_bank_1_command)) {
        this->HardResetWithMultiOSBank(1);
    }

    if (m_cst.WasActioned(g_hard_reset_multi_os_bank_2_command)) {
        this->HardResetWithMultiOSBank(2);
    }

    if (m_cst.WasActioned(g_hard_reset_multi_os_bank_3_command)) {
        this->HardResetWithMultiOSBank(3);
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
        this->Exit();
    }

    if (m_cst.WasActioned(g_clean_up_recent_files_lists_command)) {
        size_t num_removed = 0;

        const std::vector<RecentPaths *> *all_paths = GetAllRecentPaths();
        for (RecentPaths *paths : *all_paths) {
            size_t path_index = 0;
            while (path_index < paths->GetNumPaths()) {
                if (PathIsFileOnDisk(paths->GetPathByIndex(path_index), nullptr, nullptr)) {
                    ++path_index;
                } else {
                    paths->RemovePathByIndex(path_index);
                    ++num_removed;
                }
            }
        }

        if (num_removed > 0) {
            m_msg.i.f("Removed %zu items\n", num_removed);
        }
    }

    if (m_cst.WasActioned(g_reset_dock_windows_command)) {
        this->ResetImGuiWindows();
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

        SaveFileDialog fd(SAVE_PRINTER_DATA_SELECTOR_GUID, m_init_arguments.app_handler);

        fd.AddFilter("Data", {".dat"});

        std::string path;
        if (fd.Open(m_window, &path)) {
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
    m_cst.SetEnabled(g_debug_run_command, this->DebugGetHaltReason() != BBCMicroHaltReason_None);
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
            m->DebugHalt(BBCMicroHaltReason_ManualHalt, nullptr, -1);
        }));
    }
#endif

    m_cst.SetEnabled(g_save_default_nvram_command, HasNVRAM(m_beeb_thread->GetBBCMicroTypeID()));
    if (m_cst.WasActioned(g_save_default_nvram_command)) {
        this->SaveDefaultNVRAMForCurrentConfig();
    }

    m_cst.SetEnabled(g_reset_default_nvram_command, m_cst.GetEnabled(g_save_default_nvram_command));
    if (m_cst.WasActioned(g_reset_default_nvram_command)) {
        std::string config_name;
        m_beeb_thread->GetConfig(&config_name, nullptr, nullptr);
        if (BeebConfig *config = FindMutableBeebConfigByName(config_name)) {
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
        SaveFileDialog fd(SAVE_SCREENSHOT_SELECTOR_GUID, m_init_arguments.app_handler);

        fd.AddFilter("PNG", {".png"});

        std::string path;
        if (fd.Open(m_window, &path)) {
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
        PopupMetadata *popup_metadata = &g_popups[type];

        if (m_cst.WasActioned(popup_metadata->command)) {
            m_settings.popups.flags[type] = !m_settings.popups.flags[type];
        }
        m_cst.SetTicked(popup_metadata->command, m_settings.popups.flags[type]);
    }

#if BBCMICRO_DEBUGGER
    if (m_cst.WasActioned(g_clear_symbols_command)) {
        m_symbol_table->Clear();
    }
#endif

#if BBCMICRO_DEBUGGER
    if (m_cst.WasActioned(g_reload_all_symbols_command)) {
        m_symbol_table->ReloadAllFiles(&m_msg);
        m_msg.i.f("All symbol files have been reloaded from disk.\n");
    }
#endif

    if (m_cst.WasActioned(g_load_window_layout_command)) {
        OpenFileDialog fd(OPEN_WINDOW_LAYOUT_SELECTOR_GUID, m_init_arguments.app_handler);
        fd.AddFilter(WINDOW_LAYOUT_FILTER);

        std::string path;
        if (fd.Open(m_window, &path)) {
            fd.AddLastPathToRecentPaths(&g_window_layout_recent_paths);

            this->LoadWindowLayout(path);
        }
    }

    if (m_cst.WasActioned(g_save_window_layout_command)) {
        SaveFileDialog fd(SAVE_WINDOW_LAYOUT_SELECTOR_GUID, m_init_arguments.app_handler);
        fd.AddFilter(WINDOW_LAYOUT_FILTER);

        std::string path;
        if (fd.Open(m_window, &path)) {
            fd.AddLastPathToRecentPaths(&g_window_layout_recent_paths);

            this->SaveWindowLayout(path);
        }
    }

#if ENABLE_TAPE
    m_cst.SetEnabled(g_eject_tape_command, !!m_beeb_thread->GetTape());
    if (m_cst.WasActioned(g_eject_tape_command)) {
        m_beeb_thread->Send(std::make_shared<BeebThread::EjectTapeMessage>());
    }

    if (m_cst.WasActioned(g_load_tape_command)) {
        OpenFileDialog fd(OPEN_TAPE_FILE_SELECTOR_GUID, m_init_arguments.app_handler);
        fd.AddFilter(TAPE_FILE_FILTER);

        std::string path;
        if (fd.Open(m_window, &path)) {
            fd.AddLastPathToRecentPaths(&g_tape_recent_paths);

            this->LoadTape(path);
        }
    }
#endif
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
#if ENABLE_DEBUG_MENU
        this->DoDebugMenu();
#endif
        this->DoExtraDebugMenu();
        this->DoWindowMenu();
        ImGui::EndMainMenuBar();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SettingsUI *BeebWindow::DoSettingsUI() {
    SettingsUI *active_popup = nullptr;

    for (int type = 0; type < BeebWindowPopupType_MaxValue; ++type) {
        PopupMetadata *popup_metadata = &g_popups[type];

        if (m_settings.popups.flags[type]) {
            if (!m_popups[type]) {
                m_popups[type] = CreatePopup(*popup_metadata, this, m_imgui_stuff);

                if (m_popups[type]) {
                    m_popups[type]->SetName(popup_metadata->command.GetText());

                    const nlohmann::json *j = &BeebWindows::defaults.popup_persistent_data[type];
                    if (!j->is_null()) {
                        m_popups[type]->LoadPersistentData(*j);
                    }
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
                m_settings.popups.flags[type] = true;

                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                    active_popup = popup;
                }

                if (popup) {
                    popup->DoImGui();
                } else {
                    ImGui::TextWrapped("This build of b2 does not support this type of window.");
                }
            }
            ImGui::End();

            if (!opened) {
                m_settings.popups.flags[type] = false;

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
        m_settings.popups.flags[BeebWindowPopupType_Messages] = true;
    }

    return active_popup;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoPopupUI(uint64_t now, const ImVec2 &display_size) {
    bool show_popup_ui = true;
    if (m_init_arguments.app_handler->IsDearImGuiTestEngineEnabled()) {
        // The popups can interfere with the test engine, so don't show em.
        show_popup_ui = false;
    }

    if (ValueChanged(&m_msg_last_num_messages_printed, m_message_list->GetNumMessagesPrinted())) {
        m_messages_popup_ui_active = true;
        m_messages_popup_ticks = now;
    }

    if (m_messages_popup_ui_active) {
        if (show_popup_ui) {
            // With ImGuiWindowFlags_NoMouseInputs, the popup is ignored entirely
            // for hovering purposes, so the mouse can end up interacting with
            // widgets behind it. Which doesn't feel very desirable, as the popup is
            // mostly opaque.
            //
            // Without it, you can still give the popup focus by clicking. Which
            // also isn't ideal.
            ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar |
                                      ImGuiWindowFlags_NoNavInputs |
                                      ImGuiWindowFlags_NoNavFocus |
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
                ImGuiWindow *window = ImGui::GetCurrentWindow();
                ImGui::BringWindowToDisplayFront(window);

                m_message_list->ForEachMessage(15, [](MessageList::Message *m) {
                    if (!m->seen) {
                        ImGuiMessageListMessage(m);
                    }
                });
            }
            ImGui::End();
        }

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
    if (ValueChanged(&m_leds, m_beeb_thread->GetLEDs()) || (m_leds & BBCMicro::LED_FLAGS_ALL_DISKS)) {
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
        if (show_popup_ui) {
            ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar |
                                      //ImGuiWindowFlags_ShowBorders|
                                      ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::SetNextWindowPos(ImVec2(10.f, display_size.y - m_leds_popup_height - 2));

            ImGui::SetNextWindowBgAlpha(m_settings.leds_popup_alpha);

            if (ImGui::Begin("LEDs", &m_leds_popup_ui_active, flags)) {
                ImGuiStyleColourPusher colour_pusher;
                colour_pusher.Push(ImGuiCol_CheckMark, ImVec4(1.f, 0.f, 0.f, 1.f));

                ImGuiLED(ImGuiLEDStyle_Circle, !!(m_leds & BBCMicroLEDFlag_CapsLock), "Caps Lock");

                ImGui::SameLine();

                ImGuiLED(ImGuiLEDStyle_Circle, !!(m_leds & BBCMicroLEDFlag_ShiftLock), "Shift Lock");

                ImGui::SameLine();

                ImGuiLED(ImGuiLEDStyle_Circle, !!(m_leds & BBCMicroLEDFlag_TapeMotor), "Motor");

                ImGui::SameLine();

                colour_pusher.Push(ImGuiCol_CheckMark, ImVec4(0.f, 1.f, 0.f, 1.f));

                switch (timeline_state.mode) {
                case BeebThreadTimelineMode_None:
                    ImGuiLED(ImGuiLEDStyle_Circle, false, "Replay");
                    break;

                case BeebThreadTimelineMode_Replay:
                    ImGuiLED(ImGuiLEDStyle_Circle, true, "Replay");
                    ImGui::SameLine();
                    if (ImGui::Button("Stop")) {
                        m_beeb_thread->Send(std::make_shared<BeebThread::StopReplayMessage>());
                    }
                    break;

                case BeebThreadTimelineMode_Record:
                    colour_pusher.Push(ImGuiCol_CheckMark, ImVec4(1.f, 0.f, 0.f, 1.f));
                    ImGuiLED(ImGuiLEDStyle_Circle, true, "Record");
                    ImGui::SameLine();
                    if (ImGuiConfirmButton("Stop")) {
                        m_beeb_thread->Send(std::make_shared<BeebThread::StopRecordingMessage>());
                    }
                    colour_pusher.Pop();
                    break;
                }

                ImGui::SameLine();
                ImGuiLED(ImGuiLEDStyle_Circle, copying, "Copy");
                if (copying) {
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        m_beeb_thread->Send(std::make_shared<BeebThread::StopCopyMessage>());
                    }
                }

                ImGui::SameLine();
                ImGuiLED(ImGuiLEDStyle_Circle, pasting, "Paste");
                if (pasting) {
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        m_beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());
                    }
                }

                colour_pusher.Pop();
                colour_pusher.Push(ImGuiCol_CheckMark, ImVec4(1.f, 0.f, 0.f, 1.f));

                for (int i = 0; i < NUM_DRIVES; ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    ImGuiLEDf(ImGuiLEDStyle_Rectangle, !!(m_leds & 1 << (BBCMicroLEDFlag_FloppyDisk0Shift + i)), "Drive %d", i);
                }

                for (int i = 0; i < NUM_HARD_DISKS; ++i) {
                    ImGui::SameLine();
                    ImGuiLEDf(ImGuiLEDStyle_Rectangle, !!(m_leds & 1 << (BBCMicroLEDFlag_HardDisk0Shift + i)), "HD %d", i);
                }
            }

            // Annoyingly, it takes a couple of frames for the window height to
            // settle down. Try to figure out when it's at its final value, so the
            // popup doesn't briefly appear in the wrong place.
            {
                float y = ImGui::GetCursorPosY();
                float h = ImGui::GetWindowHeight();

                if (h > y) {
                    m_leds_popup_height = h;
                }
            }

            ImGui::End();
        }

        if (GetSecondsFromTicks(now - m_leds_popup_ticks) > LEDS_POPUP_TIME_SECONDS) {
            if (m_settings.leds_popup_mode != BeebWindowLEDsPopupMode_On) {
                m_leds_popup_ui_active = false;
            }
        }
    }

    if (show_popup_ui) {
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
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoFileMenu() {
    if (ImGui::BeginMenu("File###file")) {
        std::string config_name;
        BeebConfigArguments config_arguments;
        m_beeb_thread->GetConfig(&config_name, nullptr, &config_arguments);

        // This relies on the logic in BeebLoadedConfig::Load to set
        // multi_os_bank appropriately, including setting it to <0 in the case
        // of the model not having a multi-OS in the first place.
        if (config_arguments.multi_os_bank >= 0) {
            if (ImGui::BeginMenu(g_hard_reset_command.GetText().c_str())) {
                // TODO: the tick state should be set more often than this, but
                // they're for informational purposes only, so it doesn't
                // particularly matter.
                m_cst.SetTicked(g_hard_reset_multi_os_bank_0_command, config_arguments.multi_os_bank == 0);
                m_cst.SetTicked(g_hard_reset_multi_os_bank_1_command, config_arguments.multi_os_bank == 1);
                m_cst.SetTicked(g_hard_reset_multi_os_bank_2_command, config_arguments.multi_os_bank == 2);
                m_cst.SetTicked(g_hard_reset_multi_os_bank_3_command, config_arguments.multi_os_bank == 3);

                if (ImGui::MenuItem("Confirm")) {
                    m_cst.ActionCommand(g_hard_reset_command);
                }

                ImGui::Separator();

                m_cst.DoMenuItem(g_hard_reset_multi_os_bank_0_command);
                m_cst.DoMenuItem(g_hard_reset_multi_os_bank_1_command);
                m_cst.DoMenuItem(g_hard_reset_multi_os_bank_2_command);
                m_cst.DoMenuItem(g_hard_reset_multi_os_bank_3_command);
                ImGui::EndMenu();
            }
        } else {
            m_cst.DoMenuItem(g_hard_reset_command);
        }

        if (ImGui::BeginMenu("Run")) {
            this->DoDiscImageSubMenu(0, true);

            ImGui::EndMenu();
        }

        ImGui::Separator();

#if ENABLE_TAPE
        if (ImGui::BeginMenu("Tape##tape")) {
            std::shared_ptr<const UEFReader> tape = m_beeb_thread->GetTape();

            if (!!tape) {
                const std::string &name = tape->GetName();
                if (!name.empty()) {
                    if (ImGui::BeginMenu("Full path")) {
                        ImGui::MenuItem(name.c_str(), nullptr, false, false);

                        if (ImGui::MenuItem("Copy path to clipboard")) {
                            SDL_SetClipboardText(name.c_str());
                        }

                        ImGui::EndMenu();
                    }
                }

                m_cst.DoMenuItem(g_eject_tape_command);
            } else {
                ImGui::MenuItem("(no tape selected)", nullptr, false, false);
            }

            ImGui::Separator();

            m_cst.DoMenuItem(g_load_tape_command);

            std::string path;
            if (ImGuiRecentMenu(&path, "Recent tape", &g_tape_recent_paths)) {
                this->LoadTape(path);
            }

            ImGui::EndMenu();
        }
#endif

        for (int drive = 0; drive < NUM_DRIVES; ++drive) {
            char title[100];
            snprintf(title, sizeof title, "Drive %d###drive%d", drive, drive);

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
        if (!m_cst.GetEnabled(g_save_state_command)) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                uint32_t clone_impediments = m_beeb_thread->GetBBCMicroCloneImpediments();
                ImGui::SetTooltip("Can't save state, due to: %s", GetCloneImpedimentsDescription(clone_impediments).c_str());
            }
        }

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
                disc_image->SaveToFile(disc_image->GetName(), &m_msg);
            }
        }

        if (ImGui::MenuItem("Save copy as...###save_copy_as")) {
            printf("*** HERE\n");
            SaveFileDialog fd(SAVE_DISK_IMAGE_COPY_SELECTOR_GUID, m_init_arguments.app_handler);

            std::vector<FileDialogFilter> filters = disc_image->GetFileDialogFilters();
            for (const FileDialogFilter &filter : filters) {
                fd.AddFilter(filter.name, filter.extensions);
            }
            fd.AddAllFilesFilter();

            fd.SetSuggestedName(disc_image->GetName());

            std::string path;
            if (fd.Open(m_window, &path)) {
                if (disc_image->SaveToFile(path, &m_msg)) {
                    fd.AddLastPathToRecentPaths(&g_disk_image_recent_paths);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::DoNewCopyOfDiskMenu(std::string *path,
                                     const Disc *disks,
                                     size_t num_disks) {
    for (size_t i = 0; i < num_disks; ++i) {
        const Disc *disk = &disks[i];

        std::string text;
        if (disk->blank) {
            text = disk->name;
        } else {
            text = "Copy of " + disk->name;
        }
        text += "###" + disk->name;

        if (ImGui::MenuItem(text.c_str())) {
            std::string src_path = disk->GetAssetPath();
            std::vector<uint8_t> data;
            if (!LoadFile(&data, src_path, &m_msg)) {
                return false;
            }

            if (disk->geometry->adfs) {
                RandomizeADFSDiskIdentifier(&data);
            }

            SaveFileDialog fd(NEW_DISK_IMAGE_SELECTOR_GUID, m_init_arguments.app_handler);

            if (const char *ext = GetExtensionFromDiscGeometry(*disk->geometry)) {
                fd.AddFilter(std::string(ext) + " file", {ext});
            } else {
                // Take a guess...
                fd.AddAllFilesFilter();
            }

            if (!disk->blank) {
                fd.SetSuggestedName(disk->path);
            }

            if (fd.Open(m_window, path)) {
                if (!SaveFile(data, *path, &m_msg)) {
                    return false;
                }

                fd.AddLastPathToRecentPaths(&g_disk_image_recent_paths);

                return true;
            }
        }
    }

    return false;
}

// Returns true if a disk should be loaded - *path is the path to load. This
// also covers the new disk image case (*path is the path to the new disk image,
// which has been already copied into place).
bool BeebWindow::DoDiscImageSubMenu2(std::string *path,
                                     const char *disk_image_caption,
                                     const char *new_disk_image_caption,
                                     const char *recent_disk_image_caption,
                                     bool allow_zipped) {
    bool result = false;

    if (disk_image_caption) {
        if (ImGui::MenuItem(disk_image_caption)) {
            OpenFileDialog fd(OPEN_DISK_IMAGE_SELECTOR_GUID, m_init_arguments.app_handler);

            fd.AddFilter("BBC disc images", DISC_IMAGE_EXTENSIONS);
            if (allow_zipped) {
                fd.AddFilter("Zipped BBC disc images", {".zip"});
            }

            if (fd.Open(m_window, path)) {
                fd.AddLastPathToRecentPaths(&g_disk_image_recent_paths);
                result = true;
            }
        }

        if (new_disk_image_caption) {
            if (ImGui::BeginMenu(new_disk_image_caption)) {
                if (this->DoNewCopyOfDiskMenu(path, BLANK_DFS_DISCS, NUM_BLANK_DFS_DISCS)) {
                    result = true;
                }

                ImGui::Separator();

                if (this->DoNewCopyOfDiskMenu(path, BLANK_ADFS_DISCS, NUM_BLANK_ADFS_DISCS)) {
                    result = true;
                }

                ImGui::Separator();

                if (this->DoNewCopyOfDiskMenu(path, WELCOME_DISKS, NUM_WELCOME_DISKS)) {
                    result = true;
                }

                ImGui::EndMenu();
            }
        }
    }

    if (recent_disk_image_caption) {
        if (ImGuiRecentMenu(path,
                            recent_disk_image_caption,
                            &g_disk_image_recent_paths)) {
            result = true;
        }
    }

    return result;
}

void BeebWindow::DoDiscImageSubMenu(int drive, bool boot) {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);

    std::string path;

    std::shared_ptr<DiscImage> disc_image;

    if (this->DoDiscImageSubMenu2(&path,
                                  "Disc image...",
                                  boot ? nullptr : "New disc image###new_file",
                                  "Recent disc image",
                                  false)) {
        disc_image = DirectDiscImage::CreateForFile(path, m_msg);
    }

    if (this->DoDiscImageSubMenu2(&path,
                                  "In-memory disc image...###open_memory",
                                  boot ? nullptr : "New in-memory disc image###new_memory",
                                  "Recent in-memory disc image",
                                  true)) {
        disc_image = LoadMemoryDiscImage(path, m_msg);
    }

    if (!!disc_image) {
        m_beeb_thread->Send(std::make_shared<BeebThread::LoadDiscMessage>(drive, std::move(disc_image), true));

        if (boot) {
            m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Boot |
                                                                                              BeebThreadHardResetFlag_Run));
        }
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

        std::string config_name;
        m_beeb_thread->GetConfig(&config_name, nullptr, nullptr);

        for (size_t config_idx = 0; config_idx < BeebWindows::GetNumConfigs(); ++config_idx) {
            bool selected = false;

            BeebConfig *config = BeebWindows::GetMutableConfigByIndex(config_idx);
            bool ticked = config->name == config_name;
            std::string item_name = config->name + "###" + std::to_string(config_idx);

            if (ImGui::MenuItem(item_name.c_str(), nullptr, ticked)) {
                selected = true;
            }

            if (selected) {
                if (this->HardReset(*config, {}, BeebThreadHardResetFlag_Run)) {
                    m_settings.config = config->name;
                }
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

#if ENABLE_DEBUG_MENU
void BeebWindow::DoDebugMenu() {
    if (ImGui::BeginMenu("Debug")) {
        m_cst.DoMenuItem(g_debug_stop_command);
        m_cst.DoMenuItem(g_debug_run_command);

        ImGui::Separator();

        if (ImGui::BeginMenu("System")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Trace].command);
#if VIDEO_TRACK_METADATA
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_PixelMetadata].command);
#endif
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SystemDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_PagingDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_PagingBrowserDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_BreakpointsDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ElectronULADebug].command);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("CPU")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_6502Debugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_StackDebugger].command);

            if (ImGui::BeginMenu("Memory Debug")) {
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger1].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger2].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger3].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MemoryDebugger4].command);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Disassembly Debug")) {
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger1].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger2].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger3].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DisassemblyDebugger4].command);
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Devices")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_CRTCDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_VideoULADebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SystemVIADebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_UserVIADebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_NVRAMDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SN76489Debugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ADCDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DigitalJoystickDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_KeyboardDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MouseDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_WD1770Debug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DiskDriveDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_HardDiskDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SCSIDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SerialDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_TapeDebug].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Plus1Debug].command);
            if (ImGui::BeginMenu("External Memory Debug")) {
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger1].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger2].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger3].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ExtMemoryDebugger4].command);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tube")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_TubeDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteStackDebugger].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_Parasite6502Debugger].command);

            if (ImGui::BeginMenu("Parasite Memory Debug")) {
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger1].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger2].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger3].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteMemoryDebugger4].command);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Parasite Disassembly Debug")) {
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger1].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger2].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger3].command);
                m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ParasiteDisassemblyDebugger4].command);
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Symbols")) {
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SymbolGroupManagement].command);
            m_cst.DoMenuItem(g_popups[BeebWindowPopupType_SymbolGroupBrowser].command);

            ImGui::EndMenu();
        }

        ImGui::Separator();

        m_cst.DoMenuItem(g_clear_symbols_command);

        // TODO: no current good way of integrating this tidily with the
        // Command2 system.
        if (ImGui::BeginMenu("Load symbols")) {
            const std::vector<std::unique_ptr<const SymbolTable::SymbolParser>> &parsers = SymbolTable::SymbolParserRegistry::GetParsers();

            const SymbolTable::SymbolParser *selected_parser = nullptr;
            bool load_symbols = false;

            // There's no way to get the actual selected filter index on macOS
            // (and the filter options are pretty limited anyway...) so safest
            // to have the format selection implied by the action rather than
            // trying to figure it out from the file dialog filter index.

            if (ImGui::MenuItem("Auto-detect...")) {
                load_symbols = true;
            }

            for (const std::unique_ptr<const SymbolTable::SymbolParser> &parser : parsers) {
                std::string name = parser->GetDisplayName() + "...";
                if (ImGui::MenuItem(name.c_str())) {
                    selected_parser = parser.get();
                    load_symbols = true;
                }
            }

            ImGui::EndMenu();

            if (load_symbols) {
                OpenFileDialog fd(selected_parser ? selected_parser->guid : AUTODETECT_SYMBOL_PARSER_SELECTOR_GUID, m_init_arguments.app_handler);

                if (selected_parser) {
                    fd.AddFilter(selected_parser->GetDisplayName(), selected_parser->GetSuggestedFileExtensions());
                } else {
                    std::set<std::string> auto_detect_exts;
                    for (const std::unique_ptr<const SymbolTable::SymbolParser> &parser : parsers) {
                        std::vector<std::string> exts = parser->GetSuggestedFileExtensions();
                        auto_detect_exts.insert(exts.begin(), exts.end());
                    }

                    fd.AddFilter("Auto detect", std::vector<std::string>(auto_detect_exts.begin(), auto_detect_exts.end()));
                }

                fd.AddAllFilesFilter();

                // TODO: should there be a recent paths list for these??
                std::string path;
                if (fd.Open(m_window, &path)) {
                    // The settings can be modified once the symbol file is loaded.
                    bool success = m_symbol_table->LoadFromFile(path, selected_parser, &m_msg);
                    if (success) {
                        m_msg.i.f("Symbols loaded from file: %s\n", path.c_str());
                    } else {
                        m_msg.e.f("Failed to load symbols from: %s\n", path.c_str());
                    }
                }
            }
        }

        m_cst.DoMenuItem(g_reload_all_symbols_command);

        if (ImGui::BeginMenu("Symbol groups")) {
            // Show loaded groups and symbols (grouped by name for bulk operations)
            const size_t num_files = m_symbol_table->GetNumFiles();
            if (num_files > 0) {
                ImGui::Text("Symbol groups:");

                // Show one checkbox per used group
                for (unsigned group_index = 0; group_index < MAX_NUM_SYMBOL_FILE_GROUPS; ++group_index) {
                    const SymbolGroup *group = m_symbol_table->GetSymbolGroupByIndex((uint8_t)group_index);

                    if (!group->used) {
                        continue;
                    }

                    ImGuiIDPusher id_pusher(group_index);

                    ImGuiSymbolGroupEnabledCheckbox(m_symbol_table.get(), group, true);
                }

                size_t enabled_count = m_symbol_table->GetEnabledSymbolCount();
                size_t total_count = m_symbol_table->GetSymbolCount();
                if (enabled_count == total_count) {
                    ImGui::Text("Total symbols: %zu", enabled_count);
                } else {
                    ImGui::Text("Active symbols: %zu / %zu", enabled_count, total_count);
                }
            } else {
                ImGui::TextDisabled("No symbols loaded");
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::DoExtraDebugMenu() {
    if (!m_settings.extra_debug_ui) {
        return;
    }

    if (ImGui::BeginMenu("Extras")) {
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_AudioCallback].command);
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_MutexStats].command);
#if ENABLE_IMGUI_TEST
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_DearImguiTest].command);
#endif

#if ENABLE_IMGUI_DEMO
        ImGui::MenuItem("ImGui demo", NULL, &m_imgui_demo_ui);
#endif
        m_cst.DoMenuItem(g_popups[BeebWindowPopupType_ImGuiDebug].command);
#if STORE_DRAWLISTS
        ImGui::MenuItem("ImGui drawlists", nullptr, &m_imgui_drawlists_ui);
#endif
        ImGui::MenuItem("ImGui metrics", nullptr, &m_imgui_metrics_ui);
#ifdef IMGUI_ENABLE_TEST_ENGINE
        if (m_imgui_stuff->IsTestEngineEnabled()) {
            ImGui::MenuItem("ImGui Test Engine", nullptr, &m_imgui_test_engine_ui);
        }
#endif

#if SYSTEM_WINDOWS
        ImGui::Separator();

        m_cst.DoMenuItem(g_toggle_console_command);

        if (HasWindowsConsole()) {
            m_cst.DoMenuItem(g_clear_console_command);

            m_cst.DoMenuItem(g_print_separator_command);
        }
#endif

        ImGui::EndMenu();
    }
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
        ImGui::Separator();
        m_cst.DoMenuItem(g_load_window_layout_command);
        m_cst.DoMenuItem(g_save_window_layout_command);
        std::string recent_path;
        if (ImGuiRecentMenu(&recent_path, "Recent window layout", &g_window_layout_recent_paths)) {
            this->LoadWindowLayout(recent_path);
        }

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

                if (state->update_dest_pixels) {
                    state->update_tv->CopyTexturePixels(state->update_dest_pixels,
                                                        (size_t)state->update_dest_pitch);
                }
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
    ASSERT(!dest_pixels || dest_pitch > 0);
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
    Timer tmr(m_HandleVBlank_UpdateTVTexture_Consume_timer_def);

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

        if (dest_pixels) {
            ASSERT(dest_pitch > 0);
            m_tv.CopyTexturePixels(dest_pixels, (size_t)dest_pitch);
        }
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
            ImGui::Image((ImTextureID)m_tv_texture, size);

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
    if (!m_send_main_thread_ready_message) {
        m_beeb_thread->Send(std::make_shared<BeebThread::MainThreadIsReadyMessage>());
        m_send_main_thread_ready_message = true;
    }

    bool economy = false;

    ++m_vblank_counter;

    if (m_settings.background_economy_mode) {
        if (SDL_GetKeyboardFocus() != m_window && SDL_GetMouseFocus() != m_window) {
            if (m_vblank_counter % 16 != 0) {
                economy = true;
            }
        }
    }

    if (economy) {
        // Economy mode update.
        //
        // Mostly do nothing, but do at least consume some video units, ideally
        // without doing any rendering.
        m_tv.SetEnableRender(false);
        VBlankRecord *vblank_record = this->NewVBlankRecord(ticks);
        this->EndUpdateTVTexture(false, vblank_record, nullptr, 0);
        return true;
    }

    PROFILE_SCOPE(PROFILER_COLOUR_DEEP_PINK, "HandleVBlank");
    ImGuiContextSetter setter(m_imgui_stuff);

    Timer HandleVBlank_timer(m_HandleVBlank_timer_def);

    bool keep_window = true;

    m_tv.SetEnableRender(true);

    // don't use m_update_tv_texture_thread_enabled directly - it might change
    // during the DoImGui call.
    bool threaded_update = m_update_tv_texture_thread_enabled;

#if BBCMICRO_DEBUGGER
    {
        std::shared_ptr<const BBCMicroReadOnlyState> state;
        m_beeb_thread->DebugGetState(&state, nullptr);
        if (!state) {
            // Beeb thread hasn't started yet. Bail and cross fingers that it'll
            // become ready in time. (I don't think this case can be hit in
            // normal use?? - just if first startup is taking longer than
            // normal. I hit this after adding a lot of logging to BBCMicro
            // construction.)
            return true;
        }
    }
#endif

    {
        Timer tmr2(m_HandleVBlank_start_of_frame_timer_def);

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

        if (m_beeb_thread->TakeNVRAMChanged()) {
            this->SaveDefaultNVRAMForCurrentConfig();
        }

        //m_pushed_window_padding = true;
    }

    {
        Timer HandleVBlank_end_of_frame_timer(m_HandleVBlank_end_of_frame_timer_def);

        VBlankRecord *vblank_record = this->NewVBlankRecord(ticks);

        void *dest_pixels = nullptr;
        int dest_pitch = 0;
        if (m_tv_texture) {
            SDL_LockTexture(m_tv_texture, nullptr, &dest_pixels, &dest_pitch);
        }

        this->BeginUpdateTVTexture(threaded_update, dest_pixels, dest_pitch);

        {
            PROFILE_SCOPE(PROFILER_COLOUR_INDIAN_RED, "DoImGui");
            Timer tmr3(m_HandleVBlank_DoImGui_timer_def);

            if (!this->DoImGui(ticks)) {
                keep_window = false;
            }

            m_imgui_stuff->RenderImGui();
        }

        if (m_renderer) {
            SDL_RenderClear(m_renderer);
        }

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
            Timer HandleVBlank_RenderSDL_timer(m_HandleVBlank_RenderSDL_timer_def);

            m_imgui_stuff->RenderSDL();

            if (m_renderer) {
                SDL_RenderPresent(m_renderer);
            }

            m_imgui_stuff->PostSwap();
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

#if BBCMICRO_DEBUGGER
    // Save symbol table state
    m_settings.symbol_table_data = m_symbol_table->SaveToJSON();
#endif

    m_settings.gui_scale = m_imgui_stuff->GetScale();

    BeebWindows::defaults = m_settings;

    this->SavePosition();

    for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
        if (m_popups[i]) {
            BeebWindows::defaults.popup_persistent_data[i] = m_popups[i]->SavePersistentData();
        }
    }
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
    ASSERT(m_sound_device == 0 || m_init_arguments.sound_spec.freq > 0);

#if BUILD_TYPE_Debug
    m_msg.i.f("%d popup types\n", BeebWindowPopupType_MaxValue);
#endif

    bool reset_windows = m_init_arguments.reset_windows;
    m_init_arguments.reset_windows = false;

    const float display_size_x = TV_TEXTURE_WIDTH + IMGUI_DEFAULT_STYLE.WindowPadding.x * 2.f;
    const float display_size_y = TV_TEXTURE_HEIGHT + IMGUI_DEFAULT_STYLE.WindowPadding.y * 2.f;

    if (!m_init_arguments.app_handler->IsHeadless()) {
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
        uint32_t window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
        if (m_init_arguments.enable_high_dpi) {
            window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
        }
        m_window = SDL_CreateWindow("",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    (int)display_size_x,
                                    (int)display_size_y,
                                    window_flags);
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
    }

#if ENABLE_SDL_FULL_SCREEN
    if (!reset_windows) {
        this->SetWindowFullScreen(m_settings.full_screen);
    }
#endif

#if RMT_ENABLED
    if (m_renderer) {
        if (g_num_BeebWindow_inits == 0) {
#if RMT_USE_OPENGL
            if (strcmp(info.name, "opengl") == 0) {
                rmt_BindOpenGL();
                g_unbind_opengl = 1;
            }
#endif
        }
        ++g_num_BeebWindow_inits;
    }
#endif

    if (!this->RecreateTexture()) {
        return false;
    }

#ifdef IMGUI_ENABLE_TEST_ENGINE
    bool imgui_enable_test_engine = m_init_arguments.app_handler->IsDearImGuiTestEngineEnabled();
#else
    bool imgui_enable_test_engine = false;
#endif
    m_imgui_stuff = new ImGuiStuff(m_window, m_renderer, imgui_enable_test_engine, {display_size_x, display_size_y});
    if (!m_imgui_stuff->Init(ImGuiConfigFlags_DockingEnable)) {
        m_msg.e.f("failed to initialise ImGui\n");
        return false;
    }

    m_imgui_stuff->SetScale(m_settings.gui_scale);
#if SYSTEM_LINUX
    if (m_init_arguments.gui_scale > 0.f) {
        m_imgui_stuff->SetScale(m_init_arguments.gui_scale);
    }
#endif

    m_imgui_stuff->SetPixelFont(m_settings.gui_pixel_font);

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
        m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(m_init_arguments.default_config, 0));

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
            m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(m_init_arguments.default_config, flags));
        }
    }

    //    if(!m_init_arguments.initially_paused) {
    //        m_beeb_thread->Send(std::make_shared<BeebThread::PauseMessage>(false));
    //    }

    if (reset_windows) {
        this->ResetImGuiWindows();
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

    if (m_renderer) {
        SDL_RendererInfo renderer_info;
        if (SDL_GetRendererInfo(m_renderer, &renderer_info) < 0) {
            m_msg.e.f("SDL_GetRendererInfo failed: %s\n", SDL_GetError());
            return false;
        }

        Uint32 format;
        int width, height;
        SDL_QueryTexture(m_tv_texture, &format, nullptr, &width, &height);
        m_msg.i.f("Renderer: %s, %dx%d %s\n",
                  renderer_info.name,
                  width,
                  height,
                  SDL_GetPixelFormatName(format));
    } else {
        m_msg.i.f("Renderer: none (running headless)\n");
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

    if (audio_device_id != 0) {
        if (m_sound_device != audio_device_id) {
            return;
        }
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

    if (m_window) {
        SDL_SetWindowTitle(m_window, title);
    } else {
        puts(title);
    }
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

#if BBCMICRO_DEBUGGER
SymbolTable *BeebWindow::GetMutableSymbolTable() {
    return m_symbol_table.get();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const SymbolTable *BeebWindow::GetSymbolTable() const {
    return m_symbol_table.get();
}
#endif

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

void BeebWindow::Launch(const BeebWindowLaunchArguments &arguments) {
    if (PathCompare(PathGetExtension(arguments.file_path), WINDOW_LAYOUT_FILTER.extensions[0]) == 0) {
        this->LoadWindowLayout(arguments.file_path);
    } else {
        std::shared_ptr<MemoryDiscImage> disc_image = LoadMemoryDiscImage(arguments.file_path, m_msg);
        if (!disc_image) {
            return;
        }

        m_beeb_thread->Send(std::make_shared<BeebThread::LoadDiscMessage>(0, std::move(disc_image), true));
        m_beeb_thread->Send(std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Boot | BeebThreadHardResetFlag_Run));
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

std::unique_ptr<SettingsUI> BeebWindow::CreateConfigsUI(BeebWindow *beeb_window) {
    std::string config_name;
    beeb_window->m_beeb_thread->GetConfig(&config_name, nullptr, nullptr);

    size_t config_index = INVALID_CONFIG_INDEX;
    for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
        const BeebConfig *config = BeebWindows::GetConfigByIndex(i);
        if (config->name == config_name) {
            config_index = i;
            break;
        }
    }

    return ::CreateConfigsUI(beeb_window, config_index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebWindowSettings &BeebWindow::GetSettings() const {
    return m_settings;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

AppHandler *BeebWindow::GetAppHandler() const {
    return m_init_arguments.app_handler;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> BeebWindow::GetR8G8B8A8DisplayData() const {
    UniqueLock<Mutex> lock;
    uint32_t *tv_pixels = m_tv.GetLastVSyncTexturePixels(&lock);

    std::vector<uint8_t> data(TV_TEXTURE_WIDTH * TV_TEXTURE_HEIGHT * 4);
    const uint32_t *src = tv_pixels;
    uint8_t *dest = data.data();
    for (int y = 0; y < TV_TEXTURE_HEIGHT; ++y) {
        for (int x = 0; x < TV_TEXTURE_WIDTH; ++x) {
            uint32_t v = *src++;

            *dest++ = v >> 16 & 0xff; //r
            *dest++ = v >> 8 & 0xff;  //g
            *dest++ = v & 0xff;       //b
            *dest++ = 0xff;           //a
        }
    }

    return data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::StartCopyOSWRCH() {
    if (!m_copy_oswrch_callback) {
        m_copy_oswrch_callback = std::make_shared<CopyOSWRCHCallback>();
        m_beeb_thread->Send(std::make_shared<BeebThread::AddOSWRCHCallbackMessage>(m_copy_oswrch_callback));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::StopCopyOSWRCH(std::vector<uint8_t> *data) {
    if (!m_copy_oswrch_callback) {
        return false;
    } else {
        m_copy_oswrch_callback->TakeDataAndStopCapturing(data);
        m_beeb_thread->Send(std::make_shared<BeebThread::RemoveOSWRCHCallbackMessage>(m_copy_oswrch_callback));
        m_copy_oswrch_callback = nullptr;

        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#ifdef IMGUI_ENABLE_TEST_ENGINE
//std::vector<std::string> BeebWindow::GetAllDearImGuiTestNames() {
//    std::vector<std::string> names;
//    InitDearImGuiTests(nullptr, nullptr, nullptr, &names, nullptr);
//    return names;
//}
//#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#ifdef IMGUI_ENABLE_TEST_ENGINE
//std::vector<std::shared_ptr<b2Test>> BeebWindow::Getb2Tests(const std::vector<std::string> &tests_to_run) {
//    std::vector<std::shared_ptr<b2Test>> b2_tests;
//    InitDearImGuiTests(nullptr, nullptr, &b2_tests, nullptr, &tests_to_run);
//    return b2_tests;
//}
//#endif

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

    if (m_renderer) {
        m_tv_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, TV_TEXTURE_WIDTH, TV_TEXTURE_HEIGHT);
        if (!m_tv_texture) {
            m_msg.e.f("Failed to create TV texture: %s\n", SDL_GetError());
            return false;
        }
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
        std::vector<uint8_t> bbc_ascii;
        {
            int32_t bad_codepoint;
            size_t bad_char_start;
            int bad_char_len;
            if (!GetBBCASCIIFromUTF8(&bbc_ascii, utf8, &bad_codepoint, &bad_char_start, &bad_char_len)) {
                if (bad_codepoint < 0) {
                    m_msg.e.f("Clipboard contents are not valid UTF-8 text\n");
                } else {
                    m_msg.e.f("Invalid character: ");

                    if (bad_codepoint >= 32) {
                        m_msg.e.f("'%.*s', ", bad_char_len, &utf8[bad_char_start]);
                    }

                    m_msg.e.f("%u (0x%X)\n", bad_codepoint, bad_codepoint);
                }

                return;
            }
        }

        FixBBCASCIINewlines(&bbc_ascii);

        if (add_return) {
            bbc_ascii.push_back(13);
        }

        m_beeb_thread->Send(std::make_shared<BeebThread::StartPasteMessage>(std::move(bbc_ascii), 0));
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
    return this->DebugGetHaltReason() != BBCMicroHaltReason_None;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicroHaltReason BeebWindow::DebugGetHaltReason() const {
    return m_beeb_thread->DebugGetHaltReason();
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

void BeebWindow::SaveDefaultNVRAMForCurrentConfig() {
    std::string config_name;
    m_beeb_thread->GetConfig(&config_name, nullptr, nullptr);

    if (BeebConfig *config = FindMutableBeebConfigByName(config_name)) {
        config->nvram = m_beeb_thread->GetNVRAM();
    }
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

void BeebWindow::ResetImGuiWindows() {
    m_settings.popups = {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::LoadWindowLayout(const std::string &path) {
    WindowLayoutPersistentData wlpd;
    if (LoadJSONFile(&wlpd, path, &m_msg)) {
        std::string ini_data;
        for (const std::string &line : wlpd.dear_imgui_settings) {
            ini_data += line;
            ini_data.push_back('\n');
        }

        ImGui::LoadIniSettingsFromMemory(ini_data.c_str());

        m_settings.popups = wlpd.popups;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::SaveWindowLayout(const std::string &path) {
    WindowLayoutPersistentData wlpd;

    wlpd.popups = m_settings.popups;

    size_t ini_data_size;
    if (const char *ini_data = ImGui::SaveIniSettingsToMemory(&ini_data_size)) {
        ForEachLine(std::string(ini_data, ini_data + ini_data_size),
                    [&wlpd](const std::string_view &line) -> bool {
                        wlpd.dear_imgui_settings.push_back(std::string(line.begin(), line.end()));
                        return true;
                    });
    }

    SaveJSONFile(wlpd, path, &m_msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HardReset(const BeebConfig &config, const BeebConfigArguments &arguments, uint32_t flags) {
    BeebLoadedConfig tmp;

    if (BeebLoadedConfig::Load(&tmp, config, arguments, &m_msg)) {
        m_init_arguments.default_config = std::move(tmp);

        auto message = std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(m_init_arguments.default_config, flags);

        m_beeb_thread->Send(std::move(message));

        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindow::HardResetWithMultiOSBank(int multi_os_bank) {
    BeebConfig config;
    BeebConfigArguments arguments;
    m_beeb_thread->GetConfig(nullptr, &config, &arguments);

    arguments.multi_os_bank = multi_os_bank;

    bool good = this->HardReset(config, arguments, BeebThreadHardResetFlag_Run);
    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ENABLE_TAPE
void BeebWindow::LoadTape(std::string path) {
    std::vector<uint8_t> uef_data;
    if (LoadPossiblyGzippedFile(&uef_data, path, &m_msg, 0)) {
        auto uef = std::make_shared<UEFReader>();
        if (uef->Load(std::move(uef_data), std::move(path), &m_msg)) {
            m_beeb_thread->Send(std::make_shared<BeebThread::LoadTapeMessage>(std::move(uef)));
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindow::Exit() {
    this->SaveSettings();

    SDL_Event event = {};
    event.type = SDL_QUIT;

    SDL_PushEvent(&event);
}
