#ifndef HEADER_5AD5F02FDB734156B70BEEB05F66F6A2 // -*- mode:c++ -*-
#define HEADER_5AD5F02FDB734156B70BEEB05F66F6A2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include <shared/mutex.h>
#include <thread>
#include <beeb/OutputData.h>
#include <memory>
#include <vector>
#include <beeb/Trace.h>
#include "keys.h"
#include <atomic>
#include "BeebConfig.h"
#include "MessageQueue.h"
#include "BeebWindow.h"
#include <beeb/BBCMicro.h>

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
class R6522;
struct SoundDataUnit;
struct VideoDataUnit;
class DiscImage;
class MetricSet;
class Counter;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
// This probably wants to go somewhere else and/or be more clever
// and/or have a better name...

struct TraceConditions {
    BeebThreadStartTraceCondition start = BeebThreadStartTraceCondition_Immediate;
    int8_t start_key = -1;
    uint16_t start_address = 0;

    BeebThreadStopTraceCondition stop = BeebThreadStopTraceCondition_ByRequest;
    CycleCount stop_num_cycles = {0};
    uint16_t stop_address = 0;

    uint32_t trace_flags = 0;
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
// and the official data structure for the timeline is vector<TimelineEvent>.
// This is very wasteful, and could be improved. Maybe one day I'll get round to
// doing that.
//
// (Since Message objects are immutable once Prepare'd, messages that don't
// require a mutating Prepare step could at least be potentially pooled.)
//
// (Message doesn't derived from enable_shared_from_this.)

struct BeebThreadTimelineState {
    BeebThreadTimelineMode mode = BeebThreadTimelineMode_None;
    CycleCount begin_cycles = {0};
    CycleCount end_cycles = {0};
    CycleCount current_cycles = {0};
    size_t num_events = 0;
    size_t num_beeb_state_events = 0;
    bool can_record = false;
    uint32_t clone_impediments = 0;
};

static_assert(sizeof(std::atomic<CycleCount>) == sizeof(CycleCount), "atomic<CycleCount> has overhead...");

// TODO: there's probably a more generic mechanism lurking in here somewhere.
class OSWORD0Callback {
  public:
    OSWORD0Callback() = default;
    virtual ~OSWORD0Callback() = default;

    // called on some arbitrary thread, on no particular schedule.
    //
    // Return true if it's clear the callback is no longer relevant. It will be removed.
    //
    // Default impl returns true.
    virtual bool ThreadIsStillRelevant() const;

    // called on some arbitrary thread.
    //
    // Return true to leave the callback in place, or false to have it removed automatically.
    [[nodiscard]] virtual bool ThreadOnOSWORD0(BeebThread *beeb_thread) = 0;

  protected:
  private:
};

class OSWRCHCallback {
  public:
    OSWRCHCallback() = default;
    virtual ~OSWRCHCallback() = default;

    // called on some arbitrary thread.
    //
    // Return true to leave the callback in place, or false to have it removed automatically.
    [[nodiscard]] virtual bool ThreadOnOSWRCH(BeebThread *beeb_thread, uint8_t a) = 0;

    // called on some arbitrary thread, on no particular schedule.
    //
    // Return true if it's clear the callback is no longer relevant. It will be removed.
    //
    // Default impl returns true.
    //
    // TODO: is this actually worth having?
    virtual bool ThreadIsStillRelevant() const;

    // callback was removed.
    //
    // Default impl does nothing.
    virtual void ThreadCallbackWasRemoved(bool success);

  protected:
  private:
};

class BeebThread {
    struct ThreadState;

  public:
    class Message {
      public:
        // TODO: the std::string is a bit inconvenient. This mechanism could use some improvement.
        typedef std::function<void(bool, std::string)> CompletionFun;

        explicit Message() = default;
        virtual ~Message() = 0;

        // TODO: too many overloads!
        static void CallCompletionFun(CompletionFun &&completion_fun,
                                      bool success,
                                      const char *message);

        static void CallCompletionFun(CompletionFun &&completion_fun,
                                      bool success,
                                      std::string message);

        // no-ops when !completion_fun.
        static void CallCompletionFun(CompletionFun *completion_fun,
                                      bool success,
                                      const char *message);

        static void CallCompletionFun(CompletionFun *completion_fun,
                                      bool success,
                                      std::string message);

