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
//class BeebEvent;
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
//
// The official pointer type for a BeebThread message is shared_ptr<Message>,
// and the official data structure for the timeline is
// vector<shared_ptr<TimelineEvent>>. This is very wasteful, and could be
// improved. Maybe one day I'll get round to doing that.
//
// (Since Message objects are immutable once Prepare'd, messages that don't
// require a mutating Prepare step could at least be potentially pooled.)
//
// (Message doesn't derived from enable_shared_from_this.)

class BeebThread {
    struct ThreadState;
public:
    class Message {
    public:
        typedef std::function<void(bool,std::string)> CompletionFun;

//        // completion_fun is called when the message has been handled
//        // or when it gets cancelled. (This is used by the HTTP server
//        // to delay sending an HTTP response until the message has
//        // been handled.)
//        //
//        // The parameter indicates whether the message was handled
//        // (true) or cancelled (false).
//        //
//        // When the Message is destroyed, if completion_fun is set it
//        // will be called with the value of this->implicit_success
//        // (which is by default true). Alternatively, it can be called
//        // manually using CallCompletionFun.
//        //
//        // (Random notes: main other option here might be a virtual
//        // function, and the HTTP server could derive new message
//        // types that send a response on completion, perhaps using
//        // CRTP. But virtual functions are harder to call
//        // automatically from the destructor. And in some case there's
//        // extra overhead involved in working out when the message has
//        // been handled - e.g., when resetting - that could be avoided
//        // when unnecessary. Easy to test for with
//        // std::function<>::operator bool, but extra faff with a
//        // virtual function.)
//        //
//        // TODO move out of this class. The completion_fun should be
//        // associated with the Send, not the Message.
//        std::function<void(bool)> completion_fun;

        //bool implicit_success=true;

        explicit Message()=default;
        virtual ~Message()=0;

        static void CallCompletionFun(CompletionFun *completion_fun,
                                      bool success,
                                      const char *message);

//        // Calls completion fun (if set) and then resets it.
//        void CallCompletionFun(bool success);

        // Translate this, incoming message, into the message that will be
        // recorded into the timeline. *PTR points to this (and may be the
        // only pointer to it - exercise care when resetting).
        //
        // If this message isn't recordable, apply effect and do a PTR->reset().
        //
        // Otherwise, leave *PTR alone, or set *PTR to actual message to use,
        // which will (either way) cause that message to be ThreadHandle'd and
        // added to the timeline. It will be ThreadHandle'd again (but not
        // ThreadPrepare'd...) when replayed.
        //
        // COMPLETION_FUN, if non-null, points to the completion fun to be
        // called when the message completes for the first time. ('completion'
        // is rather vaguely defined, and is message-dependent.) Leave this
        // as it is to have the completion function called automatically: if
        // *PTR is non-null, with true (and no message) after the ThreadHandle
        // call returns, or with false (and no message) if *PTR is null. Move
        // it and handle in ThreadPrepare if special handling is necessary and/
        // or a message would be helpful.
        //
        // Default impl does nothing.
        //
        // Called on Beeb thread.
        virtual bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                                   CompletionFun *completion_fun,
                                   BeebThread *beeb_thread,
                                   ThreadState *ts);

        // Called on the Beeb thread.
        //
        // Default impl does nothing.
        virtual void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const;
    protected:
        // Standard policies for use from the ThreadPrepare function.

        // Returns false if ignored.
        //
        // The l
        static bool PrepareUnlessReplayingOrHalted(std::shared_ptr<Message> *ptr,
                                                   CompletionFun *completion_fun,
                                                   BeebThread *beeb_thread,
                                                   ThreadState *ts);

