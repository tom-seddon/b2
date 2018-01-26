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

#include <shared/enum_decl.h>
#include "BeebThread.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MessageQueue;
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

class BeebThreadMessage {
public:
    BeebThreadMessageType type=BeebThreadMessageType_None;

    explicit BeebThreadMessage(BeebThreadMessageType type);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadKeyMessage:
    public BeebThreadMessage
{
public:
    BeebKey key=BeebKey_None;
    bool state=false;

    BeebThreadKeyMessage(BeebKey key,bool state);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadKeySymMessage:
    public BeebThreadMessage
{
public:
    BeebKeySym key_sym=BeebKeySym_None;
    bool state=false;

    BeebThreadKeySymMessage(BeebKeySym key_sym,bool state);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadHardResetMessage:
    public BeebThreadMessage
{
public:
    bool boot=false;

    explicit BeebThreadHardResetMessage(bool boot);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadChangeConfigMessage:
    public BeebThreadMessage
{
public:
    BeebLoadedConfig config;

    explicit BeebThreadChangeConfigMessage(BeebLoadedConfig config);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadSetSpeedLimitingMessage:
    public BeebThreadMessage
{
public:
    bool limit_speed=false;

    explicit BeebThreadSetSpeedLimitingMessage(bool limit_speed);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadLoadDiscMessage:
    public BeebThreadMessage
{
public:
    int drive=-1;
    std::shared_ptr<DiscImage> disc_image;
    bool verbose=false;

    BeebThreadLoadDiscMessage(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadLoadStateMessage:
    public BeebThreadMessage
{
public:
    uint64_t parent_timeline_id=0;
    std::shared_ptr<BeebState> state;

    BeebThreadLoadStateMessage(uint64_t parent_timeline_id,std::shared_ptr<BeebState> state);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadGoToTimelineNodeMessage:
    public BeebThreadMessage
{
public:
    uint64_t timeline_id=0;

    explicit BeebThreadGoToTimelineNodeMessage(uint64_t timeline_id);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadSaveStateMessage:
    public BeebThreadMessage
{
public:
    bool verbose=false;

    explicit BeebThreadSaveStateMessage(bool verbose);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadReplayMessage:
    public BeebThreadMessage
{
public:
    Timeline::ReplayData replay_data;

    explicit BeebThreadReplayMessage(Timeline::ReplayData replay_data);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadSaveAndReplayFromMessage:
    public BeebThreadMessage
{
public:
    uint64_t timeline_start_id=0;

    explicit BeebThreadSaveAndReplayFromMessage(uint64_t timeline_start_id);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadSaveAndVideoFromMessage:
    public BeebThreadMessage
{
public:
    uint64_t timeline_start_id=0;
    std::unique_ptr<VideoWriter> video_writer;

    BeebThreadSaveAndVideoFromMessage(uint64_t timeline_start_id,std::unique_ptr<VideoWriter> video_writer);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadSetDiscImageNameAndLoadMethodMessage:
    public BeebThreadMessage
{
public:
    int drive=-1;
    std::string name;
    std::string load_method;

    BeebThreadSetDiscImageNameAndLoadMethodMessage(int drive,std::string name,std::string load_method);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
class BeebThreadStartTraceMessage:
    public BeebThreadMessage
{
public:
    const TraceConditions conditions;

    explicit BeebThreadStartTraceMessage(const TraceConditions &conditions);
protected:
private:
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
class BeebThreadStopTraceMessage:
    public BeebThreadMessage
{
public:
    BeebThreadStopTraceMessage();
protected:
private:
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadCloneWindowMessage:
    public BeebThreadMessage
{
public:
    BeebWindowInitArguments init_arguments;

    explicit BeebThreadCloneWindowMessage(BeebWindowInitArguments init_arguments);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadCloneThisThreadMessage:
    public BeebThreadMessage
{
public:
    std::shared_ptr<BeebThread> dest_thread;

    explicit BeebThreadCloneThisThreadMessage(std::shared_ptr<BeebThread> dest_thread);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadLoadLastStateMessage:
    public BeebThreadMessage
{
public:
    BeebThreadLoadLastStateMessage();
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadCancelReplayMessage:
    public BeebThreadMessage
{
public:
    BeebThreadCancelReplayMessage();
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TURBO_DISC
class BeebThreadSetTurboDiscMessage:
    public BeebThreadMessage
{
public:
    bool turbo=false;

    explicit BeebThreadSetTurboDiscMessage(bool turbo);
protected:
private:
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadStartPasteMessage:
    public BeebThreadMessage
{
public:
    std::string text;

    explicit BeebThreadStartPasteMessage(std::string text);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadStopPasteMessage:
    public BeebThreadMessage
{
public:
    BeebThreadStopPasteMessage();
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadStartCopyMessage:
    public BeebThreadMessage
{
public:
    std::function<void(std::vector<uint8_t>)> stop_fun;
    bool basic=false;

    BeebThreadStartCopyMessage(std::function<void(std::vector<uint8_t>)> stop_fun,bool basic);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadStopCopyMessage:
    public BeebThreadMessage
{
public:
    BeebThreadStopCopyMessage();
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadDebugWakeUpMessage:
    public BeebThreadMessage
{
public:
    BeebThreadDebugWakeUpMessage();
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadPauseMessage:
    public BeebThreadMessage
{
public:
    bool pause=false;

    explicit BeebThreadPauseMessage(bool pause);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
class BeebThreadDebugSetByteMessage:
    public BeebThreadMessage
{
public:
    uint16_t addr=0;
    uint8_t value=0;

    BeebThreadDebugSetByteMessage(uint16_t addr,uint8_t value);
protected:
private:
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
class BeebThreadDebugSetBytesMessage:
    public BeebThreadMessage
{
public:
    uint32_t addr=0;
    std::vector<uint8_t> values;

    BeebThreadDebugSetBytesMessage(uint32_t addr,std::vector<uint8_t> values);
protected:
private:
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThread {
public:
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
    void Send(std::unique_ptr<BeebThreadMessage> message);

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

    // Get/set BBC volume as attenuation in decibels.
    float GetBBCVolume() const;
    void SetBBCVolume(float db);

    // Get/set disc volume as attenuation in decibels.
    float GetDiscVolume() const;
    void SetDiscVolume(float db);

    // Get info about the previous N audio callbacks.
    std::vector<AudioCallbackRecord> GetAudioCallbackRecords() const;

    uint64_t GetParentTimelineEventId() const;
    uint64_t GetLastSavedStateTimelineId() const;
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

    // Pointer to this thread's running BeebState, if it has one.
    //std::shared_ptr<BeebThreadBeebState> m_beeb_state;

    // Safe provided they are accessed through their functions.
    MessageQueue *m_mq=nullptr;
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
    std::atomic<bool> m_has_vram{false};

    //mutable volatile int32_t m_is_paused=1;

    mutable Mutex m_mutex;

    bool m_paused=true;

    // Main thread must take mutex to access.
    ThreadState *m_thread_state=nullptr;

    // use GetPreviousState/SetPreviousState to modify. Controlled by
    // the global timeline mutex.
    uint64_t m_parent_timeline_event_id=0;

    // Timeline id of last saved state.
    uint64_t m_last_saved_state_timeline_id=0;

    // Pre-replay state: original timeline event id, and saved state.
    uint64_t m_pre_replay_parent_timeline_event_id=0;
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
    int m_sound_freq=0;
    float m_bbc_sound_db;
    float m_disc_sound_db;

    //
    std::shared_ptr<MessageList> m_message_list;

#if BBCMICRO_DEBUGGER
    M6502 m_6502_state={};
#endif

#if BBCMICRO_TRACE
    static bool ThreadStopTraceOnOSWORD0(BBCMicro *beeb,M6502 *cpu,void *context);
#endif
    static bool ThreadStopCopyOnOSWORD0(BBCMicro *beeb,M6502 *cpu,void *context);
    static bool ThreadAddCopyData(BBCMicro *beeb,M6502 *cpu,void *context);

    void ThreadRecordEvent(ThreadState *ts,BeebEvent event);
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
    void ThreadStartReplay(ThreadState *ts,Timeline::ReplayData replay_data);
    void ThreadStopReplay(ThreadState *ts);
    void ThreadLoadState(ThreadState *ts,uint64_t parent_timeline_id,const std::shared_ptr<BeebState> &state);
    void ThreadHandleReplayEvents(ThreadState *ts);
    bool ThreadHandleMessage(ThreadState *ts,Message *msg,bool *limit_speed,uint64_t *next_stop_2MHz_cycles);
    void ThreadSetDiscImage(ThreadState *ts,int drive,std::shared_ptr<DiscImage> disc_image);
    void ThreadStartPaste(ThreadState *ts,std::string text);
    void ThreadStopPaste(ThreadState *ts);
    void ThreadStopCopy(ThreadState *ts);
    void ThreadMain();
    void SetVolume(float *scale_var,float *db_var,float db);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