        // Called on Beeb thread with m_mutex locked.
        //
        // Translate this, incoming message, into the message that will be
        // recorded into the timeline. *PTR points to this (and may be the only
        // pointer to it - exercise care when resetting).
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
        // is rather vaguely defined, and is message-dependent.) Leave this as
        // it is to have the completion function called automatically when ThreadHandle returns, or move
        // its contents for later calling to do it manually.
        //
        // Default impl does nothing and returns true (completion_fun will be
        // called straight away with no message).
        //
        // Return false to reject the message. The completion_fun will be called
        // with false, and the message will be discarded.
        virtual bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                                   CompletionFun *completion_fun,
                                   ThreadState *ts);

        // Called on the Beeb thread with m_mutex locked.
        //
        // completion_fun may be null.
        //
        // Default impl does nothing.
        virtual void ThreadHandle(CompletionFun *completion_fun,
                                  ThreadState *ts) const;

      protected:
        // Standard policies for use from the ThreadPrepare function.

        // Returns false if ignored.
        static bool PrepareUnlessReplayingOrHalted(std::shared_ptr<Message> *ptr,
                                                   CompletionFun *completion_fun,
                                                   ThreadState *ts);

        // Returns true if ignored.
        static bool PrepareUnlessReplaying(std::shared_ptr<Message> *ptr,
                                           CompletionFun *completion_fun,
                                           ThreadState *ts);

      private:
    };

    struct TimelineEvent {
        CycleCount time_cycles;
        std::shared_ptr<Message> message;
    };

    class BeebStateMessage;

    struct TimelineBeebStateEvent {
        CycleCount time_cycles;
        std::shared_ptr<BeebStateMessage> message;
    };

    // Holds a starting state event, and a list of subsequent non-state events.
    struct TimelineEventList {
        TimelineBeebStateEvent state_event;
        std::vector<TimelineEvent> events;

        TimelineEventList() = default;

        // Moving is fine. Copying isn't!
        TimelineEventList(const TimelineEventList &) = delete;
        TimelineEventList &operator=(const TimelineEventList &) = delete;
        TimelineEventList(TimelineEventList &&) = default;
        TimelineEventList &operator=(TimelineEventList &&) = default;
    };

    class StopMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class KeyMessage : public Message {
      public:
        KeyMessage(BeebKey key, bool state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const BeebKey m_key = BeebKey_None;
        const bool m_state = false;
    };

    class KeySymMessage : public Message {
      public:
        KeySymMessage(BeebKeySym key_sym, bool state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const bool m_state = false;
        const KeySymKeyCombos *m_key_combos = nullptr;
    };

    class AllKeysUpMessage : public Message {
      public:
        AllKeysUpMessage() = default;

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        bool m_shift_up = true;
    };

    class JoystickButtonMessage : public Message {
      public:
        JoystickButtonMessage(uint8_t index, bool state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const uint8_t m_index = 0;
        const bool m_state = false;
    };

    class AnalogueChannelMessage : public Message {
      public:
        explicit AnalogueChannelMessage(uint8_t index, uint16_t value);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const uint8_t m_index = 0;
        const uint16_t m_value = 0;
    };

    class DigitalJoystickStateMessage : public Message {
      public:
        explicit DigitalJoystickStateMessage(uint8_t index, BBCMicroState::DigitalJoystickInput state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const uint8_t m_index = 0;
        const BBCMicroState::DigitalJoystickInput m_state = {};
    };

    // Completion criteria:
    //
    // Fail if replaying or halted.
    //
    // If OSWORD 0 flag specified: succeed when OSWORD 0 first called, or (when applicable) fail if OSWORD 0 not called within the timeout.
    //
    // If OSWORD 0 flag not specified: succeed when BBC reset with the new config.
    class HardResetMessage : public Message {
      public:
        static constexpr double DEFAULT_OSWORD_0_TIMEOUT_SECONDS = 0.;

        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetMessage(uint32_t flags,
                                  double osword_0_timeout_seconds = DEFAULT_OSWORD_0_TIMEOUT_SECONDS);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override = 0;

      protected:
        const uint32_t m_flags = 0;

        // How long to wait for the first OSWORD 0, if the WaitForOSWORD0 flag is specified.
        //
        // If 0, wait indefinitely.
        const double m_osword_0_timeout_seconds = 0.;

        void HardReset(CompletionFun *completion_fun,
                       ThreadState *ts,
                       const BeebLoadedConfig &loaded_config,
                       const std::vector<uint8_t> &nvram_contents) const;

