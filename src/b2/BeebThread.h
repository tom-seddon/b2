#ifndef HEADER_5AD5F02FDB734156B70BEEB05F66F6A2 // -*- mode:c++ -*-
#define HEADER_5AD5F02FDB734156B70BEEB05F66F6A2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include <shared/mutex.h>
#include <thread>
#include <beeb/sound.h>
#include <beeb/video.h>
#include <beeb/OutputData.h>
#include <beeb/conf.h>
#include <memory>
#include <vector>
#include <shared/mutex.h>
#include <beeb/Trace.h>
#include "misc.h"
#include "keys.h"
#include <beeb/DiscImage.h>
#include <beeb/BBCMicro.h>
#include <atomic>
#include "BeebConfig.h"
#include "Timeline.h"
#include "BeebWindow.h"
#include "MessageQueue.h"

#include <shared/enum_decl.h>
#include "BeebThread.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Message;
struct BeebWindowInitArguments;
class TVOutput;
struct Message;
class BeebState;
class MessageList;
class BeebEvent;
class VideoWriter;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
// This probably wants to go somewhere else and/or be more clever
// and/or have a better name...

struct TraceConditions {
    BeebThreadStartTraceCondition start=BeebThreadStartTraceCondition_Immediate;
    int8_t beeb_key=-1;

    BeebThreadStopTraceCondition stop=BeebThreadStopTraceCondition_ByRequest;

    uint32_t trace_flags=0;
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// BeebThread runs a BBCMicro object in a thread.
//
// The BBCMicro will run flat out for some period, or some smallish
// fraction of a second (a quarter, say, or a third...), whichever is
// sooner, and then stop until it receives a message.
//
// Use SendTimingMessage to send it a message controlling how long it
// runs for; the count is in total number of sound data units produced
// (please track based on unit consumed). UINT64_MAX is a good value
// when you don't care.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThread {
public:
    class Message {
    public:
        const BeebThreadMessageType type=BeebThreadMessageType_None;

        // completion_fun is called when the message has been handled
        // or when it gets cancelled. (This is used by the HTTP server
        // to delay sending an HTTP response until the message has
        // been handled.)
        //
        // The parameter indicates whether the message was handled
        // (true) or cancelled (false).
        //
        // When the Message is destroyed, if completion_fun is set it
        // will be called with the value of this->implicit_success
        // (which is by default true). Alternatively, it can be called
        // manually using CallCompletionFun.
        //
        // (Random notes: main other option here might be a virtual
        // function, and the HTTP server could derive new message
        // types that send a response on completion, perhaps using
        // CRTP. But virtual functions are harder to call
        // automatically from the destructor. And in some case there's
        // extra overhead involved in working out when the message has
        // been handled - e.g., when resetting - that could be avoided
        // when unnecessary. Easy to test for with
        // std::function<>::operator bool, but extra faff with a
        // virtual function.)
        std::function<void(bool)> completion_fun;

        bool implicit_success=true;

        explicit Message(BeebThreadMessageType type);
        virtual ~Message();

        // Calls completion fun (if set) and then resets it.
        void CallCompletionFun(bool success);
    protected:
    private:
    };

    class KeyMessage:
        public Message
    {
    public:
        BeebKey key=BeebKey_None;
        bool state=false;

        KeyMessage(BeebKey key,bool state);
    protected:
    private:
    };

    class KeySymMessage:
        public Message
    {
    public:
        BeebKeySym key_sym=BeebKeySym_None;
        bool state=false;

        KeySymMessage(BeebKeySym key_sym,bool state);
    protected:
    private:
    };

    class HardResetMessage:
        public Message
    {
    public:
        bool boot=false;

        // if non-null, use this as the new config.
        std::unique_ptr<BeebLoadedConfig> loaded_config;

        // if set, reload the existing config. (Not much use in
        // conjunction with loaded_config...)
        bool reload_config=false;

#if BBCMICRO_DEBUGGER
        // if set, set the emulator running after rebooting.
        bool run=false;
#endif

        explicit HardResetMessage();
    protected:
    private:
    };

    class SetSpeedLimitingMessage:
        public Message
    {
    public:
        bool limit_speed=false;

        explicit SetSpeedLimitingMessage(bool limit_speed);
    protected:
    private:
    };