        // Returns true if ignored.
        static bool PrepareUnlessReplaying(std::shared_ptr<Message> *ptr,
                                           CompletionFun *completion_fun,
                                           BeebThread *beeb_thread,
                                           ThreadState *ts);
    private:
    };

    struct TimelineEvent {
        uint64_t time_2MHz_cycles;
        std::shared_ptr<Message> message;
    };

    class StopMessage:
    public Message
    {
    public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    class KeyMessage:
        public Message
    {
    public:
        KeyMessage(BeebKey key,bool state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
        const BeebKey m_key=BeebKey_None;
        const bool m_state=false;
    };

    class KeySymMessage:
        public Message
    {
    public:

        KeySymMessage(BeebKeySym key_sym,bool state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
        const BeebKeySym m_key_sym=BeebKeySym_None;
        const bool m_state=false;
        BeebKey m_key=BeebKey_None;
        BeebShiftState m_shift_state=BeebShiftState_Any;
    };

    class HardResetMessage:
        public Message
    {
    public:
//        const bool boot=false;

//#if BBCMICRO_DEBUGGER
//        // if set, set the emulator running after rebooting.
//        const bool run=false;
//#endif

        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetMessage(uint32_t flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
        const uint32_t m_flags=0;

        void HardReset(BeebThread *beeb_thread,
                       ThreadState *ts,
                       const BeebLoadedConfig &loaded_config) const;
    private:
    };

    class HardResetAndChangeConfigMessage:
    public HardResetMessage
    {
    public:
        const BeebLoadedConfig loaded_config;

        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetAndChangeConfigMessage(uint32_t flags,
                                                 BeebLoadedConfig loaded_config);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
    };

    class HardResetAndReloadConfigMessage:
    public HardResetMessage
    {
    public:
        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetAndReloadConfigMessage(uint32_t flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    class SetSpeedLimitingMessage:
        public Message
    {
    public:
        const bool limit_speed=false;

        explicit SetSpeedLimitingMessage(bool limit_speed);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    class LoadDiscMessage:
        public Message
    {
    public:
        const int drive=-1;

        // This is an owning pointer. If the disc image isn't cloneable, it'll
        // be given to the BBCMicro, and the LoadDiscMessage will always be
        // destroyed; if it is cloneable, a clone of it will be made, and the
        // clone given to the BBCMicro. (The LOadDiscMessage may or may not then
        // stick around, depending on whether there's a recording being made.)
        const std::shared_ptr<DiscImage> disc_image;
        const bool verbose=false;

        LoadDiscMessage(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
    };

    // Any kind of message that has a BeebState.
    class BeebStateMessage:
    public Message
    {
    public:
        explicit BeebStateMessage(std::shared_ptr<BeebState> state);

        const std::shared_ptr<const BeebState> &GetBeebState() const;
    protected:
    private:
        const std::shared_ptr<const BeebState> m_state;
    };

    struct TimelineBeebStateEvent {
        uint64_t time_2MHz_cycles;
        std::shared_ptr<BeebStateMessage> message;
    };

    class LoadStateMessage:
        public BeebStateMessage
    {
    public:
        explicit LoadStateMessage(std::shared_ptr<BeebState> state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
    };

    class SaveStateMessage:
        public Message
    {
    public:
        const bool verbose=false;

        explicit SaveStateMessage(bool verbose);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    class StartReplayMessage:
        public Message
    {
    public:
        explicit StartReplayMessage(size_t timeline_event_index);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
        const size_t m_timeline_event_index;
    };

    class StartRecordingMessage:
    public Message
    {
    public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    class StopRecordingMessage:
    public Message
    {
    public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    class ClearRecordingMessage:
    public Message
    {
    public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
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

#if BBCMICRO_TRACE
    class StartTraceMessage:
        public Message
    {
    public:
        const TraceConditions conditions;
        const size_t max_num_bytes;

        explicit StartTraceMessage(const TraceConditions &conditions,size_t max_num_bytes);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };
#endif

#if BBCMICRO_TRACE
    class StopTraceMessage:
        public Message
    {
    public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };
#endif

    class CloneWindowMessage:
        public Message
    {
    public:
        explicit CloneWindowMessage(BeebWindowInitArguments init_arguments);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
        const BeebWindowInitArguments m_init_arguments;
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
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

#if BBCMICRO_TURBO_DISC
    class SetTurboDiscMessage:
        public Message
    {
    public:
        const bool turbo=false;

        explicit SetTurboDiscMessage(bool turbo);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
    };
#endif

    class StartPasteMessage:
        public Message
    {
    public:
        explicit StartPasteMessage(std::string text);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
        std::shared_ptr<const std::string> m_text;
    };

    class StopPasteMessage:
        public Message
    {
    public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
    };

    class StartCopyMessage:
        public Message
    {
    public:
        StartCopyMessage(std::function<void(std::vector<uint8_t>)> stop_fun,bool basic);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
        std::function<void(std::vector<uint8_t>)> m_stop_fun;
        bool m_basic=false;
    };

    class StopCopyMessage:
        public Message
    {
    public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    // Wake thread up when emulator is being resumed. The thread could
    // have gone to sleep.
    class DebugWakeUpMessage:
        public Message
    {
    public:
    protected:
    private:
    };

    class PauseMessage:
        public Message
    {
    public:
        explicit PauseMessage(bool pause);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
        const bool m_pause=false;
    };

#if BBCMICRO_DEBUGGER
    class DebugSetByteMessage:
        public Message
    {
    public:
        DebugSetByteMessage(uint16_t addr,uint8_t value);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
        const uint16_t m_addr=0;
        const uint8_t m_value=0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetBytesMessage:
        public Message
    {
    public:
        DebugSetBytesMessage(uint32_t addr,std::vector<uint8_t> values);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
        const uint32_t m_addr=0;
        const std::vector<uint8_t> m_values;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetExtByteMessage:
    public Message
    {
    public:
        DebugSetExtByteMessage(uint32_t addr,uint8_t value);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
    protected:
    private:
        const uint32_t m_addr=0;
        const uint8_t m_value=0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugAsyncCallMessage:
        public Message
    {
    public:
        const uint16_t addr=0;
        const uint8_t a=0,x=0,y=0;
        const bool c=false;

        DebugAsyncCallMessage(uint16_t addr,uint8_t a,uint8_t x,uint8_t y,bool c);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,ThreadState *ts) const override;
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
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

        virtual void ThreadHandleMessage(BBCMicro *beeb)=0;
    protected:
    private:
    };

    class TimingMessage:
        public Message
    {
    public:
        const uint64_t max_sound_units=0;

        explicit TimingMessage(uint64_t max_sound_units);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
    protected:
    private:
    };

    // there's probably a few too many things called 'TimelineState' now...
    struct TimelineState {
        BeebThreadTimelineState state=BeebThreadTimelineState_None;
        uint64_t begin_2MHz_cycles=0;
        uint64_t end_2MHz_cycles=0;
        uint64_t current_2MHz_cycles=0;
        size_t num_events=0;
        size_t num_beeb_state_events=0;
        bool can_record=true;
        uint32_t non_cloneable_drives=0;
    };

    struct AudioCallbackRecord {
        uint64_t time=0;
        uint64_t needed=0;
        uint64_t available=0;
    };

    // When planning to set up the BeebThread using a saved state,
    // DEFAULT_LOADED_CONFIG may be default-constructed. In this case hard reset
    // messages and clone window messages won't work, though.
    explicit BeebThread(std::shared_ptr<MessageList> message_list,
                        uint32_t sound_device_id,
                        int sound_freq,
                        size_t sound_buffer_size_samples,
                        BeebLoadedConfig default_loaded_config,
                        std::vector<TimelineEvent> initial_timeline_events);
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
    void Send(std::shared_ptr<Message> message);
    void Send(std::shared_ptr<Message> message,Message::CompletionFun completion_fun);

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
    size_t AudioThreadFillAudioBuffer(float *samples,
                                      size_t num_samples,
                                      bool perfect,
                                      void (*fn)(int,float,void *)=nullptr,
                                      void *fn_context=nullptr);

    // Set sound/disc volume as attenuation in decibels.
    void SetBBCVolume(float db);
    void SetDiscVolume(float db);

    // Get info about the previous N audio callbacks.
    std::vector<AudioCallbackRecord> GetAudioCallbackRecords() const;

    //
    void GetTimelineState(TimelineState *timeline_state) const;
    void GetTimelineBeebStateEvents(std::vector<TimelineBeebStateEvent> *timeline_beeb_state_events,
                                    size_t begin_index,
                                    size_t end_index);
protected:
private:
    struct AudioThreadData;

    class KeyStates {
    public:
        bool GetState(BeebKey key) const;
        void SetState(BeebKey key,bool state);
    protected:
    private:
        std::atomic<uint64_t> m_flags[2]={};
    };

    struct SentMessage {
        std::shared_ptr<Message> message;
        Message::CompletionFun completion_fun;
    };

    // Initialisation-time stuff. Controlled by m_mutex, but it's not terribly
    // important as the thread just moves this stuff on initialisation.
    BeebLoadedConfig m_default_loaded_config;
    std::vector<TimelineEvent> m_initial_timeline_events;

    // Safe provided they are accessed through their functions.
    MessageQueue<SentMessage> m_mq;
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
    //std::atomic<bool> m_is_replaying{false};
#if BBCMICRO_TURBO_DISC
    // Whether turbo disc mode is active.
    std::atomic<bool> m_is_turbo_disc{false};
#endif
    std::atomic<bool> m_is_pasting{false};
    std::atomic<bool> m_is_copying{false};
    std::atomic<bool> m_has_nvram{false};

    // Controlled by m_mutex.
    TimelineState m_timeline_state;
    std::vector<TimelineBeebStateEvent> m_timeline_beeb_state_events_copy;

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

#if BBCMICRO_TRACE
    static bool ThreadStopTraceOnOSWORD0(const BBCMicro *beeb,const M6502 *cpu,void *context);
#endif
    static bool ThreadStopCopyOnOSWORD0(const BBCMicro *beeb,const M6502 *cpu,void *context);
    static bool ThreadAddCopyData(const BBCMicro *beeb,const M6502 *cpu,void *context);

    //void ThreadRecordEvent(ThreadState *ts,BeebEvent &&event);
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
    //void ThreadHandleEvent(ThreadState *ts,const BeebEvent &event,bool replay);
    //void ThreadStartReplay(ThreadState *ts,std::unique_ptr<Timeline> timeline);
//    void ThreadStopReplay(ThreadState *ts);
    void ThreadLoadState(ThreadState *ts,const std::shared_ptr<BeebState> &state);
//    void ThreadHandleReplayEvents(ThreadState *ts);
    //void ThreadHandleMessage(ThreadState *ts,std::shared_ptr<Message> message);
    void ThreadSetDiscImage(ThreadState *ts,int drive,std::shared_ptr<DiscImage> disc_image);
    void ThreadStartPaste(ThreadState *ts,std::shared_ptr<const std::string> text);
    void ThreadStopPaste(ThreadState *ts);
    void ThreadStopCopy(ThreadState *ts);
    void ThreadFailCompletionFun(std::function<void(bool)> *fun);
    void ThreadMain();
    void SetVolume(float *scale_var,float db);
//    bool ThreadIsReplayingOrHalted(ThreadState *ts);
    bool ThreadRecordSaveState(ThreadState *ts);
    void ThreadStopRecording(ThreadState *ts);
    void ThreadClearRecording(ThreadState *ts);
    void ThreadCheckTimeline(ThreadState *ts);

    static bool ThreadWaitForHardReset(const BBCMicro *beeb,const M6502 *cpu,void *context);
#if HTTP_SERVER
    static void DebugAsyncCallCallback(bool called,void *context);
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

