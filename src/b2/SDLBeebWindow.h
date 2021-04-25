#ifndef HEADER_939433291D2A4C609E54F31A2B9EC042// -*- mode:c++ -*-
#define HEADER_939433291D2A4C609E54F31A2B9EC042

// Horrible name - will change.
//
// Looks after the SDL thread side window state.

#include "BeebWindow.h"
#include "Messages.h"
//#include <SDL.h>
#include "misc.h"

#include <shared/enum_decl.h>
#include "SDLBeebWindow.inl"
#include <shared/enum_end.h>

struct BeebWindowInitArguments;
struct SDL_KeyboardEvent;
struct SDL_MouseMotionEvent;
class VBlankMonitor;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

// TODO: remove
struct SDLThreadConstantOutput {
    ImTextureID font_texture_id={};
    ImTextureID tv_texture_id={};
};

// TODO: remove
struct SDLThreadVaryingOutput {
    int renderer_output_width=0;
    int renderer_output_height=0;

    // Buffer for SDL keyboard events, so they can be handled as part of
    // the usual update.
    std::vector<SDL_KeyboardEvent> sdl_keyboard_events;
    std::string sdl_text_input;

    // Mouse position.
    bool got_mouse_focus=false;
    SDL_Point mouse_pos={-1,-1};
    uint32_t mouse_buttons=0;
    SDL_Point mouse_wheel_delta={0,0};

    SDL_Keymod keymod=KMOD_NONE;
};

// TODO: remove
struct BeebThreadVaryingOutput {
    ImGuiMouseCursor imgui_mouse_cursor=ImGuiMouseCursor_None;
    bool filter_tv_texture=true;
    std::vector<ImDrawListUniquePtr> draw_lists;

    // this just points straight into the buffer. There's no locking; the SDL
    // thread just gets whatever it happens to be able to see at the time,
    void *tv_texture_data=nullptr;
};

// The placement data isn't used on OS X, but it's included anyway just to save
// on #if hell.
class SDLBeebWindow {
public:
    // Normally, commands are created, prepared, and executed.
    //
    // In timeline mode, commands are recorded into a list for later replay.
    // Commands aren't re-prepared when replaying, just re-executed.
    class Command {
    public:
        typedef std::function<void(bool,std::string)> CompletionFun;
        
        //static void CallCompletionFun(CompletionFun *completion_fun,bool success,const char *message);
        
        explicit Command()=default;
        virtual ~Command()=0;
        
        Command(const Command &)=delete;
        Command &operator=(const Command &)=delete;
        Command(Command &&)=delete;
        Command &operator=(Command &&)=delete;

        // default impl returns CommandPrepareResult_Execute.
        virtual CommandPrepareResult Prepare(SDLBeebWindow *beeb_window);
        virtual void Execute(SDLBeebWindow *beeb_window) const=0;
    protected:
        // standard prepare policies.
        bool PrepareUnlessHalted(SDLBeebWindow *beeb_window,CompletionFun *completion_fun);
        //bool PrepareUnlessReplaying(SDLBeebWindow *beeb_window,CompletionFun *completion_fun);
    private:
    };
    
    class KeySymCommand:
    public Command
    {
    public:
        KeySymCommand(BeebKeySym key_sym,bool down);
        
        CommandPrepareResult Prepare(SDLBeebWindow *beeb_window) override;
        void Execute(SDLBeebWindow *beeb_window) const override;
    protected:
    private:
        const bool m_down=false;
        BeebKey m_beeb_key=BeebKey_None;
        BeebShiftState m_shift_state=BeebShiftState_Any;
    };
    
    class KeyCommand:
    public Command
    {
    public:
        KeyCommand(BeebKey beeb_key,bool down);
        
        CommandPrepareResult Prepare(SDLBeebWindow *beeb_window) override;
        void Execute(SDLBeebWindow *beeb_window) const override;
    protected:
    private:
        const BeebKey m_beeb_key=BeebKey_None;
        const bool m_down=false;
    };
    
    class LoadDiscCommand:
    public Command
    {
    public:
        LoadDiscCommand(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose);
        
        //bool Prepare(SDLBeebWindow *beeb_window,CompletionFun *completion_fun) override;
        void Execute(SDLBeebWindow *beeb_window) const override;
    protected:
    private:
        const int m_drive=-1;
        const std::shared_ptr<DiscImage> m_disc_image;
        const bool m_verbose=false;
    };
    
    class EjectDiscCommand:
    public Command
    {
    public:
        EjectDiscCommand(int drive);
        
        void Execute(SDLBeebWindow *beeb_window) const override;
    protected:
    private:
        const int m_drive=-1;
    };
    