    class LoadDiscMessage:
        public Message
    {
    public:
        int drive=-1;
        std::shared_ptr<DiscImage> disc_image;
        bool verbose=false;

        LoadDiscMessage(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose);
    protected:
    private:
    };

    class LoadStateMessage:
        public Message
    {
    public:
        std::shared_ptr<BeebState> state;

        LoadStateMessage(std::shared_ptr<BeebState> state);
    protected:
    private:
    };

    class GoToTimelineNodeMessage:
        public Message
    {
    public:
        uint64_t timeline_id=0;

        explicit GoToTimelineNodeMessage(uint64_t timeline_id);
    protected:
    private:
    };

    class SaveStateMessage:
        public Message
    {
    public:
        bool verbose=false;

        explicit SaveStateMessage(bool verbose);
    protected:
    private:
    };

    class ReplayMessage:
        public Message
    {
    public:
        std::unique_ptr<Timeline> timeline;

        explicit ReplayMessage(std::unique_ptr<Timeline> timeline);
    protected:
    private:
    };

//    class SaveAndReplayFromMessage:
//        public Message
//    {
//    public:
//        uint64_t timeline_start_id=0;
//
//        explicit SaveAndReplayFromMessage(uint64_t timeline_start_id);
//    protected:
//    private:
//    };

//    // Save current state, start recording video of replay from given
//    // state to new state.
//    //
//    // This doesn't affect the receiver - video recording runs as a
//    // job, and emulation continues.
//    class SaveAndVideoFromMessage:
//        public Message
//    {
//    public:
//        uint64_t timeline_start_id=0;
//        std::unique_ptr<VideoWriter> video_writer;
//
//        SaveAndVideoFromMessage(uint64_t timeline_start_id,std::unique_ptr<VideoWriter> video_writer);
//    protected:
//    private:
//    };

    // Set name and load method for a loaded disc image. (The disc
    // contents don't change.)
    class SetDiscImageNameAndLoadMethodMessage:
        public Message
    {
    public:
        int drive=-1;
        std::string name;
        std::string load_method;

        SetDiscImageNameAndLoadMethodMessage(int drive,std::string name,std::string load_method);
    protected:
    private:
    };

#if BBCMICRO_TRACE
    class StartTraceMessage:
        public Message
    {
    public:
        const TraceConditions conditions;
        const size_t max_num_bytes;

        explicit StartTraceMessage(const TraceConditions &conditions,size_t max_num_bytes);
    protected:
    private:
    };
#endif

#if BBCMICRO_TRACE
    class StopTraceMessage:
        public Message
    {
    public:
        StopTraceMessage();
    protected:
    private:
    };
#endif

    class CloneWindowMessage:
        public Message
    {
    public:
        BeebWindowInitArguments init_arguments;

        explicit CloneWindowMessage(BeebWindowInitArguments init_arguments);
    protected:
    private:
    };

//    // Clone self into existing window. Save this thread's state and
//    // load the resulting state into another thread. (Not sure "clone"
//    // is really the best name for that though...?)
//    class CloneThisThreadMessage:
//        public Message
//    {
//    public:
//        std::shared_ptr<BeebThread> dest_thread;
//
//        explicit CloneThisThreadMessage(std::shared_ptr<BeebThread> dest_thread);
//    protected:
//    private:
//    };

//    class LoadLastStateMessage:
//        public Message
//    {
//    public:
//        LoadLastStateMessage();
//    protected:
//    private:
//    };

    class CancelReplayMessage:
        public Message
    {
    public:
        CancelReplayMessage();
    protected:
    private:
    };

#if BBCMICRO_TURBO_DISC
    class SetTurboDiscMessage:
        public Message
    {
    public:
        bool turbo=false;

        explicit SetTurboDiscMessage(bool turbo);
    protected:
    private:
    };
#endif

    class StartPasteMessage:
        public Message
    {
    public:
        std::string text;

        explicit StartPasteMessage(std::string text);
    protected:
    private:
    };

    class StopPasteMessage:
        public Message
    {
    public:
        StopPasteMessage();
    protected:
    private:
    };