      private:
    };

    class HardResetAndChangeConfigMessage : public HardResetMessage {
      public:
        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetAndChangeConfigMessage(BeebLoadedConfig loaded_config,
                                                 uint32_t flags,
                                                 double osword_0_timeout_seconds = DEFAULT_OSWORD_0_TIMEOUT_SECONDS);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const BeebLoadedConfig m_loaded_config;
        const std::vector<uint8_t> m_nvram_contents;
    };

    class HardResetAndReloadConfigMessage : public HardResetMessage {
      public:
        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetAndReloadConfigMessage(uint32_t flags,
                                                 double osword_0_timeout_seconds = DEFAULT_OSWORD_0_TIMEOUT_SECONDS);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
    };

    class SetSpeedLimitedMessage : public Message {
      public:
        explicit SetSpeedLimitedMessage(bool limited);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        const bool m_limited = false;
    };

    class SetSpeedScaleMessage : public Message {
      public:
        explicit SetSpeedScaleMessage(float scale);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        const float m_scale = 1.f;
    };

    class LoadDiscMessage : public Message {
      public:
        LoadDiscMessage(int drive, std::shared_ptr<DiscImage> disc_image, bool verbose);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const int m_drive = -1;
        // This is an owning pointer. If the disc image isn't cloneable, it'll
        // be given to the BBCMicro, and the LoadDiscMessage will always be
        // destroyed; if it is cloneable, a clone of it will be made, and the
        // clone given to the BBCMicro. (The LoadDiscMessage may or may not then
        // stick around, depending on whether there's a recording being made.)
        const std::shared_ptr<DiscImage> m_disc_image;
        const bool m_verbose = false;
    };

    class EjectDiscMessage : public Message {
      public:
        explicit EjectDiscMessage(int drive);

        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const int m_drive = -1;
    };

#if ENABLE_TAPE
    class LoadTapeMessage : public Message {
      public:
        LoadTapeMessage(std::shared_ptr<const UEFReader> tape);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(ThreadState *ts) const override;

      protected:
      private:
        std::shared_ptr<const UEFReader> m_tape;
    };
#endif

#if ENABLE_TAPE
    class EjectTapeMessage : public Message {
      public:
        EjectTapeMessage() = default;

        void ThreadHandle(ThreadState *ts) const override;

      protected:
      private:
    };
#endif

    class SetDriveWriteProtectedMessage : public Message {
      public:
        explicit SetDriveWriteProtectedMessage(int drive, bool is_write_protected);

        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const int m_drive = -1;
        const bool m_is_write_protected = false;
    };

    // Any kind of message that has a BeebState.
    class BeebStateMessage : public Message {
      public:
        BeebStateMessage() = delete;
        explicit BeebStateMessage(std::shared_ptr<const BeebState> state,
                                  bool user_initiated);

        const std::shared_ptr<const BeebState> &GetBeebState() const;

        // true if this state was created due to user interaction, rather than
        // something that happened behind the scenes.
        bool WasUserInitiated() const;

        // This is a no-op. Whether saving or replaying, the current state is
        // the same. (At least... in principle. Maybe a check would be
        // worthwhile.)
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        std::shared_ptr<const BeebState> m_state;
        const bool m_user_initiated;
    };

    // Load a random state from the saved states list.
    class LoadStateMessage : public BeebStateMessage {
      public:
        explicit LoadStateMessage(std::shared_ptr<const BeebState> state,
                                  bool verbose);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        bool m_verbose = false;
    };

    // Load a state from the timeline states list. When recording, the timeline
    // is rewound to that point.
    class LoadTimelineStateMessage : public BeebStateMessage {
      public:
        explicit LoadTimelineStateMessage(std::shared_ptr<const BeebState> state,
                                          bool verbose);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        bool m_verbose = false;
    };

    class DeleteTimelineStateMessage : public BeebStateMessage {
      public:
        explicit DeleteTimelineStateMessage(std::shared_ptr<const BeebState> state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class SaveStateMessage : public Message {
      public:
        explicit SaveStateMessage(bool verbose);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        bool m_verbose = false;
    };

    class StartReplayMessage : public Message {
      public:
        explicit StartReplayMessage(std::shared_ptr<const BeebState> start_state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        std::shared_ptr<const BeebState> m_start_state;
    };

    class StopReplayMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class StartRecordingMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class StopRecordingMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class ClearRecordingMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

#if BBCMICRO_TRACE
    class StartTraceMessage : public Message {
      public:
        explicit StartTraceMessage(const TraceConditions &conditions, size_t max_num_bytes);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        const TraceConditions m_conditions;
        const size_t m_max_num_bytes;
    };
#endif

#if BBCMICRO_TRACE
    class StopTraceMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };
#endif

#if BBCMICRO_TRACE
    class CancelTraceMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };
#endif

