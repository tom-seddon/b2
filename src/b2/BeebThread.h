#ifdef B2_LIBRETRO_CORE
#error
#endif
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
#include <beeb/BBCMicro.h>
#include <atomic>
#include "BeebConfig.h"
#include "MessageQueue.h"
#include "BeebWindow.h"

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
// and the official data structure for the timeline is
// vector<shared_ptr<TimelineEvent>>. This is very wasteful, and could be
// improved. Maybe one day I'll get round to doing that.
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

class BeebThread {
    struct ThreadState;

  public:
    class Message {
      public:
        typedef std::function<void(bool, std::string)> CompletionFun;

        explicit Message() = default;
        virtual ~Message() = 0;

        static void CallCompletionFun(CompletionFun *completion_fun,
                                      bool success,
                                      const char *message);

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
        // it is to have the completion function called automatically, or move
        // its contents for later calling to do it manually.
        //
        // Default impl does nothing and returns true (completion_fun will be
        // called straight away with no message).
        //
        // Return false to reject the message. The completion_fun will be called
        // with false, and the message will be discarded.
        //
        // Called on Beeb thread.
        virtual bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                                   CompletionFun *completion_fun,
                                   BeebThread *beeb_thread,
                                   ThreadState *ts);

        // Called on the Beeb thread.
        //
        // Default impl does nothing.
        virtual void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const;

      protected:
        // Standard policies for use from the ThreadPrepare function.

