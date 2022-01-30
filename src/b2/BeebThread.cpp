#include <shared/system.h>
#include "BeebThread.h"
#include <shared/log.h>
#include <shared/debug.h>
#include "MessageQueue.h"
#include <beeb/OutputData.h>
#include <beeb/BBCMicro.h>
#include <beeb/video.h>
#include <beeb/sound.h>
#include <stdlib.h>
#include <beeb/DiscImage.h>
#include <vector>
#include "BeebState.h"
#include "BeebWindows.h"
#include "b2.h"
#include <beeb/TVOutput.h>
#include "misc.h"
#include "Messages.h"
#include <beeb/Trace.h>
#include "keys.h"
#include <string.h>
#include <inttypes.h>
#include "Remapper.h"
#include "filters.h"
#include "conf.h"
#include <math.h>
#include "WriteVideoJob.h"
#include "BeebWindow.h"
#include <Remotery.h>
#include "GenerateThumbnailJob.h"
#include "VideoWriter.h"
#include "BeebLinkHTTPHandler.h"

#include <shared/enum_def.h>
#include "BeebThread.inl"
#include <shared/enum_end.h>

#include <shared/enum_decl.h>
#include "BeebThread_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "BeebThread_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Number of 2MHz cycles the emulated BBC will run for, flat out.
static const int32_t RUN_2MHz_CYCLES = 2000;

// ~1MByte
static constexpr size_t NUM_VIDEO_UNITS = 262144;
static constexpr size_t NUM_AUDIO_UNITS = NUM_VIDEO_UNITS / 2; //(1<<SOUND_CLOCK_SHIFT);

// When recording, how often to save a state.
static const uint64_t TIMELINE_SAVE_STATE_FREQUENCY_2MHz_CYCLES = (uint64_t)2e6;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::atomic<uint64_t> g_next_uid{1};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define LOGGING 0
#define LOGGING_FREQUENCY_SECONDS (1.)
#endif

#if LOGGING
LOG_DEFINE(ATH, "audio", "AUTHRD", &log_printer_stdout_and_debugger);
#endif

// This is a dummy log. It's never used for printing, only for
// initialising each BeebThread's log. Since it's a global, it gets an
// entry in the global table, so it can interact with the command line
// options.
LOG_TAGGED_DEFINE(BTHREAD, "beeb", "", &log_printer_stdout_and_debugger, false);