    class CloneWindowMessage : public Message {
      public:
        explicit CloneWindowMessage(BeebWindowInitArguments init_arguments);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        const BeebWindowInitArguments m_init_arguments;
    };

    class StartPasteMessage : public Message {
      public:
        static constexpr double DEFAULT_OSWORD_0_TIMEOUT_SECONDS = 0.;

        // flags are a combination of BeebThreadPasteFlag
        explicit StartPasteMessage(std::vector<uint8_t> text,
                                   uint32_t flags,
                                   double osword_0_timeout_seconds = DEFAULT_OSWORD_0_TIMEOUT_SECONDS);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        std::vector<uint8_t> m_text;
        uint32_t m_flags = 0;
        double m_osword_0_timeout_seconds = DEFAULT_OSWORD_0_TIMEOUT_SECONDS;
    };

    class StopPasteMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
    };

#if BBCMICRO_DEBUGGER
    class DebugSetByteMessage : public Message {
      public:
        DebugSetByteMessage(uint16_t addr, uint32_t dso, bool mos, uint8_t value);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const uint16_t m_addr = 0;
        const uint32_t m_dso = 0;
        const bool m_mos = false;
        const uint8_t m_value = 0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetBytesMessage : public Message {
      public:
        DebugSetBytesMessage(uint16_t addr, uint32_t dso, bool mos, std::vector<uint8_t> values);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const uint16_t m_addr = 0;
        const uint32_t m_dso = 0;
        const bool m_mos = false;
        const std::vector<uint8_t> m_values;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetExtByteMessage : public Message {
      public:
        DebugSetExtByteMessage(uint32_t addr, uint8_t value);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const uint32_t m_addr = 0;
        const uint8_t m_value = 0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugModifyAddressDebugFlags : public Message {
      public:
        DebugModifyAddressDebugFlags(M6502Word addr, uint32_t dso, uint8_t clear_flags, uint8_t set_flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        const M6502Word m_addr = {};
        const uint32_t m_dso = 0;
        const uint8_t m_clear_flags = 0;
        const uint8_t m_set_flags = 0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetAddressDebugFlags : public DebugModifyAddressDebugFlags {
      public:
        DebugSetAddressDebugFlags(M6502Word addr, uint32_t dso, uint8_t flags);

      protected:
      private:
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugModifyByteDebugFlags : public Message {
      public:
        DebugModifyByteDebugFlags(BigPageIndex big_page_index, uint16_t offset, uint8_t clear_flags, uint8_t set_flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        const BigPageIndex m_big_page_index = {0};
        const uint16_t m_offset = 0;
        const uint8_t m_clear_flags = 0;
        const uint8_t m_set_flags = 0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetByteDebugFlags : public DebugModifyByteDebugFlags {
      public:
        DebugSetByteDebugFlags(BigPageIndex big_page_index, uint16_t offset, uint8_t flags);

      protected:
      private:
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugClearBreakpoints : public Message {
      public:
        DebugClearBreakpoints() = default;

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };
#endif

    class CreateTimelineVideoMessage : public Message {
      public:
        CreateTimelineVideoMessage(std::shared_ptr<const BeebState> state,
                                   std::unique_ptr<VideoWriter> video_writer);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        std::shared_ptr<const BeebState> m_state;
        std::unique_ptr<VideoWriter> m_video_writer;
    };

    // Extension mechanism. The supplied callback is called on the BeebThread
    // once, when the message is prepared.
    class CallbackMessage : public Message {
      public:
        explicit CallbackMessage(std::function<void(BBCMicro *)> prepare_callback);
        explicit CallbackMessage(std::function<void(BBCMicro *)> prepare_callback, std::function<void(BBCMicro *)> replay_callback);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        std::function<void(BBCMicro *)> m_prepare_callback;
        std::function<void(BBCMicro *)> m_replay_callback;
    };

    class TimingMessage : public Message {
      public:
        explicit TimingMessage(uint64_t max_sound_units);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        const uint64_t m_max_sound_units = 0;
    };

    class BeebLinkResponseMessage : public Message {
      public:
        explicit BeebLinkResponseMessage(std::vector<uint8_t> data);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        std::vector<uint8_t> m_data;
    };

    class SetPrinterEnabledMessage : public Message {
      public:
        explicit SetPrinterEnabledMessage(bool enabled);

        //bool ThreadPrepare(std::shared_ptr<Message> *ptr,
        //                   CompletionFun *completion_fun,
        //
        //                   ThreadState *ts) override;
        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const bool m_enabled = false;
    };

    class ResetPrinterBufferMessage : public Message {
      public:
        explicit ResetPrinterBufferMessage();

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class MouseMotionMessage : public Message {
      public:
        explicit MouseMotionMessage(int dx, int dy);

        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const int m_dx = 0;
        const int m_dy = 0;
    };

    class MouseButtonsMessage : public Message {
      public:
        explicit MouseButtonsMessage(uint8_t mask, uint8_t value);

        void ThreadHandle(CompletionFun *completion_fun,
                          ThreadState *ts) const override;

      protected:
      private:
        const uint8_t m_mask = 0;
        const uint8_t m_value = 0;
    };

    class MainThreadIsReadyMessage : public Message {
      public:
        MainThreadIsReadyMessage() = default;

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class AddOSWORD0CallbackMessage : public Message {
      public:
        explicit AddOSWORD0CallbackMessage(std::shared_ptr<OSWORD0Callback> callback);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        std::shared_ptr<OSWORD0Callback> m_callback;
    };

    class RemoveOSWORD0CallbackMessage : public Message {
      public:
        explicit RemoveOSWORD0CallbackMessage(std::shared_ptr<OSWORD0Callback> callback);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        std::shared_ptr<OSWORD0Callback> m_callback;
    };

    class AddOSWRCHCallbackMessage : public Message {
      public:
        explicit AddOSWRCHCallbackMessage(std::shared_ptr<OSWRCHCallback> callback);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        std::shared_ptr<OSWRCHCallback> m_callback;
    };

    class RemoveOSWRCHCallbackMessage : public Message {
      public:
        explicit RemoveOSWRCHCallbackMessage(std::shared_ptr<OSWRCHCallback> callback);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           ThreadState *ts) override;

      protected:
      private:
        std::shared_ptr<OSWRCHCallback> m_callback;
    };

    struct AudioCallbackRecord {
        uint64_t time = 0;
        uint64_t needed = 0;
        uint64_t available = 0;
    };

    // When planning to set up the BeebThread using a saved state,
    // DEFAULT_LOADED_CONFIG may be default-constructed. In this case hard reset
    // messages and clone window messages won't work, though.
    explicit BeebThread(std::shared_ptr<MessageList> message_list,
                        std::shared_ptr<MetricSet> metric_set,
                        uint32_t sound_device_id,
                        int sound_freq,
                        size_t sound_buffer_size_samples,
                        BeebLoadedConfig default_loaded_config,
                        std::vector<TimelineEventList> initial_timeline_event_lists,
                        bool is_main_thread_ready);
    ~BeebThread();

    // Start/stop the thread.
    bool Start();
    void Stop();

    // Returns true if Start was called and the thread is running.
    bool IsStarted() const;

    // Get number of cycles elapsed. This value is for UI purposes only - it's
    // updated regularly, but it isn't authoritative.
    CycleCount GetEmulatedCycles() const;

    // The caller has to lock the buffer, read the data out, and send
    // it to the TVOutput. The (VideoWriter has to be able to spot the
    // vblank immediately, so it knows to write a new frame, so the
    // BeebThread can't do this itself.)
    OutputDataBuffer<VideoDataUnit> *GetVideoOutput();

    // Crap naming, because windows.h does #define SendMessage.
    void Send(std::shared_ptr<Message> message);
    void Send(std::shared_ptr<Message> message, Message::CompletionFun completion_fun);

    template <class SeqIt>
    void Send(SeqIt begin, SeqIt end) {
        m_mq.ProducerPush(begin, end);
    }

    void SendTimingMessage(uint64_t max_sound_units);

    bool AreNonTimingMessagesPending() const;

    bool IsPasting() const;

    //bool IsCopying() const;

    // Get trace stats, or nullptr if there's no trace.
    const volatile TraceStats *GetTraceStats() const;

    bool IsSpeedLimited() const;

    // Get the speed scale.
    float GetSpeedScale() const;

    std::shared_ptr<const UEFReader> GetTape() const;

    // Get the disc image pointer for the given drive, using the given
    // lock object to take a lock on the disc access mutex.
    std::shared_ptr<const DiscImage> GetDiscImage(UniqueLock<Mutex> *lock, int drive) const;

    // Get the current LED flags - a combination of BBCMicroLEDFlag values.
    uint32_t GetLEDs();

    // Get the given BBC key state.
    bool GetKeyState(BeebKey beeb_key) const;

    // Get/set NVRAM. 0 is the first byte of CMOS RAM/EEPROM (the RTC
    // data is not included) - so the values are indexed as per the
    // OSWORD calls.
    std::vector<uint8_t> GetNVRAM() const;

    std::shared_ptr<const BBCMicroType> GetBBCMicroType() const;
    BBCMicroTypeID GetBBCMicroTypeID() const;

    uint32_t GetBBCMicroCloneImpediments() const;

    // Forget about the last recorded trace.
    void ClearLastTrace();

    // Get a shared_ptr to the last recorded trace, if there is one.
    std::shared_ptr<Trace> GetLastTrace();

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
    size_t AudioThreadFillAudioBuffer(float *samples,
                                      size_t num_samples,
                                      bool perfect);

    // Set sound/disc volume as attenuation in decibels.
    void SetBBCVolume(float db, bool mute);
    void SetDiscVolume(float db, bool mute);

    void SetPowerOnTone(bool power_on_tone);

    void SetLowPassFilter(bool low_pass_filter);
    void SetLowPassFilterCutoff(int hz);

    // Get info about the previous N audio callbacks.
    std::vector<AudioCallbackRecord> GetAudioCallbackRecords() const;

    void GetTimelineState(BeebThreadTimelineState *timeline_state) const;

    // Got total number of events on the timeline.
    size_t GetNumTimelineBeebStateEvents() const;

    // May return fewer items than requested, if the indexes (presumably
    // calculated from the TimelineState values...) turn out to be outdated.
    std::vector<BeebThread::TimelineBeebStateEvent> GetTimelineBeebStateEvents(size_t begin_index,
                                                                               size_t end_index);

    bool IsDriveWriteProtected(int drive) const;

    bool IsParallelPrinterEnabled() const;

    size_t GetPrinterDataSizeBytes() const;

    std::vector<uint8_t> GetPrinterData() const;

#if BBCMICRO_DEBUGGER
    BBCMicroHaltReason DebugGetHaltReason() const;
    void DebugGetState(std::shared_ptr<const BBCMicroReadOnlyState> *state_ptr, std::shared_ptr<const BBCMicroDebugState> *debug_state_ptr) const;
#endif

    uint32_t GetUpdateFlags() const;

    bool HasMouse() const;

#if BBCMICRO_DEBUGGER
    std::shared_ptr<const BBCMicro::UpdateMFnData> GetUpdateMFnData() const;
#endif

    bool GetShowCursor() const;
    void SetShowCursor(bool show_cursor);

    void GetConfig(std::string *config_name, BeebConfig *config, BeebConfigArguments *config_arguments) const;

    bool TakeNVRAMChanged();

  protected:
  private:
    struct AudioThreadData;

    class KeyStates {
      public:
        bool GetState(BeebKey key) const;
        void SetState(BeebKey key, bool state);

      protected:
      private:
        std::atomic<uint64_t> m_flags[2] = {};
    };

    struct SentMessage {
        std::shared_ptr<Message> message;
        Message::CompletionFun completion_fun;
    };

    const uint64_t m_uid = 0;
    const bool m_is_main_thread_ready = false; //initial value for the thread's corresponding flag

    // Initialisation-time stuff. Controlled by m_mutex, but it's not terribly
    // important as the thread just moves this stuff on initialisation.
    BeebLoadedConfig m_default_loaded_config;
    std::vector<TimelineEventList> m_initial_timeline_event_lists;

    // Safe provided they are accessed through their functions.
    MessageQueue<SentMessage> m_mq;
    OutputDataBuffer<VideoDataUnit> m_video_output;
    OutputDataBuffer<SoundDataUnit> m_sound_output;
    KeyStates m_effective_key_states; //includes fake shift
    KeyStates m_real_key_states;      //corresponds to PC keys pressed

    // Copies of the corresponding BBCMicro flags and/or other info
    // from the thread. These are updated atomically fairly regularly
    // so that the UI can query them.
    //
    // Safe provided they are updated atomically.
    std::atomic<CycleCount> m_num_cycles{};
    std::atomic<bool> m_is_speed_limited{true};
    std::atomic<float> m_speed_scale{1.0};
    std::atomic<uint32_t> m_leds{0};
#if BBCMICRO_TRACE
    std::atomic<bool> m_is_tracing{false};
#endif
    std::atomic<bool> m_is_pasting{false};
    std::atomic<bool> m_is_copying{false};
    std::atomic<BBCMicroTypeID> m_beeb_type_id{BBCMicroTypeID_B};
    std::atomic<uint32_t> m_clone_impediments{0};
    std::atomic<bool> m_is_drive_write_protected[NUM_DRIVES]{};
    std::atomic<bool> m_is_printer_enabled{false};
    std::atomic<size_t> m_printer_data_size_bytes{false};
    std::atomic<BBCMicroHaltReason> m_debug_halt_reason{BBCMicroHaltReason_None};
    std::atomic<uint32_t> m_update_flags{0};

    // Set if NVRAM changes. Query using TakeNVRAMChanged, which does an atomic
    // swap with false. (The way b2 is arranged, it's just a lot simpler to
    // query this by polling...)
    std::atomic<bool> m_nvram_changed{false};

    uint32_t m_last_leds = 0;

    // Stuff the BeebThread reads to update the BBCMicro at appropriate times.
    std::atomic<bool> m_show_cursor{true};
    bool m_power_on_tone = true;

#if BBCMICRO_DEBUGGER
    // Lock m_mutex first, if locking both. (The public API makes this hard to
    // get wrong.)
    mutable Mutex m_beeb_state_mutex;
    std::shared_ptr<const BBCMicroReadOnlyState> m_beeb_state;
    std::shared_ptr<const BBCMicroDebugState> m_beeb_debug_state;
    std::shared_ptr<const BBCMicro::UpdateMFnData> m_update_mfn_data;
#endif

    // Lock m_mutex first, if locking both. (The public API makes this hard to
    // get wrong.)
    mutable Mutex m_timeline_state_mutex;
    BeebThreadTimelineState m_timeline_state;

    // Controlled by m_mutex.
    std::vector<TimelineBeebStateEvent> m_timeline_beeb_state_events_copy;

    // Controlled by m_mutex.
    std::shared_ptr<const BBCMicroType> m_bbc_micro_type;

    mutable Mutex m_mutex;

    // Controlled by m_mutex.
    BeebConfig m_config;
    BeebConfigArguments m_config_arguments;

    // Main thread must take mutex to access.
    ThreadState *m_thread_state = nullptr;

    // Lock m_mutex first, if locking both. (The public API makes this hard to
    // get wrong.)
    Mutex m_last_trace_mutex;

    // Last recorded trace. Controlled by m_last_trace_mutex.
    std::shared_ptr<Trace> m_last_trace;

    // Has its own mutex internally.
    PrinterBuffer m_printer_buffer;

#if BBCMICRO_TRACE
    // Trace stats. Updated regularly when a trace is active. There's
    // no mutex for this... it's only for the UI, so the odd
    // inconsistency isn't a problem.
    TraceStats m_trace_stats;
#endif

    // Shadow copies of disks and tapes.
    std::shared_ptr<const UEFReader> m_tape;
    std::shared_ptr<const DiscImage> m_disc_images[NUM_DRIVES];

    // The thread.
    std::thread m_thread;

    // Audio thread data.
    AudioThreadData *m_audio_thread_data = nullptr;
    uint32_t m_sound_device_id = 0;

    //
    std::shared_ptr<MessageList> m_message_list;
    std::shared_ptr<MetricSet> m_metric_set;

    Counter *m_mq_polls_counter = nullptr, *m_mq_waits_counter = nullptr;

#if BBCMICRO_TRACE
    static bool
    ThreadHandleTraceInstructionConditions(const BBCMicro *beeb, const M6502 *cpu, void *context);
    static bool ThreadHandleTraceWriteConditions(const BBCMicro *beeb, const M6502 *cpu, void *context);
#endif

    static bool ThreadHandleOSWORD0Callbacks(const BBCMicro *beeb, const M6502 *cpu, void *context);
    static bool ThreadHandleOSWRCHCallbacks(const BBCMicro *beeb, const M6502 *cpu, void *context);

    std::shared_ptr<BeebState> ThreadSaveState(ThreadState *ts);
    void ThreadReplaceBeebFromState(ThreadState *ts, const std::shared_ptr<const BeebState> &beeb_state, uint32_t flags);
    void ThreadReplaceBeeb(ThreadState *ts, std::unique_ptr<BBCMicro> beeb, uint32_t flags);
#if BBCMICRO_TRACE
    void ThreadStartTrace(ThreadState *ts);
    void ThreadBeebStartTrace(ThreadState *ts);
    void ThreadStopTrace(ThreadState *ts);
    void ThreadCancelTrace(ThreadState *ts);
#endif
    void ThreadSetKeyState(ThreadState *ts, BeebKey beeb_key, bool state);
    void ThreadSetFakeMetaKeyStates(ThreadState *ts, BeebMetaKeyState shift_state, BeebMetaKeyState ctrl_state, BeebMetaKeyState func_state);
    void ThreadSetBootState(ThreadState *ts, bool state);
    void ThreadUpdateMetaKeyStates(ThreadState *ts);
#if ENABLE_TAPE
    void ThreadSetTape(ThreadState *ts, std::shared_ptr<const UEFReader> tape);
#endif
    void ThreadSetDiscImage(ThreadState *ts, int drive, std::shared_ptr<DiscImage> disc_image);
    void ThreadStartPaste(ThreadState *ts, std::vector<uint8_t> text);
    void ThreadMain();
    void SetVolume(float *scale_var, float db, bool mute);
    bool ThreadRecordSaveState(ThreadState *ts, bool user_initiated);
    void ThreadStopRecording(ThreadState *ts);
    void ThreadClearRecording(ThreadState *ts);
    void ThreadCheckTimeline(ThreadState *ts);
    static void ThreadAddOSWORD0Callback(ThreadState *ts, std::shared_ptr<OSWORD0Callback> callback);
    static void ThreadRemoveOSWORD0Callback(ThreadState *ts, const std::shared_ptr<OSWORD0Callback> &callback);
    static void ThreadAddOSWRCHCallback(ThreadState *ts, std::shared_ptr<OSWRCHCallback> callback);
    static void ThreadRemoveOSWRCHCallback(ThreadState *ts, const std::shared_ptr<OSWRCHCallback> &callback, bool success);

    // The timeout is not cycle-exact, but it will time out no sooner.
    void ThreadAddCompletionTimeout(ThreadState *ts, std::shared_ptr<Message::CompletionFun> shared_completion_fun, double timeout_relative_seconds);

    // Delete one timeline save state event, leaving the timeline as intact as
    // possible.
    void ThreadDeleteTimelineState(ThreadState *ts, const std::shared_ptr<const BeebState> &state);

    // Truncate the timeline. STATE is the new end.
    void ThreadTruncateTimeline(ThreadState *ts, const std::shared_ptr<const BeebState> &state);

    bool ThreadFindTimelineEventListIndexByBeebState(ThreadState *ts,
                                                     size_t *index,
                                                     const std::shared_ptr<const BeebState> &state);

    // Get next un-replayed replay event.
    const TimelineEvent *ThreadGetNextReplayEvent(ThreadState *ts);

    // Advance to next replay event - i.e., past the one that
    // ThreadGetNextReplayEvent returns.
    void ThreadNextReplayEvent(ThreadState *ts);

    void ThreadStopReplay(ThreadState *ts);

    static void ThreadUpdateCallbacks(ThreadState *ts);
    static void ThreadUpdateInstructionCallbacks(ThreadState *ts);

    static void ThreadCallSharedCompletionFun2(ThreadState *ts, std::shared_ptr<Message::CompletionFun> &&completion_fun, bool success, const char *char_message, std::string *str_message);
    static void ThreadCallSharedCompletionFun(ThreadState *ts, std::shared_ptr<Message::CompletionFun> &&completion_fun, bool success, const char *message);
    static void ThreadCallSharedCompletionFun(ThreadState *ts, std::shared_ptr<Message::CompletionFun> &&completion_fun, bool success, std::string message);

    void SetLastTrace(std::shared_ptr<Trace> last_trace);

    static void ThreadHandleNVRAMChanged(BBCMicro *m, void *context);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