    class SetDriveWriteProtectedCommand:
    public Command
    {
    public:
        SetDriveWriteProtectedCommand(int drive,bool write_protected);

        void Execute(SDLBeebWindow *beeb_window) const override;
    protected:
    private:
        const int m_drive=-1;
        const bool m_write_protected=false;
    };
    
    class HardResetAndReloadConfigCommand:
    public Command
    {
    public:
        HardResetAndReloadConfigCommand(uint32_t reset_flags);
        
        CommandPrepareResult Prepare(SDLBeebWindow *beeb_window) override;
        void Execute(SDLBeebWindow *beeb_window) const override;
    protected:
    private:
        const uint32_t m_reset_flags=0;
        BeebLoadedConfig m_config;
        std::vector<uint8_t> m_nvram_contents;
    };
    
    class HardResetAndChangeConfigCommand:
    public Command
    {
    public:
        HardResetAndChangeConfigCommand(uint32_t reset_flags,BeebLoadedConfig config);
        
        void Execute(SDLBeebWindow *beeb_window) const override;
    protected:
    private:
        const uint32_t m_reset_flags=0;
        const BeebLoadedConfig m_config;
        const std::vector<uint8_t> m_nvram_contents;
    };
    
    SDLBeebWindow()=default;
    ~SDLBeebWindow();

    bool Init(const BeebWindowInitArguments &init_arguments,
              const BeebWindowSettings &settings,
              std::shared_ptr<MessageList> message_list,
              std::vector<uint8_t> window_placement_data,
              uint32_t *sdl_window_id);

    //BeebWindow *GetBeebWindow() const;

    void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);
    void SetSDLMouseWheelState(int x,int y);
    void HandleSDLTextInput(const char *text);
    void HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event);
    void HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks);

    void ThreadFillAudioBuffer(uint32_t audio_device_id,float *mix_buffer,size_t num_samples);

    void UpdateWindowPlacement();
    //const std::vector<uint8_t> &GetWindowPlacementData() const;

    void SaveConfig();
    
    std::shared_ptr<MessageList> GetMessageList() const;
    
    UpdateResult UpdateBeeb();
    
    const BeebKeymap *GetKeymap() const;
    void SetKeymap(const BeebKeymap *keymap);
protected:
private:
    class FileMenuItem;
    
//    enum ImGuiTextures {
//        ImGuiTexture_Font,
//        ImGuiTexture_TV,
//        ImGuiTexture_MaxValue,
//    };

    struct DriveState {
        SaveFileDialog new_disc_image_file_dialog;
        OpenFileDialog open_disc_image_file_dialog;
        SaveFileDialog new_direct_disc_image_file_dialog;
        OpenFileDialog open_direct_disc_image_file_dialog;

        DriveState();
    };

    //
    // Message list and associated UI.
    //
    std::shared_ptr<MessageList> m_message_list;
    uint64_t m_msg_last_num_messages_printed=0;
    uint64_t m_msg_last_num_errors_and_warnings_printed=0;
    bool m_messages_popup_ui_active=false;
    uint64_t m_messages_popup_ticks=0;

    //
    // Settings.
    //
    BeebWindowInitArguments m_init_arguments;
    BeebWindowSettings m_settings;
    
    //
    // Window title.
    //
    uint64_t m_last_title_update_ticks=0;
    uint64_t m_last_title_update_2MHz_cycles=0;
    
    //
    // SDL.
    //
    SDL_Cursor *m_sdl_cursors[ImGuiMouseCursor_COUNT]={};
    SDLUniquePtr<SDL_Window> m_window;
    SDLUniquePtr<SDL_Renderer> m_renderer;
    SDLUniquePtr<SDL_PixelFormat> m_pixel_format;
    //std::vector<SDL_KeyboardEvent> m_sdl_keyboard_events;
    std::string m_sdl_text_input;
    SDL_Point m_mouse_wheel_delta={};
    SDLUniquePtr<SDL_Texture> m_font_texture;
    SDLUniquePtr<SDL_Texture> m_tv_texture;
    
    //
    // Dear imgui.
    //
    std::vector<ImDrawListUniquePtr> m_last_imgui_draw_lists;
    ImGuiStuff *m_imgui_stuff=nullptr;
    
    //
    // Misc UI bits.
    //
    const CommandContext m_cc{this,&ms_command_table};
    
    // Current command tables from the last dear imgui update, so the
    // keyboard handler knows what the keyboard shortcuts are.
    std::vector<const CommandTable *> m_current_command_tables;
    
    DriveState m_drives[NUM_DRIVES];

    //
    // Window placement data.
    //
#if SYSTEM_WINDOWS
    void *m_hwnd=nullptr;