LOG_DEFINE(REPLAY, "REPLAY ", &log_printer_stderr_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// What to feed to OSRDCH (via Paste OSRDCH) to list a program.
static const std::shared_ptr<const std::string> COPY_BASIC = std::make_shared<const std::string>("OLD\rLIST\r");

// What a listed program's OSWRCH output will start with if it was listed by
// doing a Paste OSRDCH with *COPY_BASIC.
//
// (Since BBCMicro::PASTE_START_CHAR is of POD type, I'm gambling that it will
// be initialised in time.)
static const std::string COPY_BASIC_PREFIX = strprintf("%cOLD\n\r>LIST\n\r", BBCMicro::PASTE_START_CHAR);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const float VOLUMES_TABLE[] = {
    0.00000f,
    0.03981f,
    0.05012f,
    0.06310f,
    0.07943f,
    0.10000f,
    0.12589f,
    0.15849f,
    0.19953f,
    0.25119f,
    0.31623f,
    0.39811f,
    0.50119f,
    0.63096f,
    0.79433f,
    1.00000f,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebThread::ThreadState {
    bool stop = false;

    // For the benefit of callbacks that have a ThreadState * as their context.
    BeebThread *beeb_thread = nullptr;
    const uint64_t *num_executed_2MHz_cycles = nullptr;
    uint64_t next_stop_2MHz_cycles = 0;

    BBCMicro *beeb = nullptr;
    BeebLoadedConfig current_config;
#if BBCMICRO_TRACE
    BeebThreadTraceState trace_state = BeebThreadTraceState_None;
    uint64_t trace_start_2MHz_cycles = 0;
    TraceConditions trace_conditions;
    size_t trace_max_num_bytes = 0;
#endif
    bool boot = false;
    BeebShiftState fake_shift_state = BeebShiftState_Any;

    BeebThreadTimelineMode timeline_mode = BeebThreadTimelineMode_None;

    // The timeline end event's message pointer is always null.
    TimelineEvent timeline_end_event = {};
    std::vector<TimelineEventList> timeline_event_lists;
    std::shared_ptr<BeebState> timeline_replay_old_state;
    size_t timeline_replay_list_index = 0;
    size_t timeline_replay_list_event_index = 0;
    uint64_t timeline_replay_time_2MHz_cycles = 0;

    bool copy_basic = false;
    std::function<void(std::vector<uint8_t>)> copy_stop_fun;
    std::vector<uint8_t> copy_data;

    Message::CompletionFun reset_completion_fun;
    uint64_t reset_timeout_cycles = 0;

    Message::CompletionFun paste_completion_fun;

#if HTTP_SERVER
    Message::CompletionFun async_call_completion_fun;
#endif

    std::unique_ptr<BeebLinkHTTPHandler> beeblink_handler;

    Log log{"BEEB  ", LOG(BTHREAD)};
    Messages msgs;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebThread::AudioThreadData {
    uint64_t sound_freq;
    Remapper remapper;
    uint64_t num_consumed_sound_units = 0;
    float bbc_sound_scale = 1.f;
    float disc_sound_scale = 1.f;
    std::vector<AudioCallbackRecord> records;
    size_t record0_index = 0;
    uint64_t sound_buffer_size_samples = 0;

#if LOGGING
    volatile uint64_t num_executed_cycles = 0;
    uint64_t last_print_ticks = 0;
#endif

    AudioThreadData(uint64_t sound_freq, uint64_t sound_buffer_size_samples, size_t max_num_records);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::AudioThreadData::AudioThreadData(uint64_t sound_freq_,
                                             uint64_t sound_buffer_size_samples_,
                                             size_t max_num_records)
    : sound_freq(sound_freq_)
    , remapper(sound_freq, SOUND_CLOCK_HZ)
    , sound_buffer_size_samples(sound_buffer_size_samples_) {
    this->records.resize(max_num_records);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::Message::~Message() = default;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::Message::CallCompletionFun(CompletionFun *completion_fun,
                                            bool success,
                                            const char *message_) {
    if (!!*completion_fun) {
        std::string message;
        if (message_) {
            message.assign(message_);
        }

        (*completion_fun)(success, std::move(message));

        *completion_fun = CompletionFun();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::Message::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                        CompletionFun *completion_fun,
                                        BeebThread *beeb_thread,
                                        ThreadState *ts) {
    (void)ptr, (void)completion_fun, (void)beeb_thread, (void)ts;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::Message::ThreadHandle(BeebThread *beeb_thread,
                                       ThreadState *ts) const {
    (void)beeb_thread, (void)ts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::Message::PrepareUnlessReplayingOrHalted(std::shared_ptr<Message> *ptr,
                                                         CompletionFun *completion_fun,
                                                         BeebThread *beeb_thread,
                                                         ThreadState *ts) {
    if (!PrepareUnlessReplaying(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

#if BBCMICRO_DEBUG
    if (ts->beeb) {
        if (ts->beeb->DebugIsHalted()) {
            CallCompletionFun(completion_fun, false, "not valid while halted");
            return false;
        }
    }
#endif

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::Message::PrepareUnlessReplaying(std::shared_ptr<Message> *ptr,
                                                 CompletionFun *completion_fun,
                                                 BeebThread *beeb_thread,
                                                 ThreadState *ts) {
    (void)ptr, (void)beeb_thread;

    if (ts->timeline_mode == BeebThreadTimelineMode_Replay) {
        CallCompletionFun(completion_fun, false, "not valid while replaying");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StopMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                            CompletionFun *completion_fun,
                                            BeebThread *beeb_thread,
                                            ThreadState *ts) {
    (void)completion_fun, (void)beeb_thread;

    ptr->reset();

    ts->stop = true;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::KeyMessage::KeyMessage(BeebKey key, bool state)
    : m_key(key)
    , m_state(state) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::KeyMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                           CompletionFun *completion_fun,
                                           BeebThread *beeb_thread,
                                           ThreadState *ts) {
    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    if (beeb_thread->m_real_key_states.GetState(m_key) == m_state) {
        // not an error - just don't duplicate events when the key is held.
        ptr->reset();
        return true;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::KeyMessage::ThreadHandle(BeebThread *beeb_thread,
                                          ThreadState *ts) const {
    beeb_thread->ThreadSetKeyState(ts, m_key, m_state);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::KeySymMessage::KeySymMessage(BeebKeySym key_sym, bool state)
    : m_state(state) {
    if (!GetBeebKeyComboForKeySym(&m_key, &m_shift_state, key_sym)) {
        m_key = BeebKey_None;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::KeySymMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                              CompletionFun *completion_fun,
                                              BeebThread *beeb_thread,
                                              ThreadState *ts) {
    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    if (m_key == BeebKey_None) {
        return false;
    }

    if (beeb_thread->m_real_key_states.GetState(m_key) == m_state &&
        ts->fake_shift_state == m_shift_state) {
        // not an error - just don't duplicate events when the key is held.
        ptr->reset();
        return true;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::KeySymMessage::ThreadHandle(BeebThread *beeb_thread,
                                             ThreadState *ts) const {
    beeb_thread->ThreadSetFakeShiftState(ts, m_shift_state);
    beeb_thread->ThreadSetKeyState(ts, m_key, m_state);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::HardResetMessage::HardResetMessage(uint32_t flags)
    : m_flags(flags) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::HardResetMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                 CompletionFun *completion_fun,
                                                 BeebThread *beeb_thread,
                                                 ThreadState *ts) {
    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::HardResetMessage::HardReset(BeebThread *beeb_thread,
                                             ThreadState *ts,
                                             const BeebLoadedConfig &loaded_config,
                                             const std::vector<uint8_t> &nvram_contents) const {
    uint32_t replace_flags = BeebThreadReplaceFlag_KeepCurrentDiscs;

    if (m_flags & BeebThreadHardResetFlag_Boot) {
        replace_flags |= BeebThreadReplaceFlag_Autoboot;
    }

    if (ts->timeline_mode != BeebThreadTimelineMode_Replay) {
        replace_flags |= BeebThreadReplaceFlag_ResetKeyState;
    }

    ts->current_config = loaded_config;

    tm now = GetLocalTimeNow();

    uint64_t num_2MHz_cycles = 0;
    if (ts->num_executed_2MHz_cycles) {
        num_2MHz_cycles = *ts->num_executed_2MHz_cycles;
    }

    if (ts->current_config.config.beeblink) {
        if (!ts->beeblink_handler) {
            std::string sender_id = strprintf("%" PRIu64, beeb_thread->m_uid);
            ts->beeblink_handler = std::make_unique<BeebLinkHTTPHandler>(beeb_thread,
                                                                         std::move(sender_id),
                                                                         beeb_thread->m_message_list);

            if (!ts->beeblink_handler->Init(&ts->msgs)) {
                // Ugh. Just give up.
                ts->beeblink_handler.reset();
            }
        }
    } else {
        ts->beeblink_handler.reset();
    }

    auto beeb = std::make_unique<BBCMicro>(ts->current_config.config.type,
                                           ts->current_config.config.disc_interface,
                                           nvram_contents,
                                           &now,
                                           ts->current_config.config.video_nula,
                                           ts->current_config.config.ext_mem,
                                           beeb_thread->m_power_on_tone.load(std::memory_order_acquire),
                                           ts->beeblink_handler.get(),
                                           num_2MHz_cycles);

    beeb->SetOSROM(ts->current_config.os);

    for (uint8_t i = 0; i < 16; ++i) {
        if (ts->current_config.config.roms[i].writeable) {
            if (!!ts->current_config.roms[i]) {
                beeb->SetSidewaysRAM(i, ts->current_config.roms[i]);
            } else {
                beeb->SetSidewaysRAM(i, nullptr);
            }
        } else {
            if (!!ts->current_config.roms[i]) {
                beeb->SetSidewaysROM(i, ts->current_config.roms[i]);
            } else {
                beeb->SetSidewaysROM(i, nullptr);
            }
        }
    }

    beeb_thread->ThreadReplaceBeeb(ts, std::move(beeb), replace_flags);

#if BBCMICRO_DEBUGGER
    if (m_flags & BeebThreadHardResetFlag_Run) {
        ts->beeb->DebugRun();
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::HardResetAndChangeConfigMessage::HardResetAndChangeConfigMessage(uint32_t flags,
                                                                             BeebLoadedConfig loaded_config)
    : HardResetMessage(flags)
    , m_loaded_config(std::move(loaded_config))
    , m_nvram_contents(GetDefaultNVRAMContents(m_loaded_config.config.type)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::HardResetAndChangeConfigMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                                CompletionFun *completion_fun,
                                                                BeebThread *beeb_thread,
                                                                ThreadState *ts) {
    return PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::HardResetAndChangeConfigMessage::ThreadHandle(BeebThread *beeb_thread,
                                                               ThreadState *ts) const {
    this->HardReset(beeb_thread, ts, m_loaded_config, m_nvram_contents);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::HardResetAndReloadConfigMessage::HardResetAndReloadConfigMessage(uint32_t flags)
    : HardResetMessage(flags) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::HardResetAndReloadConfigMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                                CompletionFun *completion_fun,
                                                                BeebThread *beeb_thread,
                                                                ThreadState *ts) {
    (void)beeb_thread;

    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    BeebLoadedConfig reloaded_config;
    if (!BeebLoadedConfig::Load(&reloaded_config, ts->current_config.config, &ts->msgs)) {
        return false;
    }

    reloaded_config.ReuseROMs(ts->current_config);

    *ptr = std::make_shared<HardResetAndChangeConfigMessage>(m_flags,
                                                             std::move(reloaded_config));
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::HardResetAndReloadConfigMessage::ThreadHandle(BeebThread *beeb_thread,
                                                               ThreadState *ts) const {
    (void)beeb_thread, (void)ts;

    // should never happen - ThreadPrepare replaces this with a
    // HardResetAndChangeConfigMessage.
    ASSERT(false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SetSpeedLimitedMessage::SetSpeedLimitedMessage(bool limited)
    : m_limited(limited) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::SetSpeedLimitedMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                       CompletionFun *completion_fun,
                                                       BeebThread *beeb_thread,
                                                       ThreadState *ts) {
    (void)ptr, (void)completion_fun, (void)ts;

    beeb_thread->m_is_speed_limited.store(m_limited, std::memory_order_release);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SetSpeedScaleMessage::SetSpeedScaleMessage(float scale)
    : m_scale(scale) {
    //ASSERT(m_scale>=0.0&&m_scale<=1.0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::SetSpeedScaleMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                     CompletionFun *completion_fun,
                                                     BeebThread *beeb_thread,
                                                     ThreadState *ts) {
    (void)completion_fun, (void)ts;

    beeb_thread->m_speed_scale.store(m_scale, std::memory_order_release);

    {
        AudioDeviceLock lock(beeb_thread->m_sound_device_id);

        // This resets the error, but I'm really not bothered.
        beeb_thread->m_audio_thread_data->remapper = Remapper(beeb_thread->m_audio_thread_data->sound_freq,
                                                              (uint64_t)(SOUND_CLOCK_HZ * m_scale));
    }

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::LoadDiscMessage::LoadDiscMessage(int drive,
                                             std::shared_ptr<DiscImage> disc_image,
                                             bool verbose)
    : m_drive(drive)
    , m_disc_image(std::move(disc_image))
    , m_verbose(verbose) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::LoadDiscMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                CompletionFun *completion_fun,
                                                BeebThread *beeb_thread,
                                                ThreadState *ts) {
    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    if (m_verbose) {
        if (!m_disc_image) {
            ts->msgs.i.f("Drive %d: empty\n", m_drive);
        } else {
            ts->msgs.i.f("Drive %d: %s\n", m_drive, m_disc_image->GetName().c_str());

            std::string hash = m_disc_image->GetHash();
            if (!hash.empty()) {
                ts->msgs.i.f("(Hash: %s)\n", hash.c_str());
            }
        }
    }

    if (m_disc_image->CanClone()) {
        // OK - can record this message as-is.
    } else {
        // Not recordable.
        if (ts->timeline_mode == BeebThreadTimelineMode_Record) {
            ts->msgs.e.f("Can't load this type of disc image while recording\n");
            ts->msgs.i.f("(%s: %s)\n",
                         m_disc_image->GetLoadMethod().c_str(),
                         m_disc_image->GetName().c_str());
            ptr->reset();
            return false;
        }

        ASSERT(ts->timeline_mode == BeebThreadTimelineMode_None);

        // This doesn't move the disc image pointer, because it can't.
        //
        // (There's only supposed to be one owning DiscImage pointer at a time,
        // but there's no enforcing this, and it doesn't matter here anyway,
        // because *this will be destroyed soon enough.)
        beeb_thread->ThreadSetDiscImage(ts, m_drive, m_disc_image);
        ptr->reset();
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::LoadDiscMessage::ThreadHandle(BeebThread *beeb_thread,
                                               ThreadState *ts) const {
    ASSERT(m_disc_image->CanClone());
    beeb_thread->ThreadSetDiscImage(ts, m_drive, m_disc_image->Clone());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::EjectDiscMessage::EjectDiscMessage(int drive)
    : m_drive(drive) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::EjectDiscMessage::ThreadHandle(BeebThread *beeb_thread,
                                                ThreadState *ts) const {
    beeb_thread->ThreadSetDiscImage(ts, m_drive, nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SetDriveWriteProtectedMessage::SetDriveWriteProtectedMessage(int drive,
                                                                         bool is_write_protected)
    : m_drive(drive)
    , m_is_write_protected(is_write_protected) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetDriveWriteProtectedMessage::ThreadHandle(BeebThread *beeb_thread,
                                                             ThreadState *ts) const {
    ASSERT(m_drive >= 0 && m_drive < NUM_DRIVES);

    ts->beeb->SetDriveWriteProtected(m_drive, m_is_write_protected);
    beeb_thread->m_is_drive_write_protected[m_drive].store(m_is_write_protected,
                                                           std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::BeebStateMessage::BeebStateMessage(std::shared_ptr<const BeebState> state,
                                               bool user_initiated)
    : m_state(std::move(state))
    , m_user_initiated(user_initiated) {
}

const std::shared_ptr<const BeebState> &BeebThread::BeebStateMessage::GetBeebState() const {
    return m_state;
}

void BeebThread::BeebStateMessage::ThreadHandle(BeebThread *beeb_thread,
                                                ThreadState *ts) const {
    (void)beeb_thread, (void)ts;
    //beeb_thread->ThreadReplaceBeeb(ts,this->GetBeebState()->CloneBBCMicro(),0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::LoadStateMessage::LoadStateMessage(std::shared_ptr<const BeebState> state,
                                               bool verbose)
    : BeebStateMessage(std::move(state), true)
    , m_verbose(verbose) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::LoadStateMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                 CompletionFun *completion_fun,
                                                 BeebThread *beeb_thread,
                                                 ThreadState *ts) {
    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        if (m_verbose) {
            ts->msgs.e.f("Can't load a saved state while replaying or halted.\n");
        }

        return false;
    }

    if (ts->timeline_mode == BeebThreadTimelineMode_Record) {
        return false;
    }

    beeb_thread->ThreadReplaceBeeb(ts,
                                   this->GetBeebState()->CloneBBCMicro(),
                                   BeebThreadReplaceFlag_ResetKeyState);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::LoadTimelineStateMessage::LoadTimelineStateMessage(std::shared_ptr<const BeebState> state,
                                                               bool verbose)
    : BeebStateMessage(std::move(state), true)
    , m_verbose(verbose) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::LoadTimelineStateMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                         CompletionFun *completion_fun,
                                                         BeebThread *beeb_thread,
                                                         ThreadState *ts) {
    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        if (m_verbose) {
            ts->msgs.e.f("Can't load a saved state while replaying or halted.\n");
        }

        return false;
    }

    if (ts->timeline_mode == BeebThreadTimelineMode_Record) {
        beeb_thread->ThreadTruncateTimeline(ts, this->GetBeebState());
    }

    beeb_thread->ThreadReplaceBeeb(ts,
                                   this->GetBeebState()->CloneBBCMicro(),
                                   BeebThreadReplaceFlag_ResetKeyState);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::DeleteTimelineStateMessage::DeleteTimelineStateMessage(std::shared_ptr<const BeebState> state)
    : BeebStateMessage(std::move(state), true) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::DeleteTimelineStateMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                           CompletionFun *completion_fun,
                                                           BeebThread *beeb_thread,
                                                           ThreadState *ts) {
    if (!PrepareUnlessReplaying(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    beeb_thread->ThreadDeleteTimelineState(ts, this->GetBeebState());
    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SaveStateMessage::SaveStateMessage(bool verbose)
    : m_verbose(verbose) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::SaveStateMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                 CompletionFun *completion_fun,
                                                 BeebThread *beeb_thread,
                                                 ThreadState *ts) {
    uint32_t impediments = ts->beeb->GetCloneImpediments();
    if (impediments != 0) {
        ts->msgs.e.f("Can't save state, due to: %s\n", GetCloneImpedimentsDescription(impediments).c_str());
        return false;
    }

    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    auto &&state = std::make_shared<BeebState>(ts->beeb->Clone());
    state->SetName(GetTimeString(GetUTCTimeNow()));

    if (m_verbose) {
        std::string time_str = Get2MHzCyclesString(state->GetEmulated2MHzCycles());
        ts->msgs.i.f("Saved state: %s\n", time_str.c_str());
    }

    BeebWindows::AddSavedState(state);

    *ptr = std::make_shared<BeebStateMessage>(std::move(state), true);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::StartReplayMessage::StartReplayMessage(std::shared_ptr<const BeebState> start_state)
    : m_start_state(std::move(start_state)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StartReplayMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                   CompletionFun *completion_fun,
                                                   BeebThread *beeb_thread,
                                                   ThreadState *ts) {
    (void)ptr, (void)completion_fun;

    if (ts->timeline_mode == BeebThreadTimelineMode_Record) {
        // Not valid in record mode.
        return false;
    }

    size_t index;
    if (!beeb_thread->ThreadFindTimelineEventListIndexByBeebState(ts,
                                                                  &index,
                                                                  m_start_state)) {
        return false;
    }

    if (ts->timeline_mode == BeebThreadTimelineMode_None) {
        if (ts->beeb) {
            // TODO - how to get the TVOutput here? Don't remember what I had
            // planned originally...
            ts->timeline_replay_old_state = std::make_shared<BeebState>(ts->beeb->Clone());
        }
    }

    ts->timeline_mode = BeebThreadTimelineMode_Replay;
    ts->timeline_replay_list_index = index;
    ts->timeline_replay_list_event_index = ~(size_t)0;
    beeb_thread->ThreadNextReplayEvent(ts);
    ts->timeline_replay_time_2MHz_cycles = m_start_state->GetEmulated2MHzCycles();

    LOGF(REPLAY, "replay initiated: list index=%zu, list event index=%zu, replay cycles=%" PRIu64 "\n",
         ts->timeline_replay_list_index,
         ts->timeline_replay_list_event_index,
         ts->timeline_replay_time_2MHz_cycles);

    beeb_thread->ThreadReplaceBeeb(ts, m_start_state->CloneBBCMicro(), 0);

    beeb_thread->ThreadCheckTimeline(ts);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StopReplayMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                  CompletionFun *completion_fun,
                                                  BeebThread *beeb_thread,
                                                  ThreadState *ts) {
    (void)ptr, (void)completion_fun;

    if (ts->timeline_mode != BeebThreadTimelineMode_Replay) {
        return false;
    }

    beeb_thread->ThreadStopReplay(ts);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StartRecordingMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                      CompletionFun *completion_fun,
                                                      BeebThread *beeb_thread,
                                                      ThreadState *ts) {
    if (!PrepareUnlessReplaying(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    if (ts->timeline_mode != BeebThreadTimelineMode_None) {
        return false;
    }

    uint32_t impediments = ts->beeb->GetCloneImpediments();
    if (impediments != 0) {
        ts->msgs.e.f("Can't record, due to: %s\n", GetCloneImpedimentsDescription(impediments).c_str());
        return false;
    }

    beeb_thread->ThreadClearRecording(ts);

    ts->timeline_mode = BeebThreadTimelineMode_Record;

    bool good_save_state = beeb_thread->ThreadRecordSaveState(ts, true);
    (void)good_save_state;
    ASSERT(good_save_state);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StopRecordingMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                     CompletionFun *completion_fun,
                                                     BeebThread *beeb_thread,
                                                     ThreadState *ts) {
    (void)completion_fun;

    beeb_thread->ThreadStopRecording(ts);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ClearRecordingMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                      CompletionFun *completion_fun,
                                                      BeebThread *beeb_thread,
                                                      ThreadState *ts) {
    (void)completion_fun;

    beeb_thread->ThreadClearRecording(ts);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
BeebThread::StartTraceMessage::StartTraceMessage(const TraceConditions &conditions,
                                                 size_t max_num_bytes)
    : m_conditions(conditions)
    , m_max_num_bytes(max_num_bytes) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
bool BeebThread::StartTraceMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                  CompletionFun *completion_fun,
                                                  BeebThread *beeb_thread,
                                                  ThreadState *ts) {
    ts->trace_conditions = m_conditions;
    ts->trace_max_num_bytes = m_max_num_bytes;

    beeb_thread->ThreadStartTrace(ts);

    CallCompletionFun(completion_fun, true, nullptr);

    ptr->reset();
    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
bool BeebThread::StopTraceMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                 CompletionFun *completion_fun,
                                                 BeebThread *beeb_thread,
                                                 ThreadState *ts) {
    beeb_thread->ThreadStopTrace(ts);

    CallCompletionFun(completion_fun, true, nullptr);

    ptr->reset();
    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::CloneWindowMessage::CloneWindowMessage(BeebWindowInitArguments init_arguments)
    : m_init_arguments(std::move(init_arguments)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::CloneWindowMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                   CompletionFun *completion_fun,
                                                   BeebThread *beeb_thread,
                                                   ThreadState *ts) {
    (void)completion_fun;

    BeebWindowInitArguments init_arguments = m_init_arguments;

    //init_arguments.initially_paused=false;
    init_arguments.initial_state = beeb_thread->ThreadSaveState(ts);

    PushNewWindowMessage(init_arguments);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::StartPasteMessage::StartPasteMessage(std::string text)
    : m_text(std::make_shared<std::string>(std::move(text))) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StartPasteMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                  CompletionFun *completion_fun,
                                                  BeebThread *beeb_thread,
                                                  ThreadState *ts) {
    // In principle, it ought to be OK to initaite a paste when halted,
    // but it's probably not very useful.
    if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::StartPasteMessage::ThreadHandle(BeebThread *beeb_thread,
                                                 ThreadState *ts) const {
    beeb_thread->ThreadStartPaste(ts, m_text);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StopPasteMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                 CompletionFun *completion_fun,
                                                 BeebThread *beeb_thread,
                                                 ThreadState *ts) {
    return PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::StopPasteMessage::ThreadHandle(BeebThread *beeb_thread,
                                                ThreadState *ts) const {
    ts->beeb->StopPaste();
    beeb_thread->m_is_pasting.store(false, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::StartCopyMessage::StartCopyMessage(std::function<void(std::vector<uint8_t>)> stop_fun,
                                               bool basic)
    : m_stop_fun(std::move(stop_fun))
    , m_basic(basic) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StartCopyMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                 CompletionFun *completion_fun,
                                                 BeebThread *beeb_thread,
                                                 ThreadState *ts) {
    // StartCopy and StartCopyBASIC really aren't the same
    // sort of thing, but they share enough code that it felt
    // a bit daft whichever way round they went.

    if (m_basic) {
        if (!PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
            return false;
        }

        beeb_thread->ThreadStartPaste(ts, COPY_BASIC);

        //        std::string text;
        //        for(size_t i=0;COPY_BASIC_LINES[i];++i) {
        //            text+=COPY_BASIC_LINES[i];
        //            text.push_back('\r');
        //        }
        //
        //        beeb_thread->ThreadStartPaste(ts,std::move(text));

        ts->beeb->AddInstructionFn(&ThreadStopCopyOnOSWORD0, ts);
    }

    ts->copy_data.clear();
    if (!ts->beeb_thread->m_is_copying) {
        ts->beeb->AddInstructionFn(&ThreadAddCopyData, ts);
    }

    ts->copy_basic = m_basic;
    ts->copy_stop_fun = std::move(m_stop_fun);
    beeb_thread->m_is_copying.store(true, std::memory_order_release);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::StopCopyMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                CompletionFun *completion_fun,
                                                BeebThread *beeb_thread,
                                                ThreadState *ts) {
    (void)completion_fun;

    beeb_thread->ThreadStopCopy(ts);

    ptr->reset();

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BeebThread::PauseMessage::PauseMessage(bool pause):
//    m_pause(pause)
//{
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool BeebThread::PauseMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
//                                             CompletionFun *completion_fun,
//                                             BeebThread *beeb_thread,
//                                             ThreadState *ts)
//{
//    (void)ptr,(void)completion_fun,(void)ts;
//
//    beeb_thread->m_paused=m_pause;
//
//    ptr->reset();
//    return true;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetByteMessage::DebugSetByteMessage(uint16_t addr,
                                                     uint32_t dpo,
                                                     uint8_t value)
    : m_addr(addr)
    , m_dpo(dpo)
    , m_value(value) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebThread::DebugSetByteMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                    CompletionFun *completion_fun,
                                                    BeebThread *beeb_thread,
                                                    ThreadState *ts) {
    return PrepareUnlessReplaying(ptr, completion_fun, beeb_thread, ts);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebThread::DebugSetByteMessage::ThreadHandle(BeebThread *beeb_thread,
                                                   ThreadState *ts) const {
    (void)beeb_thread;

    M6502Word addr = {m_addr};

    ts->beeb->DebugSetBytes(addr, m_dpo, &m_value, 1);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetBytesMessage::DebugSetBytesMessage(uint32_t addr,
                                                       uint32_t dpo,
                                                       std::vector<uint8_t> values)
    : m_addr(addr)
    , m_dpo(dpo)
    , m_values(std::move(values)) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebThread::DebugSetBytesMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                     CompletionFun *completion_fun,
                                                     BeebThread *beeb_thread,
                                                     ThreadState *ts) {
    return PrepareUnlessReplaying(ptr, completion_fun, beeb_thread, ts);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebThread::DebugSetBytesMessage::ThreadHandle(BeebThread *beeb_thread,
                                                    ThreadState *ts) const {
    (void)beeb_thread;

    ts->beeb->DebugSetBytes({(uint16_t)m_addr}, m_dpo, m_values.data(), m_values.size());
    //
    //    M6502Word addr={(uint16_t)m_addr};
    //
    //    for(uint8_t value:m_values) {
    //        ts->beeb->DebugSetByte(
    //        const BBCMicro::BigPage *bp=ts->beeb->DebugGetBigPageForAddress(addr,m_dpo);
    //        bp->
    //        ts->beeb->DebugSetByte(addr,m_dpo,value);
    //        ++addr.w;
    //    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetExtByteMessage::DebugSetExtByteMessage(uint32_t addr_, uint8_t value_)
    : m_addr(addr_)
    , m_value(value_) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebThread::DebugSetExtByteMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                       CompletionFun *completion_fun,
                                                       BeebThread *beeb_thread,
                                                       ThreadState *ts) {
    return PrepareUnlessReplaying(ptr, completion_fun, beeb_thread, ts);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebThread::DebugSetExtByteMessage::ThreadHandle(BeebThread *beeb_thread,
                                                      ThreadState *ts) const {
    (void)beeb_thread;

    ts->beeb->SetExtMemory(m_addr, m_value);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugAsyncCallMessage::DebugAsyncCallMessage(uint16_t addr,
                                                         uint8_t a,
                                                         uint8_t x,
                                                         uint8_t y,
                                                         bool c)
    : m_addr(addr)
    , m_a(a)
    , m_x(x)
    , m_y(y)
    , m_c(c) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebThread::DebugAsyncCallMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                      CompletionFun *completion_fun,
                                                      BeebThread *beeb_thread,
                                                      ThreadState *ts) {
    if (!this->PrepareUnlessReplayingOrHalted(ptr, completion_fun, beeb_thread, ts)) {
        return false;
    }

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BeebThread::DebugAsyncCallMessage::ThreadHandle(BeebThread *beeb_thread,
                                                     ThreadState *ts) const {
    (void)beeb_thread;

    ts->beeb->DebugSetAsyncCall(m_addr, m_a, m_x, m_y, m_c, &BeebThread::DebugAsyncCallCallback, ts);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetAddressDebugFlags::DebugSetAddressDebugFlags(M6502Word addr, uint8_t addr_flags)
    : m_addr(addr)
    , m_addr_flags(addr_flags) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebThread::DebugSetAddressDebugFlags::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                          CompletionFun *completion_fun,
                                                          BeebThread *beeb_thread,
                                                          ThreadState *ts) {
    (void)completion_fun, (void)beeb_thread;

    ts->beeb->DebugSetAddressDebugFlags(m_addr, m_addr_flags);

    ptr->reset();
    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetByteDebugFlags::DebugSetByteDebugFlags(uint8_t big_page_index, uint16_t offset, uint8_t byte_flags)
    : m_big_page_index(big_page_index)
    , m_offset(offset)
    , m_byte_flags(byte_flags) {
    ASSERT(m_big_page_index < NUM_BIG_PAGES);
    ASSERT(offset < BBCMicro::BIG_PAGE_SIZE_BYTES);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebThread::DebugSetByteDebugFlags::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                       CompletionFun *completion_fun,
                                                       BeebThread *beeb_thread,
                                                       ThreadState *ts) {
    (void)completion_fun, (void)beeb_thread;

    ts->beeb->DebugSetByteDebugFlags(m_big_page_index, m_offset, m_byte_flags);

    ptr->reset();
    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::CreateTimelineVideoMessage::CreateTimelineVideoMessage(std::shared_ptr<const BeebState> state,
                                                                   std::unique_ptr<VideoWriter> video_writer)
    : m_state(std::move(state))
    , m_video_writer(std::move(video_writer)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::CreateTimelineVideoMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                           CompletionFun *completion_fun,
                                                           BeebThread *beeb_thread,
                                                           ThreadState *ts) {
    (void)completion_fun;

    size_t index;
    if (!beeb_thread->ThreadFindTimelineEventListIndexByBeebState(ts, &index, m_state)) {
        return false;
    }

    TimelineEventList video_event_list;
    video_event_list.state_event = ts->timeline_event_lists[index].state_event;
    for (size_t i = index; i < ts->timeline_event_lists.size(); ++i) {
        const TimelineEventList *list = &ts->timeline_event_lists[i];
        video_event_list.events.insert(video_event_list.events.end(),
                                       list->events.begin(),
                                       list->events.end());
    }

    video_event_list.events.push_back(ts->timeline_end_event);

    auto job = std::make_shared<WriteVideoJob>(std::move(video_event_list),
                                               std::move(m_video_writer));
    BeebWindows::AddJob(job);

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BeebThread::CustomMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                              CompletionFun *completion_fun,
                                              BeebThread *beeb_thread,
                                              ThreadState *ts) {
    (void)completion_fun, (void)beeb_thread;

    this->ThreadHandleMessage(ts->beeb);

    ptr->reset();
    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::TimingMessage::TimingMessage(uint64_t max_sound_units)
    : m_max_sound_units(max_sound_units) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::TimingMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                              CompletionFun *completion_fun,
                                              BeebThread *beeb_thread,
                                              ThreadState *ts) {
    (void)completion_fun, (void)beeb_thread;

    //uint64_t old=ts->next_stop_2MHz_cycles;
    ts->next_stop_2MHz_cycles = m_max_sound_units << SOUND_CLOCK_SHIFT;

    // The new next stop can be sooner than the old next stop when the speed
    // limit is being reduced.

    //printf("%s: was %" PRIu64 ", now %" PRIu64 " (%" PRId64 ")\n",__func__,old,ts->next_stop_2MHz_cycles,ts->next_stop_2MHz_cycles-old);

#if LOGGING
    beeb_thread->m_audio_thread_data->num_executed_cycles.store(*ts->num_executed_2MHz_cycles,
                                                                std::memory_order_release);
#endif

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::BeebLinkResponseMessage::BeebLinkResponseMessage(std::vector<uint8_t> data)
    : m_data(std::move(data)) {
}

bool BeebThread::BeebLinkResponseMessage::ThreadPrepare(std::shared_ptr<Message> *ptr,
                                                        CompletionFun *completion_fun,
                                                        BeebThread *beeb_thread,
                                                        ThreadState *ts) {
    (void)completion_fun, (void)beeb_thread;

    if (ts->timeline_mode != BeebThreadTimelineMode_None) {
        return false;
    }

    ts->beeb->SendBeebLinkResponse(std::move(m_data));

    ptr->reset();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::KeyStates::GetState(BeebKey key) const {
    ASSERT(key >= 0 && (int)key < 128);

    uint8_t index = key >> 6 & 1;
    uint64_t mask = 1ull << (key & 63);

    uint64_t state = m_flags[index].load(std::memory_order_acquire) & mask;
    return !!state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::KeyStates::SetState(BeebKey key, bool state) {
    ASSERT(key >= 0 && (int)key < 128);

    uint8_t index = key >> 6 & 1;
    uint64_t mask = 1ull << (key & 63);

    if (state) {
        m_flags[index].fetch_or(mask, std::memory_order_acq_rel);
    } else {
        m_flags[index].fetch_and(~mask, std::memory_order_acq_rel);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::BeebThread(std::shared_ptr<MessageList> message_list,
                       uint32_t sound_device_id,
                       int sound_freq,
                       size_t sound_buffer_size_samples,
                       BeebLoadedConfig default_loaded_config,
                       std::vector<TimelineEventList> initial_timeline_event_lists)
    : m_uid(g_next_uid++)
    , m_default_loaded_config(std::move(default_loaded_config))
    , m_initial_timeline_event_lists(std::move(initial_timeline_event_lists))
    , m_video_output(NUM_VIDEO_UNITS)
    , m_sound_output(NUM_AUDIO_UNITS)
    , m_message_list(std::move(message_list)) {
    m_sound_device_id = sound_device_id;

    ASSERT(sound_freq >= 0);
    m_audio_thread_data = new AudioThreadData((uint64_t)sound_freq, (uint64_t)sound_buffer_size_samples, 100);

    this->SetBBCVolume(MAX_DB);
    this->SetDiscVolume(MAX_DB);

    MUTEX_SET_NAME(m_mutex, "BeebThread");
    m_mq.SetName("BeebThread MQ");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::~BeebThread() {
    this->Stop();

    delete m_audio_thread_data;
    m_audio_thread_data = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::Start() {
    try {
        m_thread = std::thread(std::bind(&BeebThread::ThreadMain, this));

        // Don't exit until the thread state is set up.
        //
        // I had a one-off crash due to the UI trying to do a LockBeeb
        // before ThreadMain had got as far as setting up
        // m_thread_state. ThreadMain could indeed take any amount of
        // time to get to that point, and the mutex isn't locked for
        // the whole period, so there's a potential race condition
        // here.
        //
        // (The spin loop is OK, under the circumstances, but should
        // replace this with some kind of unique_lock hand-off.)
        for (;;) {
            {
                std::lock_guard<Mutex> lock(m_mutex);

                if (m_thread_state) {
                    break;
                }
            }

            SleepMS(1);
        }
    } catch (const std::system_error &) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::Stop() {
    if (m_thread.joinable()) {
        this->Send(std::make_shared<StopMessage>());
        m_thread.join();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsStarted() const {
    return m_thread.joinable();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t BeebThread::GetEmulated2MHzCycles() const {
    return (uint64_t)m_num_2MHz_cycles;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

OutputDataBuffer<VideoDataUnit> *BeebThread::GetVideoOutput() {
    return &m_video_output;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::Send(std::shared_ptr<Message> message) {
    m_mq.ProducerPush(SentMessage{std::move(message)});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::Send(std::shared_ptr<Message> message,
                      Message::CompletionFun completion_fun) {
    m_mq.ProducerPush(SentMessage{std::move(message), std::move(completion_fun)});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendTimingMessage(uint64_t max_sound_units) {
    m_mq.ProducerPushIndexed(0, SentMessage{std::make_shared<TimingMessage>(max_sound_units)});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsPasting() const {
    return m_is_pasting.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsCopying() const {
    return m_is_copying.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const BBCMicro *BeebThread::LockBeeb(std::unique_lock<Mutex> *lock) const {
    *lock = std::unique_lock<Mutex>(m_mutex);

    return m_thread_state->beeb;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicro *BeebThread::LockMutableBeeb(std::unique_lock<Mutex> *lock) {
    *lock = std::unique_lock<Mutex>(m_mutex);

    return m_thread_state->beeb;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
const volatile TraceStats *BeebThread::GetTraceStats() const {
    if (m_is_tracing.load(std::memory_order_acquire)) {
        return &m_trace_stats;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsSpeedLimited() const {
    return m_is_speed_limited.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

float BeebThread::GetSpeedScale() const {
    return m_speed_scale.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool BeebThread::IsPaused() const {
//    std::lock_guard<Mutex> lock(m_mutex);
//
//    return m_paused;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BeebThread::GetDiscImage(std::unique_lock<Mutex> *lock, int drive) const {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);

    *lock = std::unique_lock<Mutex>(m_mutex);

    return m_disc_images[drive];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BeebThread::GetLEDs() const {
    return m_leds.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::GetKeyState(BeebKey beeb_key) const {
    ASSERT(beeb_key >= 0);
    return m_effective_key_states.GetState(beeb_key);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> BeebThread::GetNVRAM() const {
    std::lock_guard<Mutex> lock(m_mutex);

    std::vector<uint8_t> nvram = m_thread_state->beeb->GetNVRAM();
    return nvram;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::HasNVRAM() const {
    return m_has_nvram.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType *BeebThread::GetBBCMicroType() const {
    return m_beeb_type.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BeebThread::GetBBCMicroCloneImpediments() const {
    return m_clone_impediments.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ClearLastTrace() {
    std::lock_guard<Mutex> lock(m_mutex);

    m_last_trace = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<Trace> BeebThread::GetLastTrace() {
    std::lock_guard<Mutex> lock(m_mutex);

    return m_last_trace;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebThread::AudioThreadFillAudioBuffer(float *samples,
                                              size_t num_samples,
                                              bool perfect,
                                              void (*fn)(int,
                                                         float,
                                                         void *),
                                              void *fn_context) {
    AudioThreadData *const atd = m_audio_thread_data;
    uint64_t now_ticks = GetCurrentTickCount();

    ASSERT(num_samples == atd->sound_buffer_size_samples);

    if (!atd) {
        return 0;
    }

    bool limit_speed = this->IsSpeedLimited();

#if LOGGING
    bool print = false;

    if (m_sound_device_id != 0) {
        if (GetSecondsFromTicks(now_ticks - atd->last_print_ticks) >= LOGGING_FREQUENCY_SECONDS) {
            print = true;
            atd->last_print_ticks = now_ticks;
        }
    }
#endif

#if LOGGING
    if (print) {
        LOGF(ATH, "Data available: %zu units in %zu chunks\n", atd->sound_chunks.total_num_units, atd->sound_chunks.num_chunks);
    }
#endif

    uint64_t units_needed_now = atd->remapper.GetNumUnits(num_samples);
    uint64_t units_needed_future = atd->remapper.GetNumUnits(num_samples * 5 / 2);

    const SoundDataUnit *sa = nullptr, *sb = nullptr;
    size_t num_sa, num_sb;
    if (!m_sound_output.GetConsumerBuffers(&sa, &num_sa, &sb, &num_sb)) {
        num_sa = 0;
        num_sb = 0;
    }

    uint64_t units_available = num_sa + num_sb;

    AudioCallbackRecord *record = nullptr;
    if (!atd->records.empty()) {
        ASSERT(atd->record0_index < atd->records.size());
        record = &atd->records[atd->record0_index];

        ++atd->record0_index;
        atd->record0_index %= atd->records.size();

        record->time = now_ticks;
        record->needed = units_needed_now;
        record->available = units_available;
    }

    uint64_t max_sound_units;
    if (limit_speed) {
        max_sound_units = atd->num_consumed_sound_units + units_needed_future;
    } else {
        max_sound_units = UINT64_MAX;
    }
    this->SendTimingMessage(max_sound_units);

    if (units_available == 0) {
        // Can't really do much with this...
        return 0;
    }

    Remapper *remapper, temp_remapper;
    if (limit_speed) {
        if (units_needed_now <= units_available) {
            remapper = &atd->remapper;
        } else {
            if (perfect) {
                // bail out and try again later.
                //m_sound_output.ConsumerUnlock(0);
                return 0;
            }

            goto use_units_available;
        }
    } else {
    use_units_available:;
        // Underflow, or no speed limiting... just eat it all, however
        // much there is.
        temp_remapper = Remapper(num_samples, units_available);
        remapper = &temp_remapper;
    }

    float *dest = samples;

    float sn_scale = 1 / 4.f * atd->bbc_sound_scale;
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    float disc_sound_scale = 1.f * atd->disc_sound_scale;
#endif

#define MIXCH(CH) (VOLUMES_TABLE[unit->sn_output.ch[CH]])
#define MIXSN (sn_scale * (MIXCH(0) + MIXCH(1) + MIXCH(2) + MIXCH(3)))
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
#define MIXALL (disc_sound_scale * unit->disc_drive_sound + MIXSN)
#else
#define MIXALL MIXSN
#endif

#define MIX (*filter++ * MIXALL)

    // This functionality may return.
    (void)fn, (void)fn_context;
    ASSERT(!fn);
    ASSERT(!fn_context);

    float acc = 0.f;
    size_t num_units_left = num_sa;
    const SoundDataUnit *unit = sa;
    uint64_t num_consumed_sound_units = 0;

    for (size_t sample_idx = 0; sample_idx < num_samples; ++sample_idx) {
        uint64_t num_units_ = remapper->Step();
        ASSERT(num_units_ <= SIZE_MAX);
        size_t num_units = (size_t)num_units_;

        if (num_units > 0) {
            acc = 0.f;

            const float *filter;
            size_t filter_width;
            GetFilterForWidth(&filter, &filter_width, (size_t)num_units);
            ASSERT(filter_width <= num_units);

            if (num_units <= num_units_left) {
                // consume NUM_UNITS contiguous units from part A or
                // part B
                for (size_t i = 0; i < filter_width; ++i, ++unit) {
                    acc += MIX;
                }

                // (the filter may be shorter)
                unit += num_units - filter_width;

                num_units_left -= num_units;
            } else {
                size_t i = 0;

                // consume NUM_UNITS_LEFT contiguous units from the
                // end of part A, then NUM_UNITS-NUM_UNITS_LEFT
                // contiguous units from the start of part B.

                while (i < num_units_left) {
                    if (i < filter_width) {
                        acc += MIX;
                    }

                    ++i;
                    ++unit;
                }

                ASSERT(unit == sa + num_sa);

                ASSERT(num_sb >= num_units - num_units_left);
                num_units_left = num_sb - (num_units - num_units_left);
                unit = sb;

                while (i < num_units) {
                    if (i < filter_width) {
                        acc += MIX;
                    }

                    ++i;
                    ++unit;
                }
            }

            m_sound_output.Consume(num_units);
            num_consumed_sound_units += num_units;
        }

        dest[sample_idx] = acc;
    }

    atd->num_consumed_sound_units += num_consumed_sound_units;
    //printf("%s: needed now=%" PRIu64 "; available=%" PRIu64 "; consumed=%" PRIu64 "; needed future=%" PRIu64 "\n",__func__,units_needed_now,units_available,num_consumed_sound_units,units_needed_future);

    return num_samples;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetBBCVolume(float db) {
    this->SetVolume(&m_audio_thread_data->bbc_sound_scale, db);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetDiscVolume(float db) {
    this->SetVolume(&m_audio_thread_data->disc_sound_scale, db);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetPowerOnTone(bool power_on_tone) {
    m_power_on_tone.store(power_on_tone, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<BeebThread::AudioCallbackRecord> BeebThread::GetAudioCallbackRecords() const {
    std::vector<AudioCallbackRecord> records;
    records.reserve(m_audio_thread_data->records.size());

    {
        AudioDeviceLock lock(m_sound_device_id);

        for (size_t i = 0; i < m_audio_thread_data->records.size(); ++i) {
            size_t index = (m_audio_thread_data->record0_index + i) % m_audio_thread_data->records.size();
            records.emplace_back(m_audio_thread_data->records[index]);
        }
    }

    return records;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::GetTimelineState(BeebThreadTimelineState *timeline_state) const {
    std::lock_guard<Mutex> lock(m_mutex);

    *timeline_state = m_timeline_state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebThread::GetNumTimelineBeebStateEvents() const {
    std::lock_guard<Mutex> lock(m_mutex);

    return m_timeline_beeb_state_events_copy.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsDriveWriteProtected(int drive) const {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);

    return m_is_drive_write_protected[drive].load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<BeebThread::TimelineBeebStateEvent>
BeebThread::GetTimelineBeebStateEvents(size_t begin_index,
                                       size_t end_index) {
    ASSERT(begin_index <= PTRDIFF_MAX);
    ASSERT(end_index <= PTRDIFF_MAX);
    ASSERT(end_index >= begin_index);

    std::lock_guard<Mutex> lock(m_mutex);

    begin_index = std::min(begin_index, m_timeline_beeb_state_events_copy.size());
    end_index = std::min(end_index, m_timeline_beeb_state_events_copy.size());

    std::vector<TimelineBeebStateEvent> result(m_timeline_beeb_state_events_copy.begin() + (ptrdiff_t)begin_index,
                                               m_timeline_beeb_state_events_copy.begin() + (ptrdiff_t)end_index);
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
bool BeebThread::ThreadHandleTraceInstructionConditions(const BBCMicro *beeb,
                                                        const M6502 *cpu,
                                                        void *context) {
    (void)beeb;
    auto ts = (ThreadState *)context;

    switch (ts->trace_state) {
    case BeebThreadTraceState_None:
        return false;

    case BeebThreadTraceState_Waiting:
        switch (ts->trace_conditions.start) {
        default:
            // start condition not instruction-related, but leave the
            // callback in, as the stop condition presumably is...
            break;

        case BeebThreadStartTraceCondition_Instruction:
            // ugh.
            if (cpu->abus.w == ts->trace_conditions.start_address) {
                ts->beeb_thread->ThreadBeebStartTrace(ts);
            }
            break;
        }
        break;

    case BeebThreadTraceState_Tracing:
        switch (ts->trace_conditions.stop) {
        default:
            // stop condition not instruction-related, so no need for
            // the callback any more.
            return false;

        case BeebThreadStopTraceCondition_OSWORD0:
            if (cpu->pc.w == 0xfff2 && cpu->a == 0) {
                ts->beeb_thread->ThreadStopTrace(ts);
                return false;
            }
            break;

        case BeebThreadStopTraceCondition_NumCycles:
            if (*ts->num_executed_2MHz_cycles - ts->trace_start_2MHz_cycles >= ts->trace_conditions.stop_num_cycles) {
                ts->beeb_thread->ThreadStopTrace(ts);
                return false;
            }
            break;
        }
        break;
    }

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
bool BeebThread::ThreadHandleTraceWriteConditions(const BBCMicro *beeb,
                                                  const M6502 *cpu,
                                                  void *context) {
    (void)beeb;
    auto ts = (BeebThread::ThreadState *)context;

    switch (ts->trace_state) {
    case BeebThreadTraceState_None:
        return false;

    case BeebThreadTraceState_Waiting:
        switch (ts->trace_conditions.start) {
        default:
            // start condition not write-related, but leave the
            // callback in, as the stop condition presumably is...
            break;

        case BeebThreadStartTraceCondition_WriteAddress:
            if (cpu->abus.w == ts->trace_conditions.start_address) {
                ts->beeb_thread->ThreadBeebStartTrace(ts);
            }
            break;
        }
        break;

    case BeebThreadTraceState_Tracing:
        switch (ts->trace_conditions.stop) {
        default:
            // stop condition not write-related, so no need for
            // the callback any more.
            return false;

        case BeebThreadStopTraceCondition_WriteAddress:
            if (cpu->abus.w == ts->trace_conditions.stop_address) {
                ts->beeb_thread->ThreadStopTrace(ts);
                return false;
            }
            break;
        }
        break;
    }

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadStopCopyOnOSWORD0(const BBCMicro *beeb, const M6502 *cpu, void *context) {
    (void)beeb;
    auto ts = (ThreadState *)context;

    if (!ts->beeb_thread->m_is_copying) {
        return false;
    }

    if (cpu->pc.w == 0xfff2 && cpu->a == 0) {
        if (!ts->beeb->IsPasting()) {
            ts->beeb_thread->ThreadStopCopy(ts);
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadAddCopyData(const BBCMicro *beeb, const M6502 *cpu, void *context) {
    (void)beeb;
    auto ts = (ThreadState *)context;

    if (!ts->beeb_thread->m_is_copying) {
        return false;
    }

    const uint8_t *ram = beeb->GetRAM();

    // Rather tiresomely, BASIC 2 prints stuff with JMP (WRCHV). Who
    // comes up with this stuff? So check against WRCHV, not just
    // 0xffee.

    if (cpu->abus.b.l == ram[0x020e] && cpu->abus.b.h == ram[0x020f]) {
        // Opcode fetch for first byte of OSWRCH
        ts->copy_data.push_back(cpu->a);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<BeebState> BeebThread::ThreadSaveState(ThreadState *ts) {
    std::unique_ptr<BBCMicro> clone_beeb = ts->beeb->Clone();

    if (!clone_beeb) {
        return nullptr;
    }

    auto state = std::make_shared<BeebState>(std::move(clone_beeb));
    return state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadReplaceBeeb(ThreadState *ts, std::unique_ptr<BBCMicro> beeb, uint32_t flags) {
    ASSERT(!!beeb);

    std::shared_ptr<DiscImage> old_disc_images[NUM_DRIVES];
    {
#if BBCMICRO_DEBUGGER
        std::unique_ptr<BBCMicro::DebugState> debug_state;
#endif

        if (ts->beeb) {
#if BBCMICRO_DEBUGGER
            debug_state = ts->beeb->TakeDebugState();
#endif

            if (flags & BeebThreadReplaceFlag_KeepCurrentDiscs) {
                for (int i = 0; i < NUM_DRIVES; ++i) {
                    old_disc_images[i] = ts->beeb->TakeDiscImage(i);
                }
            }

            delete ts->beeb;
            ts->beeb = nullptr;
        }

#if BBCMICRO_DEBUGGER
        if (!debug_state) {
            // Probably just the first time round.
            ts->log.f("Creating new BBCMicro::DebugState.\n");
            debug_state = std::make_unique<BBCMicro::DebugState>();
        }
#endif

        ts->beeb = beeb.release();

#if BBCMICRO_DEBUGGER
        ts->beeb->SetDebugState(std::move(debug_state));
#endif

        Message::CallCompletionFun(&ts->reset_completion_fun, false, nullptr);
        Message::CallCompletionFun(&ts->paste_completion_fun, false, nullptr);
#if BBCMICRO_DEBUGGER
        Message::CallCompletionFun(&ts->async_call_completion_fun, false, nullptr);
#endif
    }

    ts->num_executed_2MHz_cycles = ts->beeb->GetNum2MHzCycles();

    {
        AudioDeviceLock lock(m_sound_device_id);

        m_audio_thread_data->num_consumed_sound_units = *ts->num_executed_2MHz_cycles >> SOUND_CLOCK_SHIFT;
    }

    m_has_nvram.store(!ts->beeb->GetNVRAM().empty(), std::memory_order_release);
    m_beeb_type.store(ts->beeb->GetType(), std::memory_order_release);

    // Apply current keypresses to the emulated BBC. Reset fake shift
    // state and boot state first so that the Shift key status is set
    // properly.
    this->ThreadSetBootState(ts, false);
    this->ThreadSetFakeShiftState(ts, BeebShiftState_Any);

    if (flags & BeebThreadReplaceFlag_ResetKeyState) {
        // Set BBC state from shadow state.
        for (int i = 0; i < 128; ++i) {
            this->ThreadSetKeyState(ts, (BeebKey)i, false);
        }
    } else {
        // Set shadow state from BBC state.
        for (int i = 0; i < 128; ++i) {
            bool state = !!ts->beeb->GetKeyState((BeebKey)i);
            this->ThreadSetKeyState(ts, (BeebKey)i, state);
        }
    }

    if (flags & BeebThreadReplaceFlag_KeepCurrentDiscs) {
        for (int i = 0; i < NUM_DRIVES; ++i) {
            this->ThreadSetDiscImage(ts, i, std::move(old_disc_images[i]));
        }
    } else {
        for (int i = 0; i < NUM_DRIVES; ++i) {
            m_disc_images[i] = ts->beeb->GetDiscImage(i);
        }
    }

#if BBCMICRO_TRACE
    if (m_is_tracing.load(std::memory_order_acquire)) {
        this->ThreadStartTrace(ts);
    }
#endif

    ts->beeb->GetAndResetDiscAccessFlag();
    this->ThreadSetBootState(ts, !!(flags & BeebThreadReplaceFlag_Autoboot));

    //m_paused=false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebThread::ThreadStartTrace(ThreadState *ts) {
    ts->trace_state = BeebThreadTraceState_Waiting;

    bool any_instruction_condition = false;
    bool any_write_condition = false;

    switch (ts->trace_conditions.start) {
    default:
        ASSERT(false);
        // fall through
    case BeebThreadStartTraceCondition_Immediate:
        // Start now.
        this->ThreadBeebStartTrace(ts);
        break;

    case BeebThreadStartTraceCondition_NextKeypress:
        // Wait for the key...
        break;

    case BeebThreadStartTraceCondition_Instruction:
        any_instruction_condition = true;
        ts->beeb->AddInstructionFn(&ThreadHandleTraceInstructionConditions, ts);
        break;

    case BeebThreadStartTraceCondition_WriteAddress:
        any_write_condition = true;
        break;
    }

    //ts->beeb->SetInstructionTraceEventFn(nullptr,nullptr);

    switch (ts->trace_conditions.stop) {
    default:
        ASSERT(false);
        // fall through
    case BeebThreadStopTraceCondition_ByRequest:
        // By request...
        break;

    case BeebThreadStopTraceCondition_OSWORD0:
    case BeebThreadStopTraceCondition_NumCycles:
        any_instruction_condition = true;
        break;

    case BeebThreadStopTraceCondition_WriteAddress:
        any_write_condition = true;
        break;
    }

    if (any_instruction_condition) {
        ts->beeb->AddInstructionFn(&ThreadHandleTraceInstructionConditions, ts);
    }

    if (any_write_condition) {
        ts->beeb->AddWriteFn(&ThreadHandleTraceWriteConditions, ts);
    }

    m_trace_stats = {};
    m_is_tracing.store(true, std::memory_order_release);
    m_last_trace = nullptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebThread::ThreadBeebStartTrace(ThreadState *ts) {
    ts->trace_start_2MHz_cycles = *ts->num_executed_2MHz_cycles;
    ts->trace_state = BeebThreadTraceState_Tracing;
    ts->beeb->StartTrace(ts->trace_conditions.trace_flags, ts->trace_max_num_bytes);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebThread::ThreadStopTrace(ThreadState *ts) {
    ASSERT(ts->beeb);

    m_trace_stats = {};
    m_is_tracing.store(false, std::memory_order_release);

    ts->beeb->StopTrace(&m_last_trace);

    ts->trace_state = BeebThreadTraceState_None;
    ts->trace_conditions = TraceConditions();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetKeyState(ThreadState *ts, BeebKey beeb_key, bool state) {
    ASSERT(!(beeb_key & 0x80));

    if (IsNumericKeypadKey(beeb_key) && !ts->beeb->HasNumericKeypad()) {
        // Ignore numeric keypad key for Model B.
    } else {
        // Always set the key flags as requested.
        m_real_key_states.SetState(beeb_key, state);

        // If it's the shift key, override using fake shift flags or
        // boot flag.
        if (beeb_key == BeebKey_Shift) {
            if (ts->boot) {
                state = true;
            } else if (ts->fake_shift_state == BeebShiftState_On) {
                state = true;
            } else if (ts->fake_shift_state == BeebShiftState_Off) {
                state = false;
            }
        } else {
            if (ts->boot) {
                // this is recursive, but it only calls
                // ThreadSetKeyState for BeebKey_Shift, so it won't
                // end up here again...
                this->ThreadSetBootState(ts, false);
            } else {
                // no harm in skipping it.
            }
        }

        m_effective_key_states.SetState(beeb_key, state);
        ts->beeb->SetKeyState(beeb_key, state);

#if BBCMICRO_TRACE
        if (ts->trace_conditions.start == BeebThreadStartTraceCondition_NextKeypress) {
            if (m_is_tracing.load(std::memory_order_acquire)) {
                if (state) {
                    if (ts->trace_conditions.start_key < 0 || beeb_key == ts->trace_conditions.start_key) {
                        ts->trace_conditions.start = BeebThreadStartTraceCondition_Immediate;
                        this->ThreadBeebStartTrace(ts);
                    }
                }
            }
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetFakeShiftState(ThreadState *ts, BeebShiftState state) {
    ts->fake_shift_state = state;

    this->ThreadUpdateShiftKeyState(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetBootState(ThreadState *ts, bool state) {
    ts->boot = state;

    this->ThreadUpdateShiftKeyState(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadUpdateShiftKeyState(ThreadState *ts) {
    this->ThreadSetKeyState(ts, BeebKey_Shift, m_real_key_states.GetState(BeebKey_Shift));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetDiscImage(ThreadState *ts, int drive, std::shared_ptr<DiscImage> disc_image) {
    ts->beeb->SetDiscImage(drive, disc_image);

    m_disc_images[drive] = disc_image;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStartPaste(ThreadState *ts,
                                  std::shared_ptr<const std::string> text) {
    //auto shared_text=std::make_shared<std::string>(std::move(text));

    //this->ThreadRecordEvent(ts,BeebEvent::MakeStartPaste(*ts->num_executed_2MHz_cycles,shared_text));
    ts->beeb->StartPaste(std::move(text));
    m_is_pasting.store(true, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStopCopy(ThreadState *ts) {
    ASSERT(m_is_copying);

    if (ts->copy_basic) {
        if (!ts->copy_data.empty()) {
            if (ts->copy_data.back() == '>') {
                ts->copy_data.pop_back();
            }

            if (ts->copy_data.size() >= COPY_BASIC_PREFIX.size()) {
                auto begin = ts->copy_data.begin();

                auto end = begin + (ptrdiff_t)COPY_BASIC_PREFIX.size();

                if (std::equal(begin, end, COPY_BASIC_PREFIX.begin())) {
                    ts->copy_data.erase(begin, end);
                }
            }
        }
    }

    PushFunctionMessage([data = std::move(ts->copy_data), fun = std::move(ts->copy_stop_fun)]() {
        fun(std::move(data));
    });

    m_is_copying.store(false, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadMain(void) {
    ThreadState ts;

    {
        std::lock_guard<Mutex> lock(m_mutex);

        SetCurrentThreadNamef("BeebThread");

        ts.beeb_thread = this;
        ts.msgs = Messages(m_message_list);
        ts.timeline_event_lists = std::move(m_initial_timeline_event_lists);

        m_timeline_state.num_beeb_state_events = ts.timeline_event_lists.size();

        m_timeline_state.num_events = 0;
        for (const TimelineEventList &list : ts.timeline_event_lists) {
            m_timeline_state.num_events += 1 + list.events.size();
            m_timeline_beeb_state_events_copy.push_back(list.state_event);
        }

        if (!ts.timeline_event_lists.empty()) {
            ts.timeline_end_event.time_2MHz_cycles = ts.timeline_event_lists.back().events.back().time_2MHz_cycles;
        }

        this->ThreadCheckTimeline(&ts);

        ts.current_config = std::move(m_default_loaded_config);

        m_thread_state = &ts;
    }

    std::vector<SentMessage> messages;
    size_t total_num_audio_units_produced = 0;

    int handle_messages_reason;
    (void)handle_messages_reason;
    for (;;) {
        handle_messages_reason = -1;
    handle_messages:
        bool paused = false;
        if (!ts.beeb) {
            paused = true;
        } else {
#if BBCMICRO_DEBUGGER
            if (ts.beeb->DebugIsHalted()) {
                paused = true;
            }
#endif
        }

        const char *what;
        (void)what;

        if (paused ||
            (m_is_speed_limited.load(std::memory_order_acquire) &&
             ts.next_stop_2MHz_cycles <= *ts.num_executed_2MHz_cycles)) {
            rmt_ScopedCPUSample(MessageQueueWaitForMessage, 0);
            m_mq.ConsumerWaitForMessages(&messages);
            what = "waited";
        } else {
            m_mq.ConsumerPollForMessages(&messages);
            what = "polled";
        }

        //printf("%s: %s/%d: %zu message(s), cycles=%" PRIu64 ", stop=%" PRIu64 ", total_num_audio_units_produced=%zu\n",__func__,what,handle_messages_reason,messages.size(),ts.num_executed_2MHz_cycles?*ts.num_executed_2MHz_cycles:0,ts.next_stop_2MHz_cycles,total_num_audio_units_produced);
        total_num_audio_units_produced = 0;

        uint64_t stop_2MHz_cycles;

        //if(!messages.empty())
        {
            std::lock_guard<Mutex> lock(m_mutex);

            for (auto &&m : messages) {
                bool prepared = m.message->ThreadPrepare(&m.message, &m.completion_fun, this, &ts);
                if (!prepared) {
                    Message::CallCompletionFun(&m.completion_fun, false, nullptr);
                    continue;
                }

                if (!!m.message) {
                    m.message->ThreadHandle(this, &ts);

                    Message::CallCompletionFun(&m.completion_fun, true, nullptr);

                    if (ts.timeline_mode == BeebThreadTimelineMode_Record) {
                        ASSERT(!ts.timeline_event_lists.empty());
                        TimelineEvent event{*ts.num_executed_2MHz_cycles, std::move(m.message)};
                        ts.timeline_event_lists.back().events.emplace_back(std::move(event));
                        ++m_timeline_state.num_events;
                    }
                }

                if (ts.stop) {
                    goto done;
                }
            }

            stop_2MHz_cycles = ts.next_stop_2MHz_cycles;

            messages.clear();

            uint32_t clone_impediments = ts.beeb->GetCloneImpediments();

            m_clone_impediments.store(clone_impediments, std::memory_order_release);

            // Update ThreadState timeline stuff.
            switch (ts.timeline_mode) {
            case BeebThreadTimelineMode_None:
                m_timeline_state.clone_impediments = clone_impediments;
                m_timeline_state.can_record = m_timeline_state.clone_impediments == 0;
                break;

            case BeebThreadTimelineMode_Record:
                {
                    m_timeline_state.can_record = false;

                    ASSERT(!ts.timeline_event_lists.empty());
                    ts.timeline_end_event.time_2MHz_cycles = *ts.num_executed_2MHz_cycles;

                    const TimelineEventList *list = &ts.timeline_event_lists.back();

                    ASSERT(*ts.num_executed_2MHz_cycles >= list->state_event.time_2MHz_cycles);
                    if (*ts.num_executed_2MHz_cycles - list->state_event.time_2MHz_cycles >= TIMELINE_SAVE_STATE_FREQUENCY_2MHz_CYCLES) {
                        if (list->events.empty()) {
                            // There have been no events since the last save
                            // event. Don't bother saving a new one.
                        } else {
                            if (!this->ThreadRecordSaveState(&ts, true)) {
                                // ugh, something went wrong :(
                                this->ThreadStopRecording(&ts);
                                ts.msgs.e.f("Internal error - failed to save state.\n");
                            }
                        }
                    }
                }
                break;

            case BeebThreadTimelineMode_Replay:
                {
                    m_timeline_state.can_record = false;

                    const TimelineEvent *next_event = this->ThreadGetNextReplayEvent(&ts);
                    LOGF(REPLAY, "next_event: %" PRIu64 "; num executed: %" PRIu64 "\n", next_event->time_2MHz_cycles, *ts.num_executed_2MHz_cycles);
                    ASSERT(*ts.num_executed_2MHz_cycles <= next_event->time_2MHz_cycles);
                    if (*ts.num_executed_2MHz_cycles == next_event->time_2MHz_cycles) {
                        if (!!next_event->message) {
                            LOGF(REPLAY, "handle event: %" PRIu64 "\n", next_event->time_2MHz_cycles);
                            next_event->message->ThreadHandle(this, &ts);
                            this->ThreadNextReplayEvent(&ts);
                            next_event = this->ThreadGetNextReplayEvent(&ts);
                            LOGF(REPLAY, "new next_event: %" PRIu64 "\n", next_event->time_2MHz_cycles);
                        } else {
                            // end of timeline
                            this->ThreadStopReplay(&ts);
                        }
                    }

                    if (next_event->time_2MHz_cycles < stop_2MHz_cycles) {
                        stop_2MHz_cycles = next_event->time_2MHz_cycles;
                        LOGF(REPLAY, "stop cycles: %" PRIu64 "\n", stop_2MHz_cycles);
                    }
                }
                break;
            }

            //m_timeline_state.num_events=ts.timeline_events.size();
            if (ts.timeline_event_lists.empty()) {
                ASSERT(m_timeline_state.num_events == 0);
                m_timeline_state.begin_2MHz_cycles = 0;
                m_timeline_state.end_2MHz_cycles = m_timeline_state.begin_2MHz_cycles;
                m_timeline_state.num_beeb_state_events = 0;
            } else {
                m_timeline_state.begin_2MHz_cycles = ts.timeline_event_lists[0].state_event.time_2MHz_cycles;
                m_timeline_state.end_2MHz_cycles = ts.timeline_end_event.time_2MHz_cycles;
                m_timeline_state.num_beeb_state_events = ts.timeline_event_lists.size();
            }

            m_timeline_state.current_2MHz_cycles = *ts.num_executed_2MHz_cycles;
            m_timeline_state.mode = ts.timeline_mode;
        }

        //        if(m_is_replaying) {
        //            ASSERT(*ts.num_executed_2MHz_cycles<=ts.replay_next_event_cycles);
        //            if(*ts.num_executed_2MHz_cycles==ts.replay_next_event_cycles) {
        //                this->ThreadHandleReplayEvents(&ts);
        //            }
        //
        //            if(m_is_replaying) {
        //                if(ts.replay_next_event_cycles<stop_2MHz_cycles) {
        //                    stop_2MHz_cycles=ts.replay_next_event_cycles;
        //                }
        //            }
        //        }

        if (m_is_pasting) {
            if (!ts.beeb->IsPasting()) {
                m_is_pasting.store(false, std::memory_order_release);
                Message::CallCompletionFun(&ts.paste_completion_fun, true, nullptr);
            }
        }

        // TODO - can ts.beeb actually ever be null? I can't remember...
        if (ts.beeb) {
            m_leds.store(ts.beeb->GetLEDs(), std::memory_order_release);

#if BBCMICRO_TRACE
            ts.beeb->GetTraceStats(&m_trace_stats);
#endif
        }

        if (ts.boot) {
            if (ts.beeb->GetAndResetDiscAccessFlag()) {
                this->ThreadSetBootState(&ts, false);
            }
        }

        if (!paused && stop_2MHz_cycles > *ts.num_executed_2MHz_cycles) {
            rmt_ScopedCPUSample(BeebUpdate, 0);

            ASSERT(ts.beeb);

            uint64_t num_2MHz_cycles = stop_2MHz_cycles - *ts.num_executed_2MHz_cycles;
            if (num_2MHz_cycles > RUN_2MHz_CYCLES) {
                num_2MHz_cycles = RUN_2MHz_CYCLES;
            }

            // One day I'll clear up the units mismatch...
            VideoDataUnit *va, *vb;
            size_t num_va, num_vb;
            size_t num_video_units = (size_t)num_2MHz_cycles;
            if (!m_video_output.GetProducerBuffers(&va, &num_va, &vb, &num_vb)) {
                handle_messages_reason = 0;
                goto handle_messages;
            }

            if (num_va + num_vb > num_video_units) {
                if (num_va > num_video_units) {
                    num_va = num_video_units;
                    num_vb = 0;
                } else {
                    num_vb = num_video_units - num_va;
                }
            }

            size_t num_sound_units = (size_t)((num_va + num_vb + (1 << SOUND_CLOCK_SHIFT) - 1) >> SOUND_CLOCK_SHIFT);

            SoundDataUnit *sa, *sb;
            size_t num_sa, num_sb;
            if (!m_sound_output.GetProducerBuffers(&sa, &num_sa, &sb, &num_sb)) {
                handle_messages_reason = 1;
                goto handle_messages;
            }

            if (num_sa + num_sb < num_sound_units) {
                handle_messages_reason = 2;
                goto handle_messages;
            }

            SoundDataUnit *sunit = sa;

            total_num_audio_units_produced += num_sound_units;

            {
                VideoDataUnit *vunit;
                size_t i;
                std::unique_lock<Mutex> lock(m_mutex, std::defer_lock);

                // A.
                {
                    vunit = va;

                    for (i = 0; i < num_va; ++i) {
                        if (!lock.owns_lock()) {
                            lock.lock();
                        }

#if BBCMICRO_DEBUGGER
                        if (ts.beeb->DebugIsHalted()) {
                            break;
                        }
#endif

                        if (ts.beeb->Update(vunit++, sunit)) {
                            lock.unlock();

                            ++sunit;

                            if (sunit == sa + num_sa) {
                                sunit = sb;
                            } else if (sunit == sb + num_sb) {
                                sunit = nullptr;
                            }

                            m_sound_output.Produce(1);
                        }
                    }

                    m_video_output.Produce(i);
                }

                if (!lock.owns_lock()) {
                    lock.lock();
                }

                // B.
#if BBCMICRO_DEBUGGER                          //////////////////////////<--note
                if (!ts.beeb->DebugIsHalted()) //<--note
#endif                                         /////////////////////////////////////////<--note
                {                              //////////////////////////////<--note
                    vunit = vb;

                    for (i = 0; i < num_vb; ++i) {
                        if (!lock.owns_lock()) {
                            lock.lock();
                        }

#if BBCMICRO_DEBUGGER
                        if (ts.beeb->DebugIsHalted()) {
                            break;
                        }
#endif

                        if (ts.beeb->Update(vunit++, sunit)) {
                            lock.unlock();

                            ++sunit;

                            if (sunit == sa + num_sa) {
                                sunit = sb;
                            } else if (sunit == sb + num_sb) {
                                sunit = nullptr;
                            }

                            m_sound_output.Produce(1);
                        }
                    }

                    m_video_output.Produce(i);
                }
            }

            // It's a bit dumb having multiple copies.
            m_num_2MHz_cycles.store(*ts.num_executed_2MHz_cycles, std::memory_order_release);
        }
    }
done:
    {
        std::lock_guard<Mutex> lock(m_mutex);

        m_thread_state = nullptr;

        delete ts.beeb;
        ts.beeb = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetVolume(float *scale_var, float db) {
    if (db > MAX_DB) {
        db = MAX_DB;
    }

    if (db < MIN_DB) {
        db = MIN_DB;
    }

    {
        AudioDeviceLock lock(m_sound_device_id);

        *scale_var = powf(10.f, db / 20.f);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadRecordSaveState(ThreadState *ts, bool user_initiated) {
    this->ThreadCheckTimeline(ts);

    std::unique_ptr<BBCMicro> clone = ts->beeb->Clone();
    if (!clone) {
        return false;
    }

    auto message = std::make_shared<BeebStateMessage>(std::make_unique<BeebState>(std::move(clone)),
                                                      user_initiated);

    uint64_t time = *ts->num_executed_2MHz_cycles;

    TimelineEventList list;
    list.state_event = {time, std::move(message)};

    m_timeline_beeb_state_events_copy.push_back(list.state_event);

    ts->timeline_event_lists.push_back(std::move(list));

    ++m_timeline_state.num_events;

    this->ThreadCheckTimeline(ts);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStopRecording(ThreadState *ts) {
    ts->timeline_end_event.time_2MHz_cycles = *ts->num_executed_2MHz_cycles;
    ts->timeline_mode = BeebThreadTimelineMode_None;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadClearRecording(ThreadState *ts) {
    this->ThreadCheckTimeline(ts);

    this->ThreadStopRecording(ts);

    ts->timeline_event_lists.clear();
    m_timeline_state.num_events = 0;
    m_timeline_beeb_state_events_copy.clear();

    this->ThreadCheckTimeline(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadCheckTimeline(ThreadState *ts) {
    size_t num_events = 0;

    ASSERT(ts->timeline_event_lists.size() == m_timeline_beeb_state_events_copy.size());

    if (!ts->timeline_event_lists.empty()) {
        const TimelineEventList *pe = nullptr;
        for (size_t i = 0; i < ts->timeline_event_lists.size(); ++i) {
            const TimelineEventList *e = &ts->timeline_event_lists[i];

            ASSERT(e->state_event.time_2MHz_cycles == m_timeline_beeb_state_events_copy[i].time_2MHz_cycles);
            ASSERT(e->state_event.message == m_timeline_beeb_state_events_copy[i].message);
            ASSERT(e->state_event.time_2MHz_cycles == e->state_event.message->GetBeebState()->GetEmulated2MHzCycles());

            num_events += 1 + e->events.size();

            if (pe) {
                ASSERT(e->state_event.time_2MHz_cycles >= pe->state_event.time_2MHz_cycles);

                for (size_t j = 0; j < pe->events.size(); ++j) {
                    ASSERT(pe->events[j].time_2MHz_cycles <= e->state_event.time_2MHz_cycles);
                }
            }

            for (size_t j = 0; j < e->events.size(); ++j) {
                ASSERT(e->events[j].time_2MHz_cycles >= e->state_event.time_2MHz_cycles);
                if (j > 0) {
                    ASSERT(e->events[j].time_2MHz_cycles >= e->events[j - 1].time_2MHz_cycles);
                }
            }

            pe = e;
        }

        if (pe->events.empty()) {
            ASSERT(ts->timeline_end_event.time_2MHz_cycles >= pe->state_event.time_2MHz_cycles);
        } else {
            ASSERT(ts->timeline_end_event.time_2MHz_cycles >= pe->events.back().time_2MHz_cycles);
        }
        ASSERT(!ts->timeline_end_event.message);
    }

    ASSERT(m_timeline_state.num_events == num_events);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadDeleteTimelineState(ThreadState *ts,
                                           const std::shared_ptr<const BeebState> &state) {
    this->ThreadCheckTimeline(ts);

    size_t index;
    if (!this->ThreadFindTimelineEventListIndexByBeebState(ts, &index, state)) {
        return;
    }

    TimelineEventList *list = &ts->timeline_event_lists[index];

    if (index == 0) {
        // This list's events are going away.
        m_timeline_state.num_events -= list->events.size();
    } else {
        // Join events to previous state.
        TimelineEventList *prev = list - 1;
        prev->events.insert(prev->events.end(),
                            std::make_move_iterator(list->events.begin()),
                            std::make_move_iterator(list->events.end()));
    }

    // Account for removal of this state.
    --m_timeline_state.num_events;

    // Remove.
    m_timeline_beeb_state_events_copy.erase(m_timeline_beeb_state_events_copy.begin() + (ptrdiff_t)index);
    ts->timeline_event_lists.erase(ts->timeline_event_lists.begin() + (ptrdiff_t)index);

    this->ThreadCheckTimeline(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadTruncateTimeline(ThreadState *ts,
                                        const std::shared_ptr<const BeebState> &state) {
    this->ThreadCheckTimeline(ts);

    size_t index;
    if (!this->ThreadFindTimelineEventListIndexByBeebState(ts, &index, state)) {
        return; // Umm...
    }

    TimelineEventList *list = &ts->timeline_event_lists[index];

    // Account for removal of this state's events.
    m_timeline_state.num_events -= list->events.size();

    // Account for removal of subsequent states and their events.
    for (size_t i = index + 1; i < ts->timeline_event_lists.size(); ++i) {
        m_timeline_state.num_events -= 1 + ts->timeline_event_lists[i].events.size();
    }

    // Remove this state's events.
    list->events.clear();

    // Remove subsequent states.
    ts->timeline_event_lists.erase(ts->timeline_event_lists.begin() + (ptrdiff_t)index + 1,
                                   ts->timeline_event_lists.end());

    // The given state is now the end of the timeline.
    ts->timeline_end_event.time_2MHz_cycles = list->state_event.time_2MHz_cycles;

    // Truncate the copy list as well.
    m_timeline_beeb_state_events_copy.erase(m_timeline_beeb_state_events_copy.begin() + (ptrdiff_t)index + 1,
                                            m_timeline_beeb_state_events_copy.end());

    this->ThreadCheckTimeline(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadFindTimelineEventListIndexByBeebState(ThreadState *ts,
                                                             size_t *index,
                                                             const std::shared_ptr<const BeebState> &state) {
    this->ThreadCheckTimeline(ts);

    size_t n = ts->timeline_event_lists.size();
    ASSERT(n < PTRDIFF_MAX);

    for (size_t i = 0; i < n; ++i) {
        if (ts->timeline_event_lists[i].state_event.message->GetBeebState() == state) {
            *index = i;
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebThread::TimelineEvent *BeebThread::ThreadGetNextReplayEvent(ThreadState *ts) {
    ASSERT(ts->timeline_mode == BeebThreadTimelineMode_Replay);

    ASSERT(ts->timeline_replay_list_index <= ts->timeline_event_lists.size());
    if (ts->timeline_replay_list_index == ts->timeline_event_lists.size()) {
        // Next event is the end event.
        return &ts->timeline_end_event;
    } else {
        const TimelineEventList *list = &ts->timeline_event_lists[ts->timeline_replay_list_index];
        ASSERT(ts->timeline_replay_list_event_index < list->events.size());
        return &list->events[ts->timeline_replay_list_event_index];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadNextReplayEvent(ThreadState *ts) {
    ASSERT(ts->timeline_mode == BeebThreadTimelineMode_Replay);
    ASSERT(ts->timeline_replay_list_index < ts->timeline_event_lists.size());

    LOGF(REPLAY, "%s: before: list index=%zu/%zu, event index=%zu/%zu\n",
         __func__,
         ts->timeline_replay_list_index, ts->timeline_event_lists.size(),
         ts->timeline_replay_list_event_index, ts->timeline_event_lists[ts->timeline_replay_list_index].events.size());

    ++ts->timeline_replay_list_event_index;
    ASSERT(ts->timeline_replay_list_event_index <= ts->timeline_event_lists[ts->timeline_replay_list_index].events.size());
    if (ts->timeline_replay_list_event_index == ts->timeline_event_lists[ts->timeline_replay_list_index].events.size()) {
        ts->timeline_replay_list_event_index = 0;
        while (++ts->timeline_replay_list_index < ts->timeline_event_lists.size() &&
               ts->timeline_event_lists[ts->timeline_replay_list_index].events.empty()) {
        }
    }

    LOGF(REPLAY, "%s: after: list index=%zu, event index=%zu\n",
         __func__,
         ts->timeline_replay_list_index,
         ts->timeline_replay_list_event_index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStopReplay(ThreadState *ts) {
    ts->timeline_mode = BeebThreadTimelineMode_None;

    if (!!ts->timeline_replay_old_state) {
        this->ThreadReplaceBeeb(ts, ts->timeline_replay_old_state->CloneBBCMicro(), 0);
        ts->timeline_replay_old_state.reset();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadWaitForHardReset(const BBCMicro *beeb, const M6502 *cpu, void *context) {
    (void)beeb;
    auto ts = (ThreadState *)context;

    // Watch for OSWORD 0, OSRDCH, or 5 seconds.
    //
    // TODO - does timeout mean the request actually failed?
    if ((cpu->opcode_pc.w == 0xfff1 && cpu->a == 0) ||
        cpu->opcode_pc.w == 0xffe0 ||
        *ts->num_executed_2MHz_cycles > ts->reset_timeout_cycles) {
        Message::CallCompletionFun(&ts->reset_completion_fun, true, nullptr);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if HTTP_SERVER
void BeebThread::DebugAsyncCallCallback(bool called, void *context) {
    auto ts = (ThreadState *)context;

    Message::CallCompletionFun(&ts->async_call_completion_fun, called, nullptr);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