    class StartCopyMessage:
        public Message
    {
    public:
        std::function<void(std::vector<uint8_t>)> stop_fun;
        bool basic=false;

        StartCopyMessage(std::function<void(std::vector<uint8_t>)> stop_fun,bool basic);
    protected:
    private:
    };

    class StopCopyMessage:
        public Message
    {
    public:
        StopCopyMessage();
    protected:
    private:
    };

    // Wake thread up when emulator is being resumed. The thread could
    // have gone to sleep.
    class DebugWakeUpMessage:
        public Message
    {
    public:
        DebugWakeUpMessage();
    protected:
    private:
    };

    class PauseMessage:
        public Message
    {
    public:
        bool pause=false;

        explicit PauseMessage(bool pause);
    protected:
    private:
    };

#if BBCMICRO_DEBUGGER
    class DebugSetByteMessage:
        public Message
    {
    public:
        uint16_t addr=0;
        uint8_t value=0;

        DebugSetByteMessage(uint16_t addr,uint8_t value);
    protected:
    private:
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetBytesMessage:
        public Message
    {
    public:
        uint32_t addr=0;
        std::vector<uint8_t> values;

        DebugSetBytesMessage(uint32_t addr,std::vector<uint8_t> values);
    protected:
    private:
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetExtByteMessage:
            public Message
    {
    public:
        uint32_t addr=0;
        uint8_t value=0;

        DebugSetExtByteMessage(uint32_t addr,uint8_t value);
    protected:
    private:
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugAsyncCallMessage:
        public Message
    {
    public:
        uint16_t addr=0;
        uint8_t a=0,x=0,y=0;
        bool c=false;

        DebugAsyncCallMessage(uint16_t addr,uint8_t a,uint8_t x,uint8_t y,bool c);
    protected:
    private:
    };
#endif

    // Somewhat open-ended extension mechanism.
    //
    // This does the bare minimum needed for the HTTP stuff to hang
    // together.
    class CustomMessage:
        public Message
    {
    public:
        CustomMessage();

        virtual void ThreadHandleMessage(BBCMicro *beeb)=0;
    protected:
    private:
    };

    class TimingMessage:
        public Message
    {
    public:
        uint64_t max_sound_units=0;

        explicit TimingMessage(uint64_t max_sound_units);
    protected:
    private:
    };

    struct AudioCallbackRecord {
        uint64_t time=0;
        uint64_t needed=0;
        uint64_t available=0;
    };

    explicit BeebThread(std::shared_ptr<MessageList> message_list,uint32_t sound_device_id,int sound_freq,size_t sound_buffer_size_samples);
    ~BeebThread();

    // Start/stop the thread.
    bool Start();
    void Stop();

    // Returns true if Start was called and the thread is running.
    bool IsStarted() const;

    // Get number of 2MHz cycles elapsed. This value is for UI
    // purposes only - it's updated regularly, but it isn't
    // authoritative.
    uint64_t GetEmulated2MHzCycles() const;

    // The caller has to lock the buffer, read the data out, and send
    // it to the TVOutput. The (VideoWriter has to be able to spot the
    // vblank immediately, so it knows to write a new frame, so the
    // BeebThread can't do this itself.)
    OutputDataBuffer<VideoDataUnit> *GetVideoOutput();

    // Crap naming, because windows.h does #define SendMessage.
    void Send(std::unique_ptr<Message> message);

    template<class SeqIt>
    void Send(SeqIt begin,SeqIt end) {
        m_mq.ProducerPush(begin,end);
    }

    void SendTimingMessage(uint64_t max_sound_units);

#if BBCMICRO_TURBO_DISC
    bool IsTurboDisc() const;
#endif

    bool IsPasting() const;

    bool IsCopying() const;

#if BBCMICRO_DEBUGGER
    // It's safe to call any of the const BBCMicro public member
    // functions on the result as long as the lock is held.
    const BBCMicro *LockBeeb(std::unique_lock<Mutex> *lock) const;

    // As well as the LockBeeb guarantees, it's also safe to call the
    // non-const DebugXXX functions.
    BBCMicro *LockMutableBeeb(std::unique_lock<Mutex> *lock);
#endif

    // Get trace stats, or nullptr if there's no trace.
    const volatile TraceStats *GetTraceStats() const;

    // Get the speed limiting flag, as set by SendSetSpeedLimitingMessage.
    bool IsSpeedLimited() const;

    // Get number of events in the event log.
    //uint32_t GetNumSavedEvents() const;

    // Get debug flags, as set by SendDebugFlagsMessage.
    uint32_t GetDebugFlags() const;

    // Get pause state as set by SetPaused.
    bool IsPaused() const;

    // Get the disc image pointer for the given drive, using the given
    // lock object to take a lock on the disc access mutex.
    std::shared_ptr<const DiscImage> GetDiscImage(std::unique_lock<Mutex> *lock,int drive) const;

    // Get the current LED flags - a combination of BBCMicroLEDFlag values.
    uint32_t GetLEDs() const;

    // Get the given BBC key state.
    bool GetKeyState(BeebKey beeb_key) const;

    // Get/set NVRAM. 0 is the first byte of CMOS RAM/EEPROM (the RTC
    // data is not included) - so the values are indexed as per the
    // OSWORD calls.
    std::vector<uint8_t> GetNVRAM() const;

    // Returns true if the emulated computer has NVRAM.
    bool HasNVRAM() const;

    // Forget about the last recorded trace.
    void ClearLastTrace();

    // Get a shared_ptr to the last recorded trace, if there is one.
    std::shared_ptr<Trace> GetLastTrace();

    // Returns true if events are being replayed.
    bool IsReplaying() const;

    // Call to produce more audio and send timing messages to the
    // thread.
    //
    // When speed limiting is on: consume the correct amount of data
    // based on the playback rate, and tell the Beeb thread it can run
    // ahead far enough to produce that much data again. (The SDL
    // callback is always called for the same number of samples each
    // time.)
    //
    // If there's underflow: do a quick wait (~1ms) to try to help the
    // thread move forward, and check again. If still underflow: if
    // PERFECT, return; else, use whatever's there to fill the buffer
    // (even if it sounds crap).
    //
    // When speed limiting is off: consume all data available, tell
    // the thread it can run forward forever.
    //
    // Returns number of samples actually produced.
    //
    // (FN, if non-null, is called back with the float audio data
    // produced, at the sound chip rate of 250KHz... see the code.)
    size_t AudioThreadFillAudioBuffer(float *samples,size_t num_samples,bool perfect,void (*fn)(int,float,void *)=nullptr,void *fn_context=nullptr);

    // Set sound/disc volume as attenuation in decibels.
    void SetBBCVolume(float db);
    void SetDiscVolume(float db);

    // Get info about the previous N audio callbacks.
    std::vector<AudioCallbackRecord> GetAudioCallbackRecords() const;

//    uint64_t GetParentTimelineEventId() const;
//    uint64_t GetLastSavedStateTimelineId() const;
protected:
private:
    struct ThreadState;
    struct AudioThreadData;

    class KeyStates {
    public:
        bool GetState(BeebKey key) const;
        void SetState(BeebKey key,bool state);
    protected:
    private:
        std::atomic<uint64_t> m_flags[2]={};
    };

    // Safe provided they are accessed through their functions.
    MessageQueue<std::unique_ptr<Message>> m_mq;
    OutputDataBuffer<VideoDataUnit> m_video_output;
    OutputDataBuffer<SoundDataUnit> m_sound_output;
    KeyStates m_effective_key_states;//includes fake shift
    KeyStates m_real_key_states;//corresponds to PC keys pressed

    // Copies of the corresponding BBCMicro flags and/or other info
    // from the thread. These are updated atomically fairly regularly
    // so that the UI can query them.
    //
    // Safe provided they are updated atomically.
    std::atomic<uint64_t> m_num_2MHz_cycles{0};
    std::atomic<bool> m_limit_speed{true};
    std::atomic<uint32_t> m_leds{0};
#if BBCMICRO_TRACE
    std::atomic<bool> m_is_tracing{false};
#endif
    std::atomic<bool> m_is_replaying{false};
#if BBCMICRO_TURBO_DISC
    // Whether turbo disc mode is active.
    std::atomic<bool> m_is_turbo_disc{false};
#endif
    std::atomic<bool> m_is_pasting{false};
    std::atomic<bool> m_is_copying{false};
    std::atomic<bool> m_has_nvram{false};

    //mutable volatile int32_t m_is_paused=1;

    mutable Mutex m_mutex;

    bool m_paused=true;

    // Main thread must take mutex to access.
    ThreadState *m_thread_state=nullptr;

    // use GetPreviousState/SetPreviousState to modify. Controlled by
    // the global timeline mutex.
    //uint64_t m_parent_timeline_event_id=0;

    // Timeline id of last saved state.
    //uint64_t m_last_saved_state_timeline_id=0;

    // Pre-replay state: original timeline event id, and saved state.
    //uint64_t m_pre_replay_parent_timeline_event_id=0;
    std::shared_ptr<BeebState> m_pre_replay_state;

    // Last recorded trace. Controlled by m_mutex.
    std::shared_ptr<Trace> m_last_trace;

#if BBCMICRO_TRACE
    // Trace stats. Updated regularly when a trace is active. There's
    // no mutex for this... it's only for the UI, so the odd
    // inconsistency isn't a problem.
    TraceStats m_trace_stats;
#endif

    // Shadow copy of last known disc drive pointers.
    std::shared_ptr<const DiscImage> m_disc_images[NUM_DRIVES];

    // The thread.
    std::thread m_thread;

    // Audio thread data.
    AudioThreadData *m_audio_thread_data=nullptr;
    uint32_t m_sound_device_id=0;

    //
    std::shared_ptr<MessageList> m_message_list;

#if BBCMICRO_DEBUGGER
    M6502 m_6502_state={};
#endif

#if BBCMICRO_TRACE
    static bool ThreadStopTraceOnOSWORD0(const BBCMicro *beeb,const M6502 *cpu,void *context);
#endif
    static bool ThreadStopCopyOnOSWORD0(const BBCMicro *beeb,const M6502 *cpu,void *context);
    static bool ThreadAddCopyData(const BBCMicro *beeb,const M6502 *cpu,void *context);

    void ThreadRecordEvent(ThreadState *ts,BeebEvent &&event);
    std::shared_ptr<BeebState> ThreadSaveState(ThreadState *ts);
    void ThreadReplaceBeeb(ThreadState *ts,std::unique_ptr<BBCMicro> beeb,uint32_t flags);
#if BBCMICRO_TRACE
    void ThreadStartTrace(ThreadState *ts);
    void ThreadBeebStartTrace(ThreadState *ts);
    void ThreadStopTrace(ThreadState *ts);
#endif
    void ThreadSetKeyState(ThreadState *ts,BeebKey beeb_key,bool state);
    void ThreadSetFakeShiftState(ThreadState *ts,BeebShiftState state);
    void ThreadSetBootState(ThreadState *ts,bool state);
    void ThreadUpdateShiftKeyState(ThreadState *ts);
#if BBCMICRO_TURBO_DISC
    void ThreadSetTurboDisc(ThreadState *ts,bool turbo);
#endif
    void ThreadHandleEvent(ThreadState *ts,const BeebEvent &event,bool replay);
    void ThreadStartReplay(ThreadState *ts,std::unique_ptr<Timeline> timeline);
    void ThreadStopReplay(ThreadState *ts);
    void ThreadLoadState(ThreadState *ts,const std::shared_ptr<BeebState> &state);
    void ThreadHandleReplayEvents(ThreadState *ts);
    bool ThreadHandleMessage(ThreadState *ts,std::unique_ptr<Message> message,bool *limit_speed,uint64_t *next_stop_2MHz_cycles);
    void ThreadSetDiscImage(ThreadState *ts,int drive,std::shared_ptr<DiscImage> disc_image);
    void ThreadStartPaste(ThreadState *ts,std::string text);
    void ThreadStopPaste(ThreadState *ts);
    void ThreadStopCopy(ThreadState *ts);
    void ThreadFailCompletionFun(std::unique_ptr<Message> *message_ptr);
    void ThreadMain();
    void SetVolume(float *scale_var,float db);

    static bool ThreadWaitForHardReset(const BBCMicro *beeb,const M6502 *cpu,void *context);
#if HTTP_SERVER
    static void DebugAsyncCallCallback(bool called,void *context);
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