        // Returns false if ignored.
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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class KeyMessage : public Message {
      public:
        KeyMessage(BeebKey key, bool state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const bool m_state = false;
        BeebKey m_key = BeebKey_None;
        BeebShiftState m_shift_state = BeebShiftState_Any;
    };

    class JoystickButtonMessage : public Message {
      public:
        JoystickButtonMessage(uint8_t index, bool state);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const uint8_t m_index = 0;
        const BBCMicroState::DigitalJoystickInput m_state = {};
    };

    class HardResetMessage : public Message {
      public:
        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetMessage(uint32_t flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override = 0;

      protected:
        const uint32_t m_flags = 0;

        void HardReset(BeebThread *beeb_thread,
                       ThreadState *ts,
                       const BeebLoadedConfig &loaded_config,
                       const std::vector<uint8_t> &nvram_contents) const;

      private:
    };

    class HardResetAndChangeConfigMessage : public HardResetMessage {
      public:
        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetAndChangeConfigMessage(uint32_t flags,
                                                 BeebLoadedConfig loaded_config);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const BeebLoadedConfig m_loaded_config;
        const std::vector<uint8_t> m_nvram_contents;
    };

    class HardResetAndReloadConfigMessage : public HardResetMessage {
      public:
        // Flags are a combination of BeebThreadHardResetFlag.
        explicit HardResetAndReloadConfigMessage(uint32_t flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread,
                          ThreadState *ts) const override;

      protected:
      private:
    };

    class SetSpeedLimitedMessage : public Message {
      public:
        explicit SetSpeedLimitedMessage(bool limited);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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

        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const int m_drive = -1;
    };

    class SetDriveWriteProtectedMessage : public Message {
      public:
        explicit SetDriveWriteProtectedMessage(int drive, bool is_write_protected);

        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class SaveStateMessage : public Message {
      public:
        explicit SaveStateMessage(bool verbose);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
        std::shared_ptr<const BeebState> m_start_state;
    };

    class StopReplayMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class StartRecordingMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class StopRecordingMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class ClearRecordingMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
        const BeebWindowInitArguments m_init_arguments;
    };

    class StartPasteMessage : public Message {
      public:
        explicit StartPasteMessage(std::string text);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        std::shared_ptr<const std::string> m_text;
    };

    class StopPasteMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
    };

    class StartCopyMessage : public Message {
      public:
        StartCopyMessage(std::function<void(std::vector<uint8_t>)> stop_fun, bool basic);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
        std::function<void(std::vector<uint8_t>)> m_stop_fun;
        bool m_basic = false;
    };

    class StopCopyMessage : public Message {
      public:
        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
    };

    //    class PauseMessage:
    //        public Message
    //    {
    //    public:
    //        explicit PauseMessage(bool pause);
    //
    //        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
    //                           CompletionFun *completion_fun,
    //                           BeebThread *beeb_thread,
    //                           ThreadState *ts) override;
    //    protected:
    //    private:
    //        const bool m_pause=false;
    //    };

#if BBCMICRO_DEBUGGER
    class DebugSetByteMessage : public Message {
      public:
        DebugSetByteMessage(uint16_t addr, uint32_t dso, bool mos, uint8_t value);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

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
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const uint32_t m_addr = 0;
        const uint8_t m_value = 0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetAddressDebugFlags : public Message {
      public:
        DebugSetAddressDebugFlags(M6502Word addr, uint32_t dso, uint8_t addr_flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
        const M6502Word m_addr = {};
        const uint32_t m_dso = 0;
        const uint8_t m_addr_flags = 0;
    };
#endif

#if BBCMICRO_DEBUGGER
    class DebugSetByteDebugFlags : public Message {
      public:
        DebugSetByteDebugFlags(BigPageIndex big_page_index, uint16_t offset, uint8_t byte_flags);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
        const BigPageIndex m_big_page_index = {0};
        const uint16_t m_offset = 0;
        const uint8_t m_byte_flags = 0;
    };
#endif

    class CreateTimelineVideoMessage : public Message {
      public:
        CreateTimelineVideoMessage(std::shared_ptr<const BeebState> state,
                                   std::unique_ptr<VideoWriter> video_writer);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
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
        explicit CallbackMessage(std::function<void(BBCMicro *)> callback);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
        std::function<void(BBCMicro *)> m_callback;
    };

    class TimingMessage : public Message {
      public:
        explicit TimingMessage(uint64_t max_sound_units);

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
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
                           BeebThread *beeb_thread,
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
        //                   BeebThread *beeb_thread,
        //                   ThreadState *ts) override;
        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const bool m_enabled = false;
    };

    class ResetPrinterBufferMessage : public Message {
      public:
        explicit ResetPrinterBufferMessage();

        bool ThreadPrepare(std::shared_ptr<Message> *ptr,
                           CompletionFun *completion_fun,
                           BeebThread *beeb_thread,
                           ThreadState *ts) override;

      protected:
      private:
    };

    class MouseMotionMessage : public Message {
      public:
        explicit MouseMotionMessage(int dx, int dy);

        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const int m_dx = 0;
        const int m_dy = 0;
    };

    class MouseButtonsMessage : public Message {
      public:
        explicit MouseButtonsMessage(uint8_t mask, uint8_t value);

        void ThreadHandle(BeebThread *beeb_thread, ThreadState *ts) const override;

      protected:
      private:
        const uint8_t m_mask = 0;
        const uint8_t m_value = 0;
    };

    struct AudioCallbackRecord {
        uint64_t time = 0;
        uint64_t needed = 0;
        uint64_t available = 0;
    };

    struct TimingStats {
        size_t num_mq_waits = 0;
        size_t num_mq_polls = 0;
    };

    // When planning to set up the BeebThread using a saved state,
    // DEFAULT_LOADED_CONFIG may be default-constructed. In this case hard reset
    // messages and clone window messages won't work, though.
    explicit BeebThread(std::shared_ptr<MessageList> message_list,
                        uint32_t sound_device_id,
                        int sound_freq,
                        size_t sound_buffer_size_samples,
                        BeebLoadedConfig default_loaded_config,
                        std::vector<TimelineEventList> initial_timeline_event_lists);
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

    bool IsPasting() const;

    bool IsCopying() const;

    // Get trace stats, or nullptr if there's no trace.
    const volatile TraceStats *GetTraceStats() const;

    bool IsSpeedLimited() const;

    // Get the speed scale.
    float GetSpeedScale() const;

    // Get the disc image pointer for the given drive, using the given
    // lock object to take a lock on the disc access mutex.
    std::shared_ptr<const DiscImage> GetDiscImage(UniqueLock<Mutex> *lock, int drive) const;

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
    //
    // (FN, if non-null, is called back with the float audio data
    // produced, at the sound chip rate of 250KHz... see the code.)
    size_t AudioThreadFillAudioBuffer(float *samples,
                                      size_t num_samples,
                                      bool perfect,
                                      void (*fn)(int, float, void *) = nullptr,
                                      void *fn_context = nullptr);

    // Set sound/disc volume as attenuation in decibels.
    void SetBBCVolume(float db);
    void SetDiscVolume(float db);

    void SetPowerOnTone(bool power_on_tone);

    // Get info about the previous N audio callbacks.
    std::vector<AudioCallbackRecord> GetAudioCallbackRecords() const;

    TimingStats GetTimingStats() const;

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
    bool DebugIsHalted() const;
    void DebugGetState(std::shared_ptr<const BBCMicroReadOnlyState> *state_ptr, std::shared_ptr<const BBCMicro::DebugState> *debug_state_ptr) const;
#endif

    uint32_t GetUpdateFlags() const;

    bool HasMouse() const;

#if BBCMICRO_DEBUGGER
    std::shared_ptr<const BBCMicro::UpdateMFnData> GetUpdateMFnData() const;
#endif

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
    std::atomic<bool> m_has_nvram{false};
    std::atomic<BBCMicroTypeID> m_beeb_type_id{BBCMicroTypeID_B};
    std::atomic<uint32_t> m_clone_impediments{0};
    std::atomic<bool> m_power_on_tone{true};
    std::atomic<bool> m_is_drive_write_protected[NUM_DRIVES]{};
    std::atomic<bool> m_is_printer_enabled{false};
    std::atomic<size_t> m_printer_data_size_bytes{false};
    std::atomic<uint64_t> m_num_mq_polls{0};
    std::atomic<uint64_t> m_num_mq_waits{0};
    std::atomic<bool> m_debug_is_halted{false};
    std::atomic<uint32_t> m_update_flags{0};

    // Lock m_mutex first, if locking both. (The public API makes this hard to
    // get wrong.)
    mutable Mutex m_beeb_state_mutex;
    std::shared_ptr<const BBCMicroReadOnlyState> m_beeb_state;
#if BBCMICRO_DEBUGGER
    std::shared_ptr<const BBCMicro::DebugState> m_beeb_debug_state;
    std::shared_ptr<const BBCMicro::UpdateMFnData> m_update_mfn_data;
#endif

    // Lock m_mutex first, if locking both. (The public API makes this hard to
    // get wrong.)
    mutable Mutex m_timeline_state_mutex;
    BeebThreadTimelineState m_timeline_state;

    // Controlled by m_mutex.
    std::vector<TimelineBeebStateEvent> m_timeline_beeb_state_events_copy;
    std::string m_config_name;

    mutable Mutex m_mutex;

    //bool m_paused=true;

    // Main thread must take mutex to access.
    ThreadState *m_thread_state = nullptr;

    // Lock m_mutex first, if locking both. (The public API makes this hard to
    // get wrong.)
    Mutex m_last_trace_mutex;

    // Last recorded trace. Controlled by m_last_trace_mutex.
    std::shared_ptr<Trace> m_last_trace;

    // Controlled by m_mutex.
    std::vector<uint8_t> m_printer_buffer;

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
    AudioThreadData *m_audio_thread_data = nullptr;
    uint32_t m_sound_device_id = 0;

    //
    std::shared_ptr<MessageList> m_message_list;

#if BBCMICRO_TRACE
    static bool ThreadHandleTraceInstructionConditions(const BBCMicro *beeb, const M6502 *cpu, void *context);
    static bool ThreadHandleTraceWriteConditions(const BBCMicro *beeb, const M6502 *cpu, void *context);
#endif

    static bool ThreadStopCopyOnOSWORD0(const BBCMicro *beeb, const M6502 *cpu, void *context);
    static bool ThreadAddCopyData(const BBCMicro *beeb, const M6502 *cpu, void *context);

    std::shared_ptr<BeebState> ThreadSaveState(ThreadState *ts);
    void ThreadReplaceBeeb(ThreadState *ts, std::unique_ptr<BBCMicro> beeb, uint32_t flags);
#if BBCMICRO_TRACE
    void ThreadStartTrace(ThreadState *ts);
    void ThreadBeebStartTrace(ThreadState *ts);
    void ThreadStopTrace(ThreadState *ts);
    void ThreadCancelTrace(ThreadState *ts);
#endif
    void ThreadSetKeyState(ThreadState *ts, BeebKey beeb_key, bool state);
    void ThreadSetFakeShiftState(ThreadState *ts, BeebShiftState state);
    void ThreadSetBootState(ThreadState *ts, bool state);
    void ThreadUpdateShiftKeyState(ThreadState *ts);
    void ThreadSetDiscImage(ThreadState *ts, int drive, std::shared_ptr<DiscImage> disc_image);
    void ThreadStartPaste(ThreadState *ts, std::shared_ptr<const std::string> text);
    void ThreadStopCopy(ThreadState *ts);
    void ThreadMain();
    void SetVolume(float *scale_var, float db);
    bool ThreadRecordSaveState(ThreadState *ts, bool user_initiated);
    void ThreadStopRecording(ThreadState *ts);
    void ThreadClearRecording(ThreadState *ts);
    void ThreadCheckTimeline(ThreadState *ts);

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

    void SetLastTrace(std::shared_ptr<Trace> last_trace);

    static bool ThreadWaitForHardReset(const BBCMicro *beeb, const M6502 *cpu, void *context);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