#elif SYSTEM_OSX
    void *m_nswindow=nullptr;
#elif SYSTEM_LINUX
#include <shared/pshpack1.h>
    struct WindowPlacementData {
        uint8_t maximized;
        int x,y;
        int width,height;
    };
#include <shared/poppack.h>
#endif
    std::vector<uint8_t> m_window_placement_data;
    
    //
    // Keyboard input.
    //
    const BeebKeymap *m_keymap=nullptr;
    bool m_prefer_shortcuts=false;
    BeebShiftState m_fake_shift_state=BeebShiftState_Any;
    bool m_auto_boot=false;
    KeyStates m_real_key_states;
    
    // Keycodes to be used for shortcut commands. Don't attempt to handle
    // these outside the dear imgui update, in case the object has gone away;
    // just note that a shortcut may apply, and store off the key in question.
    std::set<uint32_t> m_command_keycodes;
    
    // Set if the BBC display panel has focus. This isn't entirely regular,
    // because the BBC display panel is handled by separate code - this will
    // probably get fixed eventually.
    //
    // The BBC display panel never has any dear imgui text widgets in it.
    bool m_beeb_focus=false;
    
    // Track Beeb key syms that were made pressed by a given
    // (unmodified) SDL keycode. SDL modifier key releases affect
    // future keycodes, so you can get (say) Shift+6 down, then Shift
    // up, then 6 up - which is no good if the states are only being
    // tracked by BBC key.
    //
    // This is a pretty crazy data type, but there's only so many keys
    // you can press per unit time.
    std::map<uint32_t,std::set<BeebKeySym>> m_beeb_keysyms_by_keycode;
    
    //
    // Emulator stuff.
    //
    BeebLoadedConfig m_current_config;
    BBCMicro *m_beeb=nullptr;
    TVOutput m_tv;
    std::vector<uint32_t> m_tv_texture_pixels;

    uint32_t m_leds=0;
    bool m_leds_popup_ui_active=false;
    uint64_t m_leds_popup_ticks=0;

#if ENABLE_IMGUI_DEMO
    bool m_imgui_demo=false;
#endif
    bool m_imgui_dock_debug=false;
    bool m_imgui_debug=false;
    bool m_imgui_metrics=false;
#if STORE_DRAWLISTS
    bool m_imgui_drawlists=false;
    std::vector<ImGuiStuff::StoredDrawList> m_stored_draw_lists;
#endif
    
    bool InitInternal(std::vector<uint8_t> window_placement_data);
    bool RecreateTexture();
    void RenderLastImGuiFrame();
    void RunNextImGuiFrame(uint64_t ticks);
    void HardReset(uint32_t flags,
                   const BeebLoadedConfig &loaded_config,
                   const std::vector<uint8_t> &nvram_contents);
    bool DoBeebDisplayUI();
    void DoMenuUI();
    void DoFileMenu();
    void DoDiscDriveSubMenu(int drive,const std::shared_ptr<const DiscImage> &disc_image);
    void DoDiscImageSubMenu(int drive,bool boot);
    void DoDiscImageSubMenuItem(int drive,std::shared_ptr<DiscImage> disc_image,FileMenuItem *item,bool boot);
    void DoEditMenu();
    void DoHardwareMenu();
    void DoKeyboardMenu();
    void DoToolsMenu();
    void DoDebugMenu();
    void DoPopupUI(uint64_t now,int output_width,int output_height);
    void UpdateTitle(uint64_t ticks);
    void ShowPrioritizeCommandShortcutsStatus();
    bool HandleBeebKey(const SDL_Keysym &keysym,bool state);
//    void RecordKeySym(BeebKeySym beeb_key_sym,bool down);
//    void RecordKey(BeebKey beeb_key,bool down);
//    void RecordLoadDisc(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose);
    //void RecordEjectDisc(int drive);
//    void RecordHardResetAndReloadConfig(uint32_t reset_flags);
//    void RecordHardResetAndChangeConfig(uint32_t reset_flags,BeebLoadedConfig config);
    void SetKeyState(BeebKey beeb_key,bool down);
    void SetFakeShiftState(BeebShiftState shift_state);
    void SetAutoBootState(bool auto_boot);
    void UpdateShiftKeyState();
    bool MaybeStoreCommandKeycode(uint32_t keycode);
    void Exit();
    void SaveDefaultNVRAM();
    bool SaveDefaultNVRAMIsEnabled() const;
    void UpdateSettings();
    bool Execute(std::unique_ptr<Command> command);
    void HardReset();
    
    // Keep this at the end. It's massive.
    Messages m_msg;

    static const ObjectCommandTable<SDLBeebWindow> ms_command_table;
};

#endif
