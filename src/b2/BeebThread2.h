#ifndef HEADER_E0C2E89360164359A1AB8F581180F302// -*- mode:c++ -*-
#define HEADER_E0C2E89360164359A1AB8F581180F302

#include <shared/mutex.h>
#include <thread>
#include "dear_imgui.h"
#include <SDL.h>

#include <shared/enum_decl.h>
#include "BeebThread2.inl"
#include <shared/enum_end.h>

class MessageList;
class BeebThread2State;

class BeebThread2 {
public:
    struct VaryingInput {
        // Keyboard input.
        std::vector<SDL_KeyboardEvent> keyboard_events;
        std::string text_input;
        SDL_Keymod keymod=KMOD_NONE;
        
        // Mouse input.
        bool got_mouse_focus=false;
        SDL_Point mouse_pos={};
        uint32_t mouse_buttons;
        SDL_Point mouse_wheel_delta={};
    };
    
    struct VaryingOutput {
        ImGuiMouseCursor imgui_mouse_cursor=ImGuiMouseCursor_None;
        bool filter_tv_texture=true;
        std::shared_ptr<std::vector<ImDrawListUniquePtr>> draw_lists;
    };
    
    explicit BeebThread2(std::shared_ptr<MessageList> message_list,
                         uint32_t sound_device_id,
                         int sound_freq,
                         size_t sound_buffer_size_samples);
    ~BeebThread2();
    
    BeebThread2(const BeebThread2 &)=delete;
    BeebThread2 &operator=(const BeebThread2 &)=delete;
    BeebThread2(BeebThread2 &&)=delete;
    BeebThread2 &operator=(BeebThread2 &&)=delete;

    bool Start();
    void Stop();
    
    void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);
    void SetSDLMouseWheelState(int x,int y);
    void HandleSDLTextInput(const char *text);
    
    VaryingOutput HandleVBlank(uint64_t ticks);
//
//    //
//    VaryingOutput GetVaryingOutput();
protected:
private:
    BeebThread2State *m_state=nullptr;
//    std::thread m_thread;
//    std::atomic<bool> m_stop{false};

    VaryingInput m_vinput;

    Mutex m_voutput_mutex;
    VaryingOutput m_voutput;
};

#endif
