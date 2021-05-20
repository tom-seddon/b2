#include <shared/system.h>
#include <memory>
#include <atomic>
#include "BeebThread2.h"
#include "MessageQueue.h"

#include <shared/enum_def.h>
#include "BeebThread2.inl"
#include <shared/enum_end.h>

class BeebThread2State {
public:
    class Message {
    public:
        typedef std::function<void(bool,std::string)> CompletionFun;
        
        Message()=default;
        virtual ~Message()=0;
        
        Message(const Message &)=delete;
        Message &operator=(const Message &)=delete;
        Message(Message &&)=delete;
        Message &operator=(Message &&)=delete;
    protected:
    private:
    };
    
    explicit BeebThread2State(Mutex *voutput_mutex,
                              BeebThread2::VaryingOutput *voutput);
    ~BeebThread2State();
    
    BeebThread2State(const BeebThread2State &)=delete;
    BeebThread2State &operator=(const BeebThread2State &)=delete;
    BeebThread2State(BeebThread2State &&)=delete;
    BeebThread2State &operator=(BeebThread2State &&)=delete;
    
    bool StartThread();
    void StopThread();
    
    void Send(std::unique_ptr<Message> message);
protected:
private:
    struct SentMessage {
        std::unique_ptr<Message> message;
        Message::CompletionFun fun;
    };
    
    MessageQueue<SentMessage> m_mq;
    std::thread m_thread;
    
    Mutex *const m_voutput_mutex=nullptr;
    BeebThread2::VaryingOutput *const m_voutput=nullptr;
    
    std::atomic<bool> m_thread_running{false};
    
    void ThreadMain();
};

BeebThread2State::Message::~Message() {
}

BeebThread2State::BeebThread2State(Mutex *voutput_mutex,
                                   BeebThread2::VaryingOutput *voutput):
m_voutput_mutex(voutput_mutex),
m_voutput(voutput)
{
}

BeebThread2State::~BeebThread2State() {
}

bool BeebThread2State::StartThread() {
    ASSERT(false);
    
    try {
        m_thread=std::thread(std::bind(&BeebThread2State::ThreadMain,this));
    } catch(const std::system_error &) {
        return false;
    }
    
    // Poll for thread initiation.
    while(!m_thread_running.load(std::memory_order_acquire)) {
        SleepMS(1);
    }
    
    return false;
}

void BeebThread2State::StopThread() {
    ASSERT(false);
    if(m_thread.joinable()) {
        
    }
}

void BeebThread2State::Send(std::unique_ptr<Message> message) {
    m_mq.ProducerPush(SentMessage{std::move(message)});
}

void BeebThread2State::ThreadMain() {
    std::vector<SentMessage> messages;
    size_t total_num_audio_units_produced=0;
    
    for(;;) {
        
    }
}

BeebThread2::BeebThread2(std::shared_ptr<MessageList> message_list,
                         uint32_t sound_device_id,
                         int sound_freq,
                         size_t sound_buffer_size_samples):
m_state(nullptr)
{
}

BeebThread2::~BeebThread2() {
    this->Stop();
}

bool BeebThread2::Start() {
    return false;
}

void BeebThread2::Stop() {
    if(m_state) {
        m_state->StopThread();
    }
}

void BeebThread2::HandleSDLKeyEvent(const SDL_KeyboardEvent &event) {
    m_vinput.keyboard_events.push_back(event);
}

void BeebThread2::SetSDLMouseWheelState(int x,int y) {
    m_vinput.mouse_wheel_delta.x+=x;
    m_vinput.mouse_wheel_delta.y+=y;
}

void BeebThread2::HandleSDLTextInput(const char *text) {
    m_vinput.text_input.append(text);
}

BeebThread2::VaryingOutput BeebThread2::HandleVBlank(uint64_t ticks) {
    VaryingOutput voutput;
    {
        std::lock_guard<Mutex> lock(m_voutput_mutex);
        
        voutput=m_voutput;
    }
    
    ASSERT(false);
    //m_state->Send(std::make_unique<HandleVBlankMessage>(ticks,std::move(voutput)));
    return {};
}

//BeebThread2::VaryingOutput BeebThread2::GetVaryingOutput() {
//    return m_voutput;
//}
