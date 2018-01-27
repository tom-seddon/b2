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
#include "TVOutput.h"
#include "misc.h"
#include "beeb_events.h"
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
#include "Timeline.h"
#include "BeebWindow.h"
#include <Remotery.h>

#include <shared/enum_def.h>
#include "BeebThread.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Number of 2MHz cycles the emulated BBC will run for, flat out.
static const int32_t RUN_2MHz_CYCLES=2000;

// ~1MByte
static constexpr size_t NUM_VIDEO_UNITS=262144;
static constexpr size_t NUM_AUDIO_UNITS=NUM_VIDEO_UNITS/2;//(1<<SOUND_CLOCK_SHIFT);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define LOGGING 0
#define LOGGING_FREQUENCY_SECONDS (1.)
#endif

#if LOGGING
LOG_DEFINE(ATH,"audio","AUTHRD",&log_printer_stdout_and_debugger)
#endif

// This is a dummy log. It's never used for printing, only for
// initialising each BeebThread's log. Since it's a global, it gets an
// entry in the global table, so it can interact with the command line
// options.
LOG_TAGGED_DEFINE(BTHREAD,"beeb","",&log_printer_stdout_and_debugger,false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *const COPY_BASIC_LINES[]={
    "OLD",
    "LIST",
    nullptr,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const float VOLUMES_TABLE[]={
    -1.00000f,-0.79433f,-0.63096f,-0.50119f,-0.39811f,-0.31623f,-0.25119f,-0.19953f,-0.15849f,-0.12589f,-0.10000f,-0.07943f,-0.06310f,-0.05012f,-0.03981f,
    0.00000f,
    0.03981f,  0.05012f, 0.06310f, 0.07943f, 0.10000f, 0.12589f, 0.15849f, 0.19953f, 0.25119f, 0.31623f, 0.39811f, 0.50119f, 0.63096f, 0.79433f, 1.00000f,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::Message::Message(BeebThreadMessageType type_):
    type(type_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::KeyMessage::KeyMessage(BeebKey key_,bool state_):
    Message(BeebThreadMessageType_KeyState),
    key(key_),
    state(state_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::KeySymMessage::KeySymMessage(BeebKeySym key_sym_,bool state_):
    Message(BeebThreadMessageType_KeySymState),
    key_sym(key_sym_),
    state(state_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::HardResetMessage::HardResetMessage(bool boot_):
    Message(BeebThreadMessageType_HardReset),
    boot(boot_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::ChangeConfigMessage::ChangeConfigMessage(BeebLoadedConfig config_):
    Message(BeebThreadMessageType_ChangeConfig),
    config(std::move(config_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SetSpeedLimitingMessage::SetSpeedLimitingMessage(bool limit_speed_):
    Message(BeebThreadMessageType_SetSpeedLimiting),
    limit_speed(limit_speed_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::LoadDiscMessage::LoadDiscMessage(int drive_,std::shared_ptr<DiscImage> disc_image_,bool verbose_):
    Message(BeebThreadMessageType_LoadDisc),
    drive(drive_),
    disc_image(std::move(disc_image_)),
    verbose(verbose_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::LoadStateMessage::LoadStateMessage(uint64_t parent_timeline_id_,std::shared_ptr<BeebState> state_):
    Message(BeebThreadMessageType_LoadState),
    parent_timeline_id(parent_timeline_id_),
    state(std::move(state_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::GoToTimelineNodeMessage::GoToTimelineNodeMessage(uint64_t timeline_id_):
    Message(BeebThreadMessageType_GoToTimelineNode),
    timeline_id(timeline_id_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SaveStateMessage::SaveStateMessage(bool verbose_):
    Message(BeebThreadMessageType_SaveState),
    verbose(verbose_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::ReplayMessage::ReplayMessage(Timeline::ReplayData replay_data_):
    Message(BeebThreadMessageType_Replay),
    replay_data(std::move(replay_data_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SaveAndReplayFromMessage::SaveAndReplayFromMessage(uint64_t timeline_start_id_):
    Message(BeebThreadMessageType_SaveAndReplayFrom),
    timeline_start_id(timeline_start_id_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SaveAndVideoFromMessage::SaveAndVideoFromMessage(uint64_t timeline_start_id_,std::unique_ptr<VideoWriter> video_writer_):
    Message(BeebThreadMessageType_SaveAndVideoFrom),
    timeline_start_id(timeline_start_id_),
    video_writer(std::move(video_writer_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SetDiscImageNameAndLoadMethodMessage::SetDiscImageNameAndLoadMethodMessage(int drive_,std::string name_,std::string load_method_):
    Message(BeebThreadMessageType_SetDiscImageNameAndLoadMethod),
    drive(drive_),
    name(std::move(name_)),
    load_method(std::move(load_method_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
BeebThread::StartTraceMessage::StartTraceMessage(const TraceConditions &conditions_):
    Message(BeebThreadMessageType_StartTrace),
    conditions(conditions_)
{
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
BeebThread::StopTraceMessage::StopTraceMessage():
    Message(BeebThreadMessageType_StopTrace)
{
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::CloneWindowMessage::CloneWindowMessage(BeebWindowInitArguments init_arguments_):
    Message(BeebThreadMessageType_CloneWindow),
    init_arguments(std::move(init_arguments))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::CloneThisThreadMessage::CloneThisThreadMessage(std::shared_ptr<BeebThread> dest_thread_):
    Message(BeebThreadMessageType_CloneThisThread),
    dest_thread(std::move(dest_thread_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::LoadLastStateMessage::LoadLastStateMessage():
    Message(BeebThreadMessageType_LoadLastState)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::CancelReplayMessage::CancelReplayMessage():
    Message(BeebThreadMessageType_CancelReplay)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SetTurboDiscMessage::SetTurboDiscMessage(bool turbo_):
    Message(BeebThreadMessageType_SetTurboDisc),
    turbo(turbo_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::StartPasteMessage::StartPasteMessage(std::string text_):
    Message(BeebThreadMessageType_StartPaste),
    text(std::move(text_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::StopPasteMessage::StopPasteMessage():
    Message(BeebThreadMessageType_StopPaste)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::StartCopyMessage::StartCopyMessage(std::function<void(std::vector<uint8_t>)> stop_fun_,bool basic_):
    Message(BeebThreadMessageType_StartCopy),
    stop_fun(std::move(stop_fun_)),
    basic(basic_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::StopCopyMessage::StopCopyMessage():
    Message(BeebThreadMessageType_StopCopy)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugWakeUpMessage::DebugWakeUpMessage():
    Message(BeebThreadMessageType_DebugWakeUp)
{
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::PauseMessage::PauseMessage(bool pause_):
    Message(BeebThreadMessageType_SetPaused),
    pause(pause_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetByteMessage::DebugSetByteMessage(uint16_t addr_,uint8_t value_):
    Message(BeebThreadMessageType_SetByte),
    addr(addr_),
    value(value_)
{
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetBytesMessage::DebugSetBytesMessage(uint32_t addr_,std::vector<uint8_t> values_):
    Message(BeebThreadMessageType_SetBytes),
    addr(addr_),
    values(std::move(values_))
{
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::CustomMessage::CustomMessage():
    Message(BeebThreadMessageType_Custom)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebThread::ThreadState {
    // For the benefit of callbacks that have a ThreadState * as their context.
    BeebThread *beeb_thread=nullptr;
    const uint64_t *num_executed_2MHz_cycles=nullptr;
    BBCMicro *beeb=nullptr;
    BeebLoadedConfig current_config;
#if BBCMICRO_TRACE
    TraceConditions trace_conditions;
#endif
    bool boot=false;
    BeebShiftState fake_shift_state=BeebShiftState_Any;

    // Replay data. All valid if the thread's m_is_replaying flag is
    // set.
    Timeline::ReplayData replay_data;
    size_t replay_next_index=0;
    uint64_t replay_next_event_cycles=0;

    bool copy_basic=false;
    std::function<void(std::vector<uint8_t>)> copy_stop_fun;
    std::vector<uint8_t> copy_data;

    Log log{"BEEB  ",LOG(BTHREAD)};
    Messages msgs;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebThread::AudioThreadData {
    Remapper remapper;
    uint64_t num_consumed_sound_units=0;
    float bbc_sound_scale=1.f;
    float disc_sound_scale=1.f;
    std::vector<AudioCallbackRecord> records;
    size_t record0_index=0;
    uint64_t sound_buffer_size_samples=0;

#if LOGGING
    volatile uint64_t num_executed_cycles=0;
    uint64_t last_print_ticks=0;
#endif

    AudioThreadData(uint64_t sound_freq,uint64_t sound_buffer_size_samples,size_t max_num_records);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::AudioThreadData::AudioThreadData(uint64_t sound_freq,uint64_t sound_buffer_size_samples_,size_t max_num_records):
    remapper(sound_freq,SOUND_CLOCK_HZ),
    sound_buffer_size_samples(sound_buffer_size_samples_)
{
    this->records.resize(max_num_records);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::KeyStates::GetState(BeebKey key) const {
    ASSERT(key>=0&&(int)key<128);

    uint8_t index=key>>6&1;
    uint64_t mask=1ull<<(key&63);

    uint64_t state=m_flags[index].load(std::memory_order_acquire)&mask;
    return !!state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::KeyStates::SetState(BeebKey key,bool state) {
    ASSERT(key>=0&&(int)key<128);

    uint8_t index=key>>6&1;
    uint64_t mask=1ull<<(key&63);

    if(state) {
        m_flags[index].fetch_or(mask,std::memory_order_acq_rel);
    } else {
        m_flags[index].fetch_and(~mask,std::memory_order_acq_rel);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::BeebThread(std::shared_ptr<MessageList> message_list,uint32_t sound_device_id,int sound_freq,size_t sound_buffer_size_samples):
    m_video_output(NUM_VIDEO_UNITS),
    m_sound_output(NUM_AUDIO_UNITS),
    m_message_list(std::move(message_list))
{
    m_sound_device_id=sound_device_id;
    m_sound_freq=sound_freq;

    ASSERT(sound_freq>=0);
    m_audio_thread_data=new AudioThreadData((uint64_t)sound_freq,(uint64_t)sound_buffer_size_samples,100);

    this->SetBBCVolume(MAX_DB);
    this->SetDiscVolume(MAX_DB);

    MUTEX_SET_NAME(m_mutex,"BeebThread");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::~BeebThread() {
    this->Stop();

    MessageQueueFree(&m_mq);

    delete m_audio_thread_data;
    m_audio_thread_data=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::Start() {
    ASSERT(!m_mq);

    m_mq=MessageQueueAlloc();

    try {
        m_thread=std::thread(std::bind(&BeebThread::ThreadMain,this));
    } catch(const std::system_error &) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::Stop() {
    if(m_thread.joinable()) {
        this->Send(std::make_unique<Message>(BeebThreadMessageType_Stop));

        m_thread.join();
    }

    MessageQueueFree(&m_mq);
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

static void DestroyBeebThreadMessageMessage(Message *m) {
    delete (BeebThread::Message *)m->data.ptr;
}

void BeebThread::Send(std::unique_ptr<Message> message) {
    if(m_mq) {
        ::Message m={};

        m.data.ptr=message.release();
        m.destroy_fn=&DestroyBeebThreadMessageMessage;

        MessageQueuePush(m_mq,&m);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendTimingMessage(uint64_t max_sound_units) {
    if(m_mq) {
        ::Message::Data tmp;

        tmp.u64=max_sound_units;

        MessageQueueAddSyntheticMessage(m_mq,BeebThreadSyntheticEventType_SoundClockTimer,tmp);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TURBO_DISC
bool BeebThread::IsTurboDisc() const {
    return m_is_turbo_disc.load(std::memory_order_acquire);
}
#endif

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

#if BBCMICRO_TRACE
const volatile TraceStats *BeebThread::GetTraceStats() const {
    if(m_is_tracing.load(std::memory_order_acquire)) {
        return &m_trace_stats;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const BBCMicro *BeebThread::LockBeeb(std::unique_lock<Mutex> *lock) const {
    *lock=std::unique_lock<Mutex>(m_mutex);

    return m_thread_state->beeb;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicro *BeebThread::LockMutableBeeb(std::unique_lock<Mutex> *lock) {
    *lock=std::unique_lock<Mutex>(m_mutex);

    return m_thread_state->beeb;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsSpeedLimited() const {
    return m_limit_speed.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsPaused() const {
    std::lock_guard<Mutex> lock(m_mutex);

    return m_paused;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BeebThread::GetDiscImage(std::unique_lock<Mutex> *lock,int drive) const {
    ASSERT(drive>=0&&drive<NUM_DRIVES);

    *lock=std::unique_lock<Mutex>(m_mutex);

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
    ASSERT(beeb_key>=0);
    return m_effective_key_states.GetState(beeb_key);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> BeebThread::GetNVRAM() const {
    std::lock_guard<Mutex> lock(m_mutex);

    std::vector<uint8_t> nvram;
    nvram.resize(m_thread_state->beeb->GetNVRAMSize());
    if(!nvram.empty()) {
        memcpy(nvram.data(),m_thread_state->beeb->GetNVRAM(),nvram.size());
    }

    return nvram;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::HasNVRAM() const {
    return m_has_vram.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ClearLastTrace() {
    std::lock_guard<Mutex> lock(m_mutex);

    m_last_trace=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<Trace> BeebThread::GetLastTrace() {
    std::lock_guard<Mutex> lock(m_mutex);

    return m_last_trace;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsReplaying() const {
    return m_is_replaying.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebThread::AudioThreadFillAudioBuffer(float *samples,size_t num_samples,bool perfect,void (*fn)(int,float,void *),void *fn_context) {
    AudioThreadData *const atd=m_audio_thread_data;
    uint64_t now_ticks=GetCurrentTickCount();

    ASSERT(num_samples==atd->sound_buffer_size_samples);

    if(!atd) {
        return 0;
    }

    bool limit_speed=this->IsSpeedLimited();

#if LOGGING
    bool print=false;

    if(m_sound_device_id!=0) {
        if(GetSecondsFromTicks(now_ticks-atd->last_print_ticks)>=LOGGING_FREQUENCY_SECONDS) {
            print=true;
            atd->last_print_ticks=now_ticks;
        }
    }
#endif

#if LOGGING
    if(print) {
        LOGF(ATH,"Data available: %zu units in %zu chunks\n",atd->sound_chunks.total_num_units,atd->sound_chunks.num_chunks);
    }
#endif

    uint64_t units_needed_now=atd->remapper.GetNumUnits(num_samples);
    uint64_t units_needed_future=atd->remapper.GetNumUnits(num_samples*5/2);

    const SoundDataUnit *sa=nullptr,*sb=nullptr;
    size_t num_sa,num_sb;
    if(!m_sound_output.ConsumerLock(&sa,&num_sa,&sb,&num_sb)) {
        num_sa=0;
        num_sb=0;
    }

    uint64_t units_available=num_sa+num_sb;

    AudioCallbackRecord *record=nullptr;
    if(!atd->records.empty()) {
        ASSERT(atd->record0_index<atd->records.size());
        record=&atd->records[atd->record0_index];

        ++atd->record0_index;
        atd->record0_index%=atd->records.size();

        record->time=now_ticks;
        record->needed=units_needed_now;
        record->available=units_available;
    }

    if(limit_speed) {
        this->SendTimingMessage(atd->num_consumed_sound_units+units_needed_future);
    } else {
        this->SendTimingMessage(UINT64_MAX);
    }

    if(units_available==0) {
        // Can't really do much with this...
        return 0;
    }

    Remapper *remapper,temp_remapper;
    if(limit_speed) {
        if(units_needed_now<=units_available) {
            remapper=&atd->remapper;
        } else {
            if(perfect) {
                // bail out and try again later.
                m_sound_output.ConsumerUnlock(0);
                return 0;
            }

            goto use_units_available;
        }
    } else {
    use_units_available:;
        // Underflow, or no speed limiting... just eat it all, however
        // much there is.
        temp_remapper=Remapper(num_samples,units_available);
        remapper=&temp_remapper;
    }

    float *dest=samples;

    float sn_scale=1/4.f*atd->bbc_sound_scale;
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    float disc_sound_scale=1.f*atd->disc_sound_scale;
#endif

#define MIXCH(CH) (VOLUMES_TABLE[15+unit->sn_output.ch[CH]])
#define MIXSN (sn_scale*(MIXCH(0)+MIXCH(1)+MIXCH(2)+MIXCH(3)))
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
#define MIXALL (disc_sound_scale*unit->disc_drive_sound+MIXSN)
#else
#define MIXALL MIXSN
#endif

#define MIX (*filter++*MIXALL)

    // This functionality may return.
    (void)fn,(void)fn_context;
    ASSERT(!fn);
    ASSERT(!fn_context);

    float acc=0.f;
    size_t num_units_left=num_sa;
    const SoundDataUnit *unit=sa;
    size_t num_units_consumed=0;

    for(size_t sample_idx=0;sample_idx<num_samples;++sample_idx) {
        uint64_t num_units=remapper->Step();

        if(num_units>0) {
            acc=0.f;

            const float *filter;
            size_t filter_width;
            GetFilterForWidth(&filter,&filter_width,(size_t)num_units);
            ASSERT(filter_width<=num_units);

            if(num_units<=num_units_left) {
                // consume NUM_UNITS contiguous units from part A or
                // part B
                for(size_t i=0;i<filter_width;++i,++unit) {
                    acc+=MIX;
                }

                // (the filter may be shorter)
                unit+=num_units-filter_width;

                num_units_left-=num_units;
            } else {
                size_t i=0;

                // consume NUM_UNITS_LEFT contiguous units from the
                // end of part A, then NUM_UNITS-NUM_UNITS_LEFT
                // contiguous units from the start of part B.

                while(i<num_units_left) {
                    if(i<filter_width) {
                        acc+=MIX;
                    }

                    ++i;
                    ++unit;
                }

                ASSERT(unit==sa+num_sa);

                ASSERT(num_sb>=num_units-num_units_left);
                num_units_left=num_sb-(num_units-num_units_left);
                unit=sb;

                while(i<num_units) {
                    if(i<filter_width) {
                        acc+=MIX;
                    }

                    ++i;
                    ++unit;
                }
            }

            num_units_consumed+=num_units;
        }

        dest[sample_idx]=acc;
    }

    atd->num_consumed_sound_units+=num_units_consumed;
    m_sound_output.ConsumerUnlock(num_units_consumed);

    return num_samples;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

float BeebThread::GetBBCVolume() const {
    return m_bbc_sound_db;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetBBCVolume(float db) {
    this->SetVolume(&m_audio_thread_data->bbc_sound_scale,&m_bbc_sound_db,db);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

float BeebThread::GetDiscVolume() const {
    return m_disc_sound_db;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetDiscVolume(float db) {
    this->SetVolume(&m_audio_thread_data->disc_sound_scale,&m_disc_sound_db,db);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<BeebThread::AudioCallbackRecord> BeebThread::GetAudioCallbackRecords() const {
    std::vector<AudioCallbackRecord> records;
    records.reserve(m_audio_thread_data->records.size());

    {
        AudioDeviceLock lock(m_sound_device_id);

        for(size_t i=0;i<m_audio_thread_data->records.size();++i) {
            size_t index=(m_audio_thread_data->record0_index+i)%m_audio_thread_data->records.size();
            records.emplace_back(m_audio_thread_data->records[index]);
        }
    }

    return records;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t BeebThread::GetParentTimelineEventId() const {
    std::lock_guard<Mutex> lock(m_mutex);

    return m_parent_timeline_event_id;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t BeebThread::GetLastSavedStateTimelineId() const {
    std::lock_guard<Mutex> lock(m_mutex);

    return m_last_saved_state_timeline_id;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetVolume(float *scale_var,float *db_var,float db) {
    if(db>MAX_DB) {
        db=MAX_DB;
    }

    if(db<MIN_DB) {
        db=MIN_DB;
    }

    *db_var=db;

    {
        AudioDeviceLock lock(m_sound_device_id);

        *scale_var=powf(10.f,db/20.f);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadRecordEvent(ThreadState *ts,BeebEvent event) {
    this->ThreadHandleEvent(ts,event,false);

    m_parent_timeline_event_id=Timeline::AddEvent(m_parent_timeline_event_id,std::move(event));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<BeebState> BeebThread::ThreadSaveState(ThreadState *ts) {
    std::unique_ptr<BBCMicro> clone_beeb=ts->beeb->Clone();

    auto state=std::make_shared<BeebState>(std::move(clone_beeb));
    return state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
bool BeebThread::ThreadStopTraceOnOSWORD0(BBCMicro *beeb,M6502 *cpu,void *context) {
    (void)beeb;
    auto ts=(BeebThread::ThreadState *)context;

    if(cpu->pc.w==0xfff2&&cpu->a==0) {
        ts->beeb_thread->ThreadStopTrace(ts);
        return false;
    } else {
        return true;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadStopCopyOnOSWORD0(BBCMicro *beeb,M6502 *cpu,void *context) {
    (void)beeb;
    auto ts=(ThreadState *)context;

    if(!ts->beeb_thread->m_is_copying) {
        return false;
    }

    if(cpu->pc.w==0xfff2&&cpu->a==0) {
        if(!ts->beeb->IsPasting()) {
            ts->beeb_thread->ThreadStopCopy(ts);
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadAddCopyData(BBCMicro *beeb,M6502 *cpu,void *context) {
    (void)beeb;
    auto ts=(ThreadState *)context;

    if(!ts->beeb_thread->m_is_copying) {
        return false;
    }

    const uint8_t *ram=beeb->GetRAM();

    // Rather tiresomely, BASIC 2 prints stuff with JMP (WRCHV). Who
    // comes up with this stuff? So check against WRCHV, not just
    // 0xffee.

    if(cpu->abus.b.l==ram[0x020e]&&cpu->abus.b.h==ram[0x020f]) {
        // Opcode fetch for first byte of OSWRCH
        ts->copy_data.push_back(cpu->a);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebThread::ThreadStartTrace(ThreadState *ts) {
    switch(ts->trace_conditions.start) {
    default:
        ASSERT(false);
        // fall through
    case BeebThreadStartTraceCondition_Immediate:
        // Start now.
        this->ThreadBeebStartTrace(ts);
        break;

    case BeebThreadStartTraceCondition_NextKeypress:
        // Wait for the keypress...
        break;
    }

    //ts->beeb->SetInstructionTraceEventFn(nullptr,nullptr);

    switch(ts->trace_conditions.stop) {
    default:
        ASSERT(false);
        // fall through
    case BeebThreadStopTraceCondition_ByRequest:
        // By request...
        break;

    case BeebThreadStopTraceCondition_OSWORD0:
        ts->beeb->AddInstructionFn(&ThreadStopTraceOnOSWORD0,ts);
        break;
    }

    memset(&m_trace_stats,0,sizeof m_trace_stats);
    m_is_tracing.store(true,std::memory_order_release);
    m_last_trace=nullptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebThread::ThreadBeebStartTrace(ThreadState *ts) {
    ts->beeb->StartTrace(ts->trace_conditions.trace_flags);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebThread::ThreadStopTrace(ThreadState *ts) {
    ASSERT(ts->beeb);

    memset(&m_trace_stats,0,sizeof m_trace_stats);
    m_is_tracing.store(false,std::memory_order_release);

    std::shared_ptr<Trace> last_trace=ts->beeb->StopTrace();

    ts->trace_conditions=TraceConditions();

    m_last_trace=last_trace;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadReplaceBeeb(ThreadState *ts,std::unique_ptr<BBCMicro> beeb,uint32_t flags) {
    {
#if BBCMICRO_DEBUGGER
        std::unique_ptr<BBCMicro::DebugState> debug_state;
#endif

        if(ts->beeb) {
#if BBCMICRO_DEBUGGER
            debug_state=ts->beeb->TakeDebugState();
#endif
            delete ts->beeb;
            ts->beeb=nullptr;
        }

#if BBCMICRO_DEBUGGER
        if(!debug_state) {
            // Probably just the first time round.
            ts->log.f("Creating new BBCMicro::DebugState.\n");
            debug_state=std::make_unique<BBCMicro::DebugState>();
        }
#endif

        ts->beeb=beeb.release();

#if BBCMICRO_DEBUGGER
        ts->beeb->SetDebugState(std::move(debug_state));
#endif
    }

    ts->num_executed_2MHz_cycles=ts->beeb->GetNum2MHzCycles();

    {
        AudioDeviceLock lock(m_sound_device_id);

        m_audio_thread_data->num_consumed_sound_units=*ts->num_executed_2MHz_cycles>>SOUND_CLOCK_SHIFT;
    }

    m_has_vram.store(ts->beeb->GetNVRAMSize()>0,std::memory_order_release);

    // Apply current keypresses to the emulated BBC. Reset fake shift
    // state and boot state first so that the Shift key status is set
    // properly.
    this->ThreadSetBootState(ts,false);
    this->ThreadSetFakeShiftState(ts,BeebShiftState_Any);

    if(flags&BeebThreadReplaceFlag_ApplyPCState) {
        // Set BBC state from shadow state.
        for(int i=0;i<128;++i) {
            bool state=this->GetKeyState((BeebKey)i);
            this->ThreadSetKeyState(ts,(BeebKey)i,state);
        }

        ts->beeb->SetTurboDisc(m_is_turbo_disc);
    } else {
        // Set shadow state from BBC state.
        for(int i=0;i<128;++i) {
            bool state=!!ts->beeb->GetKeyState((BeebKey)i);
            this->ThreadSetKeyState(ts,(BeebKey)i,state);
        }

        m_is_turbo_disc.store(ts->beeb->GetTurboDisc(),std::memory_order_release);
    }

    if(flags&BeebThreadReplaceFlag_KeepCurrentDiscs) {
        for(int i=0;i<NUM_DRIVES;++i) {
            this->ThreadSetDiscImage(ts,i,DiscImage::Clone(m_disc_images[i]));
        }
    } else {
        for(int i=0;i<NUM_DRIVES;++i) {
            m_disc_images[i]=ts->beeb->GetDiscImage(i);
        }
    }

#if BBCMICRO_TRACE
    if(m_is_tracing) {
        this->ThreadStartTrace(ts);
    }
#endif

    ts->beeb->GetAndResetDiscAccessFlag();
    this->ThreadSetBootState(ts,!!(flags&BeebThreadReplaceFlag_Autoboot));

    m_paused=false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetFakeShiftState(ThreadState *ts,BeebShiftState state) {
    ts->fake_shift_state=state;

    this->ThreadUpdateShiftKeyState(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetBootState(ThreadState *ts,bool state) {
    ts->boot=state;

    this->ThreadUpdateShiftKeyState(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadUpdateShiftKeyState(ThreadState *ts) {
    this->ThreadSetKeyState(ts,BeebKey_Shift,m_real_key_states.GetState(BeebKey_Shift));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetKeyState(ThreadState *ts,BeebKey beeb_key,bool state) {
    ASSERT(!(beeb_key&0x80));

    if(IsNumericKeypadKey(beeb_key)&&!ts->beeb->HasNumericKeypad()) {
        // Ignore numeric keypad key for Model B.
    } else {
        // Always set the key flags as requested.
        m_real_key_states.SetState(beeb_key,state);

        // If it's the shift key, override using fake shift flags or
        // boot flag.
        if(beeb_key==BeebKey_Shift) {
            if(ts->boot) {
                state=true;
            } else if(ts->fake_shift_state==BeebShiftState_On) {
                state=true;
            } else if(ts->fake_shift_state==BeebShiftState_Off) {
                state=false;
            }
        } else {
            if(ts->boot) {
                // this is recursive, but it only calls
                // ThreadSetKeyState for BeebKey_Shift, so it won't
                // end up here again...
                this->ThreadSetBootState(ts,false);
            } else {
                // no harm in skipping it.
            }
        }

        m_effective_key_states.SetState(beeb_key,state);

        //printf("%-18" PRIu64 ": %s: %s=%s (boot=%s fake_shift_state=%s real shift=%s)\n",
        //    *ts->num_executed_2MHz_cycles,
        //    __func__,
        //    GetBeebKeyEnumName(beeb_key),
        //    BOOL_STR(state),
        //    BOOL_STR(ts->boot),
        //    GetBeebShiftStateEnumName(ts->fake_shift_state),
        //    BOOL_STR(m_real_key_states.GetState(BeebKey_Shift)));

        this->ThreadRecordEvent(ts,BeebEvent::MakeKeyState(*ts->num_executed_2MHz_cycles,beeb_key,state));

#if BBCMICRO_TRACE
        if(ts->trace_conditions.start==BeebThreadStartTraceCondition_NextKeypress) {
            if(m_is_tracing) {
                if(state) {
                    if(ts->trace_conditions.beeb_key<0||beeb_key==ts->trace_conditions.beeb_key) {
                        ts->trace_conditions.start=BeebThreadStartTraceCondition_Immediate;
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

#if BBCMICRO_TURBO_DISC
void BeebThread::ThreadSetTurboDisc(ThreadState *ts,bool turbo) {
    ts->beeb->SetTurboDisc(turbo);

    m_is_turbo_disc.store(turbo,std::memory_order_release);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// REPLAYING is set if this event is coming from a replay. Otherwise,
// it's the first time.

void BeebThread::ThreadHandleEvent(ThreadState *ts,
                                   const BeebEvent &event,
                                   bool replay)
{
    auto event_type=(BeebEventType)event.type;
    switch(event_type) {
    case BeebEventType_None:
        return;

    case BeebEventType_KeyState:
        ts->beeb->SetKeyState(event.data.key_state.key,!!event.data.key_state.state);
        return;

    case BeebEventType_LoadDiscImage:
        {
            const BeebEventLoadDiscImageData *data=event.data.load_disc_image;

            ASSERT(data->disc_image->CanClone());
            this->ThreadSetDiscImage(ts,data->drive,data->disc_image->Clone());
        }
        return;

    case BeebEventType_Root:
    case BeebEventType_ChangeConfig:
        {
            ts->current_config=event.data.config->config;

            uint32_t flags=BeebThreadReplaceFlag_KeepCurrentDiscs;
            if(!replay) {
                flags|=BeebThreadReplaceFlag_ApplyPCState;
            }


            this->ThreadReplaceBeeb(ts,event.data.config->config.CreateBBCMicro(),flags);
        }
        return;

    case BeebEventType_HardReset:
        {
            uint32_t flags=event.data.hard_reset.flags;
            if(!replay) {
                flags|=BeebThreadReplaceFlag_ApplyPCState;
            }

            this->ThreadReplaceBeeb(ts,
                                    ts->current_config.CreateBBCMicro(),
                                    flags);
        }
        return;

#if BBCMICRO_TURBO_DISC
    case BeebEventType_SetTurboDisc:
        this->ThreadSetTurboDisc(ts,event.data.set_turbo_disc.turbo);
        return;
#endif

    case BeebEventType_LoadState:
        {
            this->ThreadReplaceBeeb(ts,event.data.state->state->CloneBBCMicro(),0);
        }
        return;

    case BeebEventType_SaveState:
        {
            // These events are placeholders and don't get replayed.
        }
        return;

    case BeebEventType_WindowProxy:
        {
            // These events should never occur.
        }
        break;

    case BeebEventType_StartPaste:
        {
            ts->beeb->StartPaste(event.data.paste->text);
            m_is_pasting.store(true,std::memory_order_release);
        }
        return;

    case BeebEventType_StopPaste:
        {
            ts->beeb->StopPaste();
            m_is_pasting.store(false,std::memory_order_release);
        }
        return;

#if BBCMICRO_DEBUGGER
    case BeebEventType_SetByte:
        {
            M6502Word address={event.data.set_byte.address};

            ts->beeb->SetMemory(address,event.data.set_byte.value);
        }
        return;

    case BeebEventType_SetBytes:
        {
            uint32_t addr=event.data.set_bytes->address;

            for(uint8_t value:event.data.set_bytes->values) {
                ts->beeb->DebugSetByte(addr++,value);
            }
        }
        return;
#endif
    }

    ASSERT(false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStartReplay(ThreadState *ts,Timeline::ReplayData replay_data) {
    ts->replay_data=std::move(replay_data);
    ASSERT(!ts->replay_data.events.empty());
    ASSERT(ts->replay_data.events[0].be.GetTypeFlags()&BeebEventTypeFlag_Start);

    ts->replay_next_index=0;
    ts->replay_next_event_cycles=ts->replay_data.events[0].be.time_2MHz_cycles;

    ASSERT(!m_is_replaying);
    m_is_replaying.store(true,std::memory_order_release);

    //LOGF(OUTPUT,"replay start.\n");

    // Replay first event straight away to get things started.
    this->ThreadHandleReplayEvents(ts);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStopReplay(ThreadState *ts) {
    (void)ts;

    m_is_replaying.store(false,std::memory_order_release);

    //LOGF(OUTPUT,"replay end.\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadLoadState(ThreadState *ts,uint64_t parent_timeline_id,const std::shared_ptr<BeebState> &state) {
    this->ThreadReplaceBeeb(ts,state->CloneBBCMicro(),0);

    m_parent_timeline_event_id=parent_timeline_id;

    Timeline::DidChange();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadHandleReplayEvents(ThreadState *ts) {
    ASSERT(m_is_replaying);

    ASSERT(ts->replay_next_index<ts->replay_data.events.size());

    for(;;) {
        if(ts->replay_next_index>=ts->replay_data.events.size()) {
            // Replay done.
            ASSERT(ts->replay_next_index==ts->replay_data.events.size());
            this->ThreadStopReplay(ts);
            break;
        }

        const Timeline::ReplayData::Event *re=&ts->replay_data.events[ts->replay_next_index];
        if(re->be.time_2MHz_cycles!=ts->replay_next_event_cycles) {
            // No more events now, but replay is ongoing.
            ts->replay_next_event_cycles=re->be.time_2MHz_cycles;
            break;
        }

        ++ts->replay_next_index;

        this->ThreadHandleEvent(ts,re->be,true);

        if(re->id==0) {
            // Event wasn't on timeline, so don't update parent.
        } else {
            m_parent_timeline_event_id=re->id;

            if(re->be.GetTypeFlags()&BeebEventTypeFlag_ChangesTimeline) {
                Timeline::DidChange();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadSetDiscImage(ThreadState *ts,int drive,std::shared_ptr<DiscImage> disc_image) {
    ts->beeb->SetDiscImage(drive,disc_image);

    m_disc_images[drive]=disc_image;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStartPaste(ThreadState *ts,std::string text) {
    auto shared_text=std::make_shared<std::string>(std::move(text));

    this->ThreadRecordEvent(ts,BeebEvent::MakeStartPaste(*ts->num_executed_2MHz_cycles,shared_text));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStopCopy(ThreadState *ts) {
    ASSERT(m_is_copying);

    if(ts->copy_basic) {
        if(!ts->copy_data.empty()) {
            if(ts->copy_data.back()=='>') {
                ts->copy_data.pop_back();
            }

            std::string prefix;

            for(size_t i=0;COPY_BASIC_LINES[i];++i) {
                if(i==0) {
                    prefix.push_back(BBCMicro::PASTE_START_CHAR);
                    prefix.push_back(127);
                } else {
                    prefix.push_back('>');
                }

                prefix+=COPY_BASIC_LINES[i];
                prefix+="\n\r";
            }

            if(ts->copy_data.size()>=prefix.size()) {
                auto begin=ts->copy_data.begin();

                ASSERT(prefix.size()<=PTRDIFF_MAX);
                auto end=begin+(ptrdiff_t)prefix.size();

                if(std::equal(begin,end,prefix.begin())) {
                    ts->copy_data.erase(begin,end);
                }
            }
        }
    }

    PushFunctionMessage([data=std::move(ts->copy_data),fun=std::move(ts->copy_stop_fun)]() {
        fun(std::move(data));
    });

    m_is_copying.store(false,std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadHandleMessage(
    ThreadState *ts,
    ::Message *msg,
    bool *limit_speed,
    uint64_t *next_stop_2MHz_cycles)
{
    bool result=true;

    if(ts->beeb) {
        m_leds.store(ts->beeb->GetLEDs(),std::memory_order_release);

#if BBCMICRO_TRACE
        ts->beeb->GetTraceStats(&m_trace_stats);
#endif
    }

    if(ts->boot) {
        if(ts->beeb->GetAndResetDiscAccessFlag()) {
            this->ThreadSetBootState(ts,false);
        }
    }

    switch(msg->type) {
    default:
        ASSERT(false);
        break;

    case 0:
        {
            std::unique_ptr<Message> message((Message *)msg->data.ptr);
            msg->data.ptr=nullptr;

            switch(message->type) {
            default:
                ASSERT(0);
                // fall through
            case BeebThreadMessageType_Stop:
                result=false;
                break;

            case BeebThreadMessageType_KeyState:
                {
                    if(m_is_replaying) {
                        // Ignore.
#if BBCMICRO_DEBUGGER
                    } else if(ts->beeb->DebugIsHalted()) {
                        // Ignore.
#endif
                    } else {
                        auto m=(KeyMessage *)message.get();

                        ASSERT(ts->beeb);

                        this->ThreadSetKeyState(ts,m->key,m->state);
                    }
                }
                break;

            case BeebThreadMessageType_KeySymState:
                {
                    if(m_is_replaying) {
                        // Ignore.
#if BBCMICRO_DEBUGGER
                    } else if(ts->beeb->DebugIsHalted()) {
                        // Ignore.
#endif
                    } else {
                        auto m=(KeySymMessage *)message.get();

                        ASSERT(ts->beeb);

                        BeebKey beeb_key;
                        BeebShiftState shift_state;
                        if(GetBeebKeyComboForKeySym(&beeb_key,&shift_state,m->key_sym)) {
                            this->ThreadSetFakeShiftState(ts,m->state?shift_state:BeebShiftState_Any);
                            this->ThreadSetKeyState(ts,beeb_key,m->state);
                        }
                    }
                }
                break;

            case BeebThreadMessageType_HardReset:
                {
                    auto m=(HardResetMessage *)message.get();

                    uint32_t flags=BeebThreadReplaceFlag_KeepCurrentDiscs;

                    if(m->boot) {
                        flags|=BeebThreadReplaceFlag_Autoboot;
                    }

                    ts->log.f("%s: flags=%s\n",GetBeebThreadMessageTypeEnumName((int)message->type),GetFlagsString(flags,&GetBeebThreadReplaceFlagEnumName).c_str());

                    // this->ThreadReplaceBeeb(ts,ts->current_config.CreateBBCMicro(),flags|BeebThreadReplaceFlag_ApplyPCState);

                    this->ThreadRecordEvent(ts,BeebEvent::MakeHardReset(*ts->num_executed_2MHz_cycles,flags));
                }
                break;

            case BeebThreadMessageType_ChangeConfig:
                {
                    auto m=(ChangeConfigMessage *)message.get();

                    ts->current_config=std::move(m->config);

                    this->ThreadRecordEvent(ts,BeebEvent::MakeChangeConfig(*ts->num_executed_2MHz_cycles,ts->current_config));
                }
                break;

            case BeebThreadMessageType_SetSpeedLimiting:
                {
                    auto m=(SetSpeedLimitingMessage *)message.get();

                    *limit_speed=m->limit_speed;
                    m_limit_speed.store(*limit_speed,std::memory_order_release);
                }
                break;

            case BeebThreadMessageType_LoadDisc:
                {
                    //ts->log.f("%s:\n",GetBeebThreadMessageTypeEnumName((int)msg->type));

                    if(m_is_replaying) {
                        // Ignore.
                    } else {
                        auto m=(LoadDiscMessage *)message.get();

                        if(m->verbose) {
                            if(!m->disc_image) {
                                ts->msgs.i.f("Drive %d: empty\n",m->drive);
                            } else {
                                ts->msgs.i.f("Drive %d: %s\n",m->drive,m->disc_image->GetName().c_str());

                                std::string hash=m->disc_image->GetHash();
                                if(!hash.empty()) {
                                    ts->msgs.i.f("(Hash: %s)\n",hash.c_str());
                                }
                            }
                        }

                        this->ThreadRecordEvent(ts,BeebEvent::MakeLoadDiscImage(*ts->num_executed_2MHz_cycles,m->drive,std::move(m->disc_image)));
                    }
                }
                break;

            case BeebThreadMessageType_SetPaused:
                {
                    auto m=(PauseMessage *)message.get();

                    m_paused=m->pause;
                }
                break;

            case BeebThreadMessageType_GoToTimelineNode:
                {
                    if(m_is_replaying) {
                        // ignore
                    } else {
                        auto m=(GoToTimelineNodeMessage *)message.get();

                        auto &&replay_data=Timeline::CreateReplay(m->timeline_id,m->timeline_id,&ts->msgs);

                        ThreadStartReplay(ts,replay_data);
                    }
                }
                break;

            case BeebThreadMessageType_LoadState:
                {
                    if(m_is_replaying) {
                        // ignore
                    } else {
                        auto m=(LoadStateMessage *)message.get();

                        this->ThreadLoadState(ts,m->parent_timeline_id,m->state);
                    }
                }
                break;

            case BeebThreadMessageType_CloneThisThread:
                {
                    auto m=(CloneThisThreadMessage *)message.get();

                    std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);

                    m->dest_thread->Send(std::make_unique<LoadStateMessage>(m_parent_timeline_event_id,std::move(state)));
                }
                break;

            case BeebThreadMessageType_CloneWindow:
                {
                    auto m=(CloneWindowMessage *)message.get();

                    BeebWindowInitArguments init_arguments=std::move(m->init_arguments);

                    init_arguments.initially_paused=false;
                    init_arguments.initial_state=this->ThreadSaveState(ts);

                    ASSERT(init_arguments.parent_timeline_event_id==0);
                    init_arguments.parent_timeline_event_id=m_parent_timeline_event_id;

                    PushNewWindowMessage(init_arguments);
                }
                break;

            case BeebThreadMessageType_SaveState:
                {
                    auto m=(SaveStateMessage *)message.get();

                    std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);

                    if(m->verbose) {
                        std::string time_str=Get2MHzCyclesString(state->GetEmulated2MHzCycles());
                        ts->msgs.i.f("Saved state: %s\n",time_str.c_str());
                    }

                    this->ThreadRecordEvent(ts,BeebEvent::MakeSaveState(*ts->num_executed_2MHz_cycles,state));

                    m_last_saved_state_timeline_id=m_parent_timeline_event_id;
                }
                break;

#if BBCMICRO_TRACE
            case BeebThreadMessageType_StartTrace:
                {
                    ASSERT(ts->beeb);

                    {
                        auto m=(StartTraceMessage *)message.get();

                        ts->trace_conditions=m->conditions;
                    }

                    this->ThreadStartTrace(ts);
                }
                break;
#endif

#if BBCMICRO_TRACE
            case BeebThreadMessageType_StopTrace:
                {
                    this->ThreadStopTrace(ts);
                }
                break;
#endif

            case BeebThreadMessageType_SetTurboDisc:
                {
                    auto m=(SetTurboDiscMessage *)message.get();
                    this->ThreadRecordEvent(ts,
                                            BeebEvent::MakeSetTurboDisc(*ts->num_executed_2MHz_cycles,
                                                                        m->turbo));
                }
                break;

            case BeebThreadMessageType_Replay:
                {
                    auto m=(ReplayMessage *)message.get();

                    this->ThreadStartReplay(ts,std::move(m->replay_data));
                }
                break;

            case BeebThreadMessageType_SaveAndReplayFrom:
                {
                    auto m=(SaveAndReplayFromMessage *)message.get();
                    //uint64_t start_timeline_id=msg->data.u64;

                    // Add a placeholder event that will serve as the finish event for the replay.
                    m_parent_timeline_event_id=Timeline::AddEvent(m_parent_timeline_event_id,BeebEvent::MakeNone(*ts->num_executed_2MHz_cycles));

                    std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);

                    m_pre_replay_parent_timeline_event_id=m_parent_timeline_event_id;
                    m_pre_replay_state=state;

                    auto &&replay_data=Timeline::CreateReplay(m->timeline_start_id,m_parent_timeline_event_id,&ts->msgs);

                    if(!replay_data.events.empty()) {
                        this->ThreadStartReplay(ts,replay_data);
                    }
                }
                break;

            case BeebThreadMessageType_SaveAndVideoFrom:
                {
                    auto m=(SaveAndVideoFromMessage *)message.get();
                    //auto ptr=(SaveAndVideoFromMessagePayload  *)msg->data.ptr;

                    this->ThreadRecordEvent(ts,BeebEvent::MakeNone(*ts->num_executed_2MHz_cycles));

                    auto &&replay_data=Timeline::CreateReplay(m->timeline_start_id,m_parent_timeline_event_id,&ts->msgs);

                    if(!replay_data.events.empty()) {
                        auto job=std::make_shared<WriteVideoJob>(std::move(replay_data),std::move(m->video_writer),m_message_list);
                        BeebWindows::AddJob(job);
                    }
                }
                break;

            case BeebThreadMessageType_SetDiscImageNameAndLoadMethod:
                {
                    auto m=(SetDiscImageNameAndLoadMethodMessage *)message.get();

                    if(std::shared_ptr<DiscImage> disc_image=ts->beeb->GetMutableDiscImage(m->drive)) {
                        disc_image->SetName(std::move(m->name));
                        disc_image->SetLoadMethod(std::move(m->load_method));
                    }
                }
                break;

            case BeebThreadMessageType_LoadLastState:
                {
                    if(uint64_t id=this->GetLastSavedStateTimelineId()) {
                        //auto m=(BeebThreadLoadLastStateMessage *)message.get();
                        this->Send(std::make_unique<GoToTimelineNodeMessage>(id));
                        //this->SendGoToTimelineNodeMessage(id);
                    }
                }
                break;

            case BeebThreadMessageType_CancelReplay:
                {
                    if(m_is_replaying.load(std::memory_order_acquire)) {
                        this->ThreadStopReplay(ts);

                        std::shared_ptr<BeebState> state;
                        uint64_t timeline_id;

                        state=std::move(m_pre_replay_state);
                        ASSERT(!m_pre_replay_state);

                        timeline_id=m_pre_replay_parent_timeline_event_id;
                        m_pre_replay_parent_timeline_event_id=0;

                        if(!!state) {
                            this->ThreadLoadState(ts,timeline_id,state);
                        }

                        Timeline::DidChange();
                    }
                }
                break;

            case BeebThreadMessageType_StartPaste:
                {
                    auto m=(StartPasteMessage *)message.get();
                    //auto ptr=(StartPasteMessagePayload *)msg->data.ptr;

                    if(m_is_replaying) {
                        // Ignore.
                    } else {
                        this->ThreadStartPaste(ts,std::move(m->text));
                    }
                }
                break;

            case BeebThreadMessageType_StopPaste:
                {
                    if(m_is_replaying) {
                        // Ignore.
                    } else {
                        this->ThreadRecordEvent(ts,BeebEvent::MakeStopPaste(*ts->num_executed_2MHz_cycles));
                    }
                }
                break;

            case BeebThreadMessageType_StartCopy:
                {
                    auto m=(StartCopyMessage *)message.get();

                    // StartCopy and StartCopyBASIC really aren't the same
                    // sort of thing, but they share enough code that it felt
                    // a bit daft whichever way round they went.

                    if(m->basic) {
                        if(m_is_replaying) {
                            // Ignore.
                            break;
                        }

                        std::string text;
                        for(size_t i=0;COPY_BASIC_LINES[i];++i) {
                            text+=COPY_BASIC_LINES[i];
                            text.push_back('\r');
                        }

                        this->ThreadStartPaste(ts,std::move(text));

                        ts->beeb->AddInstructionFn(&ThreadStopCopyOnOSWORD0,ts);
                    }

                    ts->copy_data.clear();
                    if(!ts->beeb_thread->m_is_copying) {
                        ts->beeb->AddInstructionFn(&ThreadAddCopyData,ts);
                    }

                    ts->copy_basic=m->basic;
                    ts->copy_stop_fun=std::move(m->stop_fun);
                    m_is_copying.store(true,std::memory_order_release);
                }
                break;

            case BeebThreadMessageType_StopCopy:
                {
                    this->ThreadStopCopy(ts);
                }
                break;

#if BBCMICRO_DEBUGGER
            case BeebThreadMessageType_SetByte:
                {
                    auto m=(DebugSetByteMessage *)message.get();
                    //M6502Word address={(uint16_t)msg->u32};
                    //uint8_t value=(uint8_t)msg->data.u64;

                    this->ThreadRecordEvent(ts,BeebEvent::MakeSetByte(*ts->num_executed_2MHz_cycles,m->addr,m->value));
                }
                break;
#endif

#if BBCMICRO_DEBUGGER
            case BeebThreadMessageType_DebugWakeUp:
                {
                    // Nothing to do.
                }
                break;
#endif

#if BBCMICRO_DEBUGGER
            case BeebThreadMessageType_SetBytes:
                {
                    auto m=(DebugSetBytesMessage *)message.get();

                    this->ThreadRecordEvent(ts,BeebEvent::MakeSetBytes(*ts->num_executed_2MHz_cycles,m->addr,std::move(m->values)));
                }
                break;
#endif

            case BeebThreadMessageType_Custom:
                {
                    auto m=(CustomMessage *)message.get();

                    m->ThreadHandleMessage(ts->beeb);
                }
                break;
            }
        }
        break;

    case MESSAGE_TYPE_SYNTHETIC:
        switch(msg->u32) {
        default:
            ASSERT(0);
            break;

        case BeebThreadSyntheticEventType_SoundClockTimer:
            *next_stop_2MHz_cycles=msg->data.u64<<SOUND_CLOCK_SHIFT;
#if LOGGING
            m_audio_thread_data->num_executed_cycles.store(*ts->num_executed_2MHz_cycles,std::memory_order_release);
#endif
            break;
        }
    }

    MessageDestroy(msg);

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadMain(void) {
    uint64_t next_stop_2MHz_cycles=0;
    bool limit_speed=m_limit_speed.load(std::memory_order_acquire);
    ThreadState ts;
    bool paused;

    {
        std::lock_guard<Mutex> lock(m_mutex);

        SetCurrentThreadNamef("BeebThread");

        ts.beeb_thread=this;
        ts.msgs=Messages(m_message_list);

        m_thread_state=&ts;

        paused=m_paused;
    }

    for(;;) {
        ::Message msg;
        int got_msg;

        if(paused||(limit_speed&&next_stop_2MHz_cycles<=*ts.num_executed_2MHz_cycles)) {
        wait_for_message:
            rmt_ScopedCPUSample(MessageQueueWaitForMessage,0);
            MessageQueueWaitForMessage(m_mq,&msg);
            got_msg=1;
        } else {
            got_msg=MessageQueuePollForMessage(m_mq,&msg);
        }

        if(got_msg) {
            std::lock_guard<Mutex> lock(m_mutex);

            //std::unique_ptr<BeebThreadMessage> message((BeebThreadMessage *)msg.data.ptr);
            //msg.data.ptr=nullptr;

            if(!this->ThreadHandleMessage(&ts,&msg,&limit_speed,&next_stop_2MHz_cycles)) {
                goto done;
            }
        }

        uint64_t stop_2MHz_cycles=next_stop_2MHz_cycles;

        if(m_is_replaying) {
            ASSERT(*ts.num_executed_2MHz_cycles<=ts.replay_next_event_cycles);
            if(*ts.num_executed_2MHz_cycles==ts.replay_next_event_cycles) {
                this->ThreadHandleReplayEvents(&ts);
            }

            if(m_is_replaying) {
                if(ts.replay_next_event_cycles<stop_2MHz_cycles) {
                    stop_2MHz_cycles=ts.replay_next_event_cycles;
                }
            }
        }

        if(m_is_pasting) {
            if(!ts.beeb->IsPasting()) {
                m_is_pasting.store(false,std::memory_order_release);
            }
        }

        paused=m_paused;
#if BBCMICRO_DEBUGGER
        if(ts.beeb->DebugIsHalted()) {
            paused=true;
        }
#endif

        if(!paused&&stop_2MHz_cycles>*ts.num_executed_2MHz_cycles) {
            rmt_ScopedCPUSample(BeebUpdate,0);

            ASSERT(ts.beeb);

            uint64_t num_2MHz_cycles=stop_2MHz_cycles-*ts.num_executed_2MHz_cycles;
            if(num_2MHz_cycles>RUN_2MHz_CYCLES) {
                num_2MHz_cycles=RUN_2MHz_CYCLES;
            }

            // One day I'll clear up the units mismatch...
            VideoDataUnit *va,*vb;
            size_t num_va,num_vb;
            size_t num_video_units=num_2MHz_cycles;
            if(!m_video_output.ProducerLock(&va,&num_va,&vb,&num_vb,num_video_units)) {
                goto wait_for_message;
            }

            // should really have this as part of the OutputDataBuffer API.
            if(num_va+num_vb<num_video_units) {
            unlock_video:;
                m_video_output.ProducerUnlock(0);
                goto wait_for_message;
            }

            size_t num_sound_units=(num_2MHz_cycles+(1<<SOUND_CLOCK_SHIFT)-1)>>SOUND_CLOCK_SHIFT;

            SoundDataUnit *sa,*sb;
            size_t num_sa,num_sb;
            if(!m_sound_output.ProducerLock(&sa,&num_sa,&sb,&num_sb,num_sound_units)) {
                goto unlock_video;
            }

            if(num_sa+num_sb<num_sound_units) {
                m_sound_output.ProducerUnlock(0);
                goto unlock_video;
            }

            SoundDataUnit *sunit=sa;
            size_t num_sunits_produced=0;
            size_t num_vunits_produced=0;

            {
                VideoDataUnit *vunit;
                size_t i;
                std::unique_lock<Mutex> lock(m_mutex,std::defer_lock);

                // A.
                {
                    vunit=va;

                    for(i=0;i<num_va;++i) {
                        if(!lock.owns_lock()) {
                            lock.lock();
                        }

#if BBCMICRO_DEBUGGER
                        if(ts.beeb->DebugIsHalted()) {
                            break;
                        }
#endif

                        if(ts.beeb->Update(vunit++,sunit)) {
                            lock.unlock();

                            ++sunit;

                            if(sunit==sa+num_sa) {
                                sunit=sb;
                            } else if(sunit==sb+num_sb) {
                                sunit=nullptr;
                            }

                            ++num_sunits_produced;
                        }
                    }

                    num_vunits_produced+=i;
                }

                if(!lock.owns_lock()) {
                    lock.lock();
                }

                // B.
#if BBCMICRO_DEBUGGER//////////////////////////<--note
                if(!ts.beeb->DebugIsHalted())//<--note
#endif/////////////////////////////////////////<--note
                {//////////////////////////////<--note
                    vunit=vb;

                    for(i=0;i<num_vb;++i) {
                        if(!lock.owns_lock()) {
                            lock.lock();
                        }

#if BBCMICRO_DEBUGGER
                        if(ts.beeb->DebugIsHalted()) {
                            break;
                        }
#endif

                        if(ts.beeb->Update(vunit++,sunit)) {
                            lock.unlock();

                            ++sunit;

                            if(sunit==sa+num_sa) {
                                sunit=sb;
                            } else if(sunit==sb+num_sb) {
                                sunit=nullptr;
                            }

                            ++num_sunits_produced;
                        }
                    }

                    num_vunits_produced+=i;
                }
            }

            m_video_output.ProducerUnlock(num_vunits_produced);
            m_sound_output.ProducerUnlock(num_sunits_produced);

            // It's a bit dumb having multiple copies.
            m_num_2MHz_cycles.store(*ts.num_executed_2MHz_cycles,std::memory_order_release);
        }
    }
done:
    {
        std::lock_guard<Mutex> lock(m_mutex);

        m_thread_state=nullptr;

        delete ts.beeb;
        ts.beeb=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
