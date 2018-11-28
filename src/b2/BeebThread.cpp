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

BeebThread::Message::~Message() {
    this->CallCompletionFun(true);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::Message::CallCompletionFun(bool success) {
    //if(this->type!=BeebThreadMessageType_Timing) {
    //    LOGF(OUTPUT,"%s: type=%s, success=%s, got completion_fun=%s\n",__func__,GetBeebThreadMessageTypeEnumName(this->type),BOOL_STR(success),BOOL_STR(!!this->completion_fun));
    //}

    if(!!this->completion_fun) {
        this->completion_fun(success);
        this->completion_fun=std::function<void(bool)>();
    }
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

BeebThread::HardResetMessage::HardResetMessage():
    Message(BeebThreadMessageType_HardReset)
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

BeebThread::LoadStateMessage::LoadStateMessage(std::shared_ptr<BeebState> state_):
    Message(BeebThreadMessageType_LoadState),
    state(std::move(state_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BeebThread::GoToTimelineNodeMessage::GoToTimelineNodeMessage(uint64_t timeline_id_):
//    Message(BeebThreadMessageType_GoToTimelineNode),
//    timeline_id(timeline_id_)
//{
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::SaveStateMessage::SaveStateMessage(bool verbose_):
    Message(BeebThreadMessageType_SaveState),
    verbose(verbose_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::ReplayMessage::ReplayMessage(std::unique_ptr<Timeline> timeline_):
    Message(BeebThreadMessageType_Replay),
    timeline(std::move(timeline_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BeebThread::SaveAndReplayFromMessage::SaveAndReplayFromMessage(uint64_t timeline_start_id_):
//    Message(BeebThreadMessageType_SaveAndReplayFrom),
//    timeline_start_id(timeline_start_id_)
//{
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BeebThread::SaveAndVideoFromMessage::SaveAndVideoFromMessage(uint64_t timeline_start_id_,std::unique_ptr<VideoWriter> video_writer_):
//    Message(BeebThreadMessageType_SaveAndVideoFrom),
//    timeline_start_id(timeline_start_id_),
//    video_writer(std::move(video_writer_))
//{
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
BeebThread::StartTraceMessage::StartTraceMessage(const TraceConditions &conditions_,
                                                 size_t max_num_bytes_):
Message(BeebThreadMessageType_StartTrace),
conditions(conditions_),
max_num_bytes(max_num_bytes_)
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
    init_arguments(std::move(init_arguments_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BeebThread::CloneThisThreadMessage::CloneThisThreadMessage(std::shared_ptr<BeebThread> dest_thread_):
//    Message(BeebThreadMessageType_CloneThisThread),
//    dest_thread(std::move(dest_thread_))
//{
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BeebThread::LoadLastStateMessage::LoadLastStateMessage():
//    Message(BeebThreadMessageType_LoadLastState)
//{
//}

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

#if BBCMICRO_DEBUGGER
BeebThread::DebugSetExtByteMessage::DebugSetExtByteMessage(uint32_t addr_,uint8_t value_):
        Message(BeebThreadMessageType_SetExtByte),
        addr(addr_),
        value(value_)
{
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BeebThread::DebugAsyncCallMessage::DebugAsyncCallMessage(uint16_t addr_,uint8_t a_,uint8_t x_,uint8_t y_,bool c_):
    Message(BeebThreadMessageType_DebugAsyncCall),
    addr(addr_),
    a(a_),
    x(x_),
    y(y_),
    c(c_)
{
    this->implicit_success=false;
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

BeebThread::TimingMessage::TimingMessage(uint64_t max_sound_units_):
    Message(BeebThreadMessageType_Timing),
    max_sound_units(max_sound_units_)
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
    size_t trace_max_num_bytes=0;
#endif
    bool boot=false;
    BeebShiftState fake_shift_state=BeebShiftState_Any;

    std::unique_ptr<Timeline> record_timeline;

    std::unique_ptr<Timeline> replay_timeline;
    size_t replay_next_index=0;
    uint64_t replay_next_event_cycles=0;

    bool copy_basic=false;
    std::function<void(std::vector<uint8_t>)> copy_stop_fun;
    std::vector<uint8_t> copy_data;

    std::unique_ptr<BeebThread::Message> reset_message;
    uint64_t reset_timeout_cycles=0;
    std::unique_ptr<BeebThread::Message> paste_message;
#if HTTP_SERVER
    std::unique_ptr<BeebThread::Message> async_call_message;
#endif

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

    ASSERT(sound_freq>=0);
    m_audio_thread_data=new AudioThreadData((uint64_t)sound_freq,(uint64_t)sound_buffer_size_samples,100);

    this->SetBBCVolume(MAX_DB);
    this->SetDiscVolume(MAX_DB);

    MUTEX_SET_NAME(m_mutex,"BeebThread");
    m_mq.SetName("BeebThread MQ");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebThread::~BeebThread() {
    this->Stop();

    delete m_audio_thread_data;
    m_audio_thread_data=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::Start() {
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

void BeebThread::Send(std::unique_ptr<Message> message) {
    m_mq.ProducerPush(std::move(message));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendTimingMessage(uint64_t max_sound_units) {
    m_mq.ProducerPushIndexed(0,std::make_unique<TimingMessage>(max_sound_units));
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
    return m_has_nvram.load(std::memory_order_acquire);
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
    if(!m_sound_output.GetConsumerBuffers(&sa,&num_sa,&sb,&num_sb)) {
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
                //m_sound_output.ConsumerUnlock(0);
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

    for(size_t sample_idx=0;sample_idx<num_samples;++sample_idx) {
        uint64_t num_units_=remapper->Step();
        ASSERT(num_units_<=SIZE_MAX);
        size_t num_units=(size_t)num_units_;

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

            m_sound_output.Consume(num_units);
            atd->num_consumed_sound_units+=num_units;
        }

        dest[sample_idx]=acc;
    }

    return num_samples;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//float BeebThread::GetBBCVolume() const {
//    return m_bbc_sound_db;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetBBCVolume(float db) {
    this->SetVolume(&m_audio_thread_data->bbc_sound_scale,db);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//float BeebThread::GetDiscVolume() const {
//    return m_disc_sound_db;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetDiscVolume(float db) {
    this->SetVolume(&m_audio_thread_data->disc_sound_scale,db);
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

//uint64_t BeebThread::GetParentTimelineEventId() const {
//    std::lock_guard<Mutex> lock(m_mutex);
//
//    return m_parent_timeline_event_id;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//uint64_t BeebThread::GetLastSavedStateTimelineId() const {
//    std::lock_guard<Mutex> lock(m_mutex);
//
//    return m_last_saved_state_timeline_id;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadRecordEvent(ThreadState *ts,BeebEvent &&event) {
    this->ThreadHandleEvent(ts,event,false);

    if(!!ts->record_timeline) {
        ts->record_timeline->AddEvent(std::move(event));
    }
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
bool BeebThread::ThreadStopTraceOnOSWORD0(const BBCMicro *beeb,const M6502 *cpu,void *context) {
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

bool BeebThread::ThreadStopCopyOnOSWORD0(const BBCMicro *beeb,const M6502 *cpu,void *context) {
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

bool BeebThread::ThreadAddCopyData(const BBCMicro *beeb,const M6502 *cpu,void *context) {
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
    ts->beeb->StartTrace(ts->trace_conditions.trace_flags,ts->trace_max_num_bytes);
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

        this->ThreadFailCompletionFun(&ts->reset_message);
        this->ThreadFailCompletionFun(&ts->paste_message);
    }

    ts->num_executed_2MHz_cycles=ts->beeb->GetNum2MHzCycles();

    {
        AudioDeviceLock lock(m_sound_device_id);

        m_audio_thread_data->num_consumed_sound_units=*ts->num_executed_2MHz_cycles>>SOUND_CLOCK_SHIFT;
    }

    m_has_nvram.store(ts->beeb->GetNVRAMSize()>0,std::memory_order_release);

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

//static void KeepOldROM(std::shared_ptr<const BeebRomData> *new_rom_ptr,const std::shared_ptr<const BeebRomData> &old_rom) {
//    if(*new_rom_ptr!=old_rom) {
//        if(!std::equal(old_rom->begin(),old_rom->end(),(*new_rom_ptr)->begin())) {
//            *new_rom_ptr=old_rom;
//        }
//    }
//}

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

//    case BeebEventType_Root:
//        {
//            ts->current_config=event.data.config->config;
//
//            uint32_t flags=BeebThreadReplaceFlag_KeepCurrentDiscs;
//            if(!replay) {
//                flags|=BeebThreadReplaceFlag_ApplyPCState;
//            }
//
//            this->ThreadReplaceBeeb(ts,
//                                    event.data.config->config.CreateBBCMicro(event.time_2MHz_cycles),
//                                    flags);
//        }
//        return;

    case BeebEventType_HardReset:
        {
            uint32_t flags=BeebThreadReplaceFlag_KeepCurrentDiscs;

            if(event.data.hard_reset->boot) {
                flags|=BeebThreadReplaceFlag_Autoboot;
            }

            if(!replay) {
                flags|=BeebThreadReplaceFlag_ApplyPCState;
            }

            ts->current_config=event.data.hard_reset->loaded_config;

            this->ThreadReplaceBeeb(ts,
                                    event.data.hard_reset->loaded_config.CreateBBCMicro(event.time_2MHz_cycles),
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

    case BeebEventType_SetExtByte:
        {
            ts->beeb->SetExtMemory(event.data.set_ext_byte.address,event.data.set_ext_byte.value);
        }
            return;

    case BeebEventType_DebugAsyncCall:
        {
            ts->beeb->DebugSetAsyncCall(event.data.debug_async_call.addr,
                                        event.data.debug_async_call.a,
                                        event.data.debug_async_call.x,
                                        event.data.debug_async_call.y,
                                        event.data.debug_async_call.c,
                                        &DebugAsyncCallCallback,ts);
        }
        return;
#endif
    }

    ASSERT(false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadStartReplay(ThreadState *ts,std::unique_ptr<Timeline> timeline) {
    ts->replay_timeline=std::move(timeline);

    ts->replay_next_index=0;

    if(ts->replay_timeline->GetNumEvents()==0) {
        ts->replay_next_event_cycles=ts->replay_timeline->GetEnd2MHzCycles();
    } else {
        ts->replay_next_event_cycles=ts->replay_timeline->GetEventByIndex(0)->time_2MHz_cycles;
    }

    ASSERT(!m_is_replaying);
    m_is_replaying.store(true,std::memory_order_release);

    //ts->replay_data.Dump(&ts->log);

    //LOGF(OUTPUT,"replay start.\n");

    // Load initial state.
    this->ThreadReplaceBeeb(ts,ts->replay_timeline->GetInitialBeebState(),0);

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

void BeebThread::ThreadLoadState(ThreadState *ts,const std::shared_ptr<BeebState> &state) {
    this->ThreadReplaceBeeb(ts,state->CloneBBCMicro(),0);

//    m_parent_timeline_event_id=parent_timeline_id;
//
//    Timeline::DidChange();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadHandleReplayEvents(ThreadState *ts) {
    ASSERT(m_is_replaying);

//    ASSERT(ts->replay_next_event_cycles<=ts->replay_timeline.GetEnd2MHzCycles());
//    if(ts->replay_next_event_cycles==ts->replay_timeline.GetEnd2MHzCycles()) {
//        // Replay done.
//        this->ThreadStopReplay(ts);
//        return;
//    }

    const BeebEvent *event=nullptr;

    if(ts->replay_next_index<ts->replay_timeline->GetNumEvents()) {
        for(;;) {
            event=ts->replay_timeline->GetEventByIndex(ts->replay_next_index);

            ASSERT(event->time_2MHz_cycles>=ts->replay_next_event_cycles);

            if(event->time_2MHz_cycles>ts->replay_next_event_cycles) {
                // this event is in the future - done.
                break;
            }

            ++ts->replay_next_index;

            this->ThreadHandleEvent(ts,*event,true);
        }
    }

    ASSERT(ts->replay_next_event_cycles<=ts->replay_timeline->GetEnd2MHzCycles());
    if(ts->replay_next_event_cycles==ts->replay_timeline->GetEnd2MHzCycles()) {
        // Replay is done.
        this->ThreadStopReplay(ts);
        return;
    }

    if(ts->replay_next_index<ts->replay_timeline->GetNumEvents()) {
        ts->replay_next_event_cycles=ts->replay_timeline->GetEnd2MHzCycles();
    } else {
        ts->replay_next_event_cycles=event->time_2MHz_cycles;
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

void BeebThread::ThreadFailCompletionFun(std::unique_ptr<Message> *message_ptr) {
    if(!!*message_ptr) {
        (*message_ptr)->CallCompletionFun(false);
        message_ptr->reset();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadHandleMessage(
    ThreadState *ts,
    std::unique_ptr<Message> message,
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
            if(m_is_replaying) {
                // Ignore.
            } else {
                auto m=(HardResetMessage *)message.get();

                if(m->loaded_config) {
                    this->ThreadRecordEvent(ts,BeebEvent::MakeHardReset(*ts->num_executed_2MHz_cycles,std::move(*m->loaded_config),m->boot));
                } else if(m->reload_config) {
                    BeebLoadedConfig reloaded_config;
                    if(!BeebLoadedConfig::Load(&reloaded_config,ts->current_config.config,&ts->msgs)) {
                        if(!!m->completion_fun) {
                            m->completion_fun(false);
                        }
                        break;
                    }

                    reloaded_config.ReuseROMs(ts->current_config);

                    this->ThreadRecordEvent(ts,BeebEvent::MakeHardReset(*ts->num_executed_2MHz_cycles,std::move(reloaded_config),m->boot));
                } else {
                    this->ThreadRecordEvent(ts,BeebEvent::MakeHardReset(*ts->num_executed_2MHz_cycles,ts->current_config,m->boot));
                }

                if(!!m->completion_fun) {
                    ts->reset_message=std::move(message);
                    ts->reset_timeout_cycles=*ts->num_executed_2MHz_cycles+10000000;
                    ts->beeb->AddInstructionFn(&ThreadWaitForHardReset,ts);
                }

#if BBCMICRO_DEBUGGER
                if(m->run) {
                    ts->beeb->DebugRun();
                }
#endif
            }
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

                if(m->disc_image->CanClone()) {
                    this->ThreadRecordEvent(ts,
                                            BeebEvent::MakeLoadDiscImage(*ts->num_executed_2MHz_cycles,
                                                                         m->drive,
                                                                         std::move(m->disc_image)));
                } else if(!!ts->record_timeline) {
                    ts->msgs.e.f("Can't load this type of disc image while recording\n");
                    ts->msgs.i.f("(%s: %s)\n",
                                 m->disc_image->GetLoadMethod().c_str(),
                                 m->disc_image->GetName().c_str());
                } else {
                    this->ThreadSetDiscImage(ts,m->drive,std::move(m->disc_image));
                }
            }
        }
        break;

    case BeebThreadMessageType_SetPaused:
        {
            auto m=(PauseMessage *)message.get();

            m_paused=m->pause;
        }
        break;

//    case BeebThreadMessageType_GoToTimelineNode:
//        {
//            if(m_is_replaying) {
//                // ignore
//            } else {
//                auto m=(GoToTimelineNodeMessage *)message.get();
//
//                auto &&replay_data=Timeline::CreateReplay(m->timeline_id,m->timeline_id,&ts->msgs);
//
//                ThreadStartReplay(ts,replay_data);
//            }
//        }
//        break;

    case BeebThreadMessageType_LoadState:
        {
            if(m_is_replaying) {
                // ignore
            } else {
                auto m=(LoadStateMessage *)message.get();

                this->ThreadLoadState(ts,m->state);
            }
        }
        break;

//    case BeebThreadMessageType_CloneThisThread:
//        {
//            auto m=(CloneThisThreadMessage *)message.get();
//
//            std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);
//
//            m->dest_thread->Send(std::make_unique<LoadStateMessage>(m_parent_timeline_event_id,std::move(state)));
//        }
//        break;

    case BeebThreadMessageType_CloneWindow:
        {
            auto m=(CloneWindowMessage *)message.get();

            BeebWindowInitArguments init_arguments=std::move(m->init_arguments);

            init_arguments.initially_paused=false;
            init_arguments.initial_state=this->ThreadSaveState(ts);

//            ASSERT(init_arguments.parent_timeline_event_id==0);
//            init_arguments.parent_timeline_event_id=m_parent_timeline_event_id;

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

//            m_last_saved_state_timeline_id=m_parent_timeline_event_id;
        }
        break;

#if BBCMICRO_TRACE
    case BeebThreadMessageType_StartTrace:
        {
            ASSERT(ts->beeb);

            {
                auto m=(StartTraceMessage *)message.get();

                ts->trace_conditions=m->conditions;
                ts->trace_max_num_bytes=m->max_num_bytes;
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

//    case BeebThreadMessageType_Replay:
//        {
//            auto m=(ReplayMessage *)message.get();
//
//            this->ThreadStartReplay(ts,std::move(m->replay_data));
//        }
//        break;

//    case BeebThreadMessageType_SaveAndReplayFrom:
//        {
//            auto m=(SaveAndReplayFromMessage *)message.get();
//            //uint64_t start_timeline_id=msg->data.u64;
//
//            // Add a placeholder event that will serve as the finish event for the replay.
//            m_parent_timeline_event_id=Timeline::AddEvent(m_parent_timeline_event_id,BeebEvent::MakeNone(*ts->num_executed_2MHz_cycles));
//
//            std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);
//
//            m_pre_replay_parent_timeline_event_id=m_parent_timeline_event_id;
//            m_pre_replay_state=state;
//
//            auto &&replay_data=Timeline::CreateReplay(m->timeline_start_id,m_parent_timeline_event_id,&ts->msgs);
//
//            if(!replay_data.events.empty()) {
//                this->ThreadStartReplay(ts,replay_data);
//            }
//        }
//        break;

//    case BeebThreadMessageType_SaveAndVideoFrom:
//        {
//            auto m=(SaveAndVideoFromMessage *)message.get();
//            //auto ptr=(SaveAndVideoFromMessagePayload  *)msg->data.ptr;
//
//            this->ThreadRecordEvent(ts,BeebEvent::MakeNone(*ts->num_executed_2MHz_cycles));
//
//            auto &&replay_data=Timeline::CreateReplay(m->timeline_start_id,m_parent_timeline_event_id,&ts->msgs);
//
//            if(!replay_data.events.empty()) {
//                auto job=std::make_shared<WriteVideoJob>(std::move(replay_data),std::move(m->video_writer),m_message_list);
//                BeebWindows::AddJob(job);
//            }
//        }
//        break;

//    case BeebThreadMessageType_LoadLastState:
//        {
//            if(m_last_saved_state_timeline_id!=0) {
//                //auto m=(BeebThreadLoadLastStateMessage *)message.get();
//                this->Send(std::make_unique<GoToTimelineNodeMessage>(m_last_saved_state_timeline_id));
//                //this->SendGoToTimelineNodeMessage(id);
//            }
//        }
//        break;

    case BeebThreadMessageType_CancelReplay:
        {
            if(m_is_replaying.load(std::memory_order_acquire)) {
                this->ThreadStopReplay(ts);

//                uint64_t timeline_id;

                std::shared_ptr<BeebState> state=std::move(m_pre_replay_state);

//                timeline_id=m_pre_replay_parent_timeline_event_id;
//                m_pre_replay_parent_timeline_event_id=0;

                if(!!state) {
                    this->ThreadLoadState(ts,state);
                }

                //Timeline::DidChange();
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

                if(!!m->completion_fun) {
                    ts->paste_message=std::move(message);
                }
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

#if BBCMICRO_DEBUGGER
        case BeebThreadMessageType_SetExtByte:
        {
            auto m=(DebugSetExtByteMessage *)message.get();
            //M6502Word address={(uint16_t)msg->u32};
            //uint8_t value=(uint8_t)msg->data.u64;

            this->ThreadRecordEvent(ts,BeebEvent::MakeSetExtByte(*ts->num_executed_2MHz_cycles,m->addr,m->value));
        }
        break;
#endif

#if BBCMICRO_DEBUGGER
    case BeebThreadMessageType_DebugAsyncCall:
        {
            auto m=(DebugAsyncCallMessage *)message.get();

            this->ThreadRecordEvent(ts,BeebEvent::MakeAsyncCall(*ts->num_executed_2MHz_cycles,m->addr,m->a,m->x,m->y,m->c));

            ts->async_call_message=std::move(message);
        }
        break;
#endif

    case BeebThreadMessageType_Custom:
        {
            auto m=(CustomMessage *)message.get();

            m->ThreadHandleMessage(ts->beeb);
        }
        break;

    case BeebThreadMessageType_Timing:
        {
            auto m=(TimingMessage *)message.get();

            *next_stop_2MHz_cycles=m->max_sound_units<<SOUND_CLOCK_SHIFT;
#if LOGGING
            m_audio_thread_data->num_executed_cycles.store(*ts->num_executed_2MHz_cycles,std::memory_order_release);
#endif
        }
        break;
    }

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

    std::vector<std::unique_ptr<Message>> messages;

    for(;;) {
        if(paused||(limit_speed&&next_stop_2MHz_cycles<=*ts.num_executed_2MHz_cycles)) {
        wait_for_message:
            rmt_ScopedCPUSample(MessageQueueWaitForMessage,0);
            m_mq.ConsumerWaitForMessages(&messages);
        } else {
            m_mq.ConsumerPollForMessages(&messages);
        }

        if(!messages.empty()) {
            std::lock_guard<Mutex> lock(m_mutex);

            for(size_t i=0;i<messages.size();++i) {
                if(!this->ThreadHandleMessage(&ts,
                                              std::move(messages[i]),
                                              &limit_speed,
                                              &next_stop_2MHz_cycles))
                {
                    goto done;
                }
            }

            messages.clear();
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
                ts.paste_message.reset();
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
            size_t num_video_units=(size_t)num_2MHz_cycles;
            if(!m_video_output.GetProducerBuffers(&va,&num_va,&vb,&num_vb)) {
                goto wait_for_message;
            }

            if(num_va+num_vb>num_video_units) {
                if(num_va>num_video_units) {
                    num_va=num_video_units;
                    num_vb=0;
                } else {
                    num_vb=num_video_units-num_va;
                }
            }

            size_t num_sound_units=(size_t)((num_va+num_vb+(1<<SOUND_CLOCK_SHIFT)-1)>>SOUND_CLOCK_SHIFT);

            SoundDataUnit *sa,*sb;
            size_t num_sa,num_sb;
            if(!m_sound_output.GetProducerBuffers(&sa,&num_sa,&sb,&num_sb)) {
                goto wait_for_message;
            }

            if(num_sa+num_sb<num_sound_units) {
                goto wait_for_message;
            }

            SoundDataUnit *sunit=sa;

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

                            m_sound_output.Produce(1);
                        }
                    }

                    m_video_output.Produce(i);
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

                            m_sound_output.Produce(1);
                        }
                    }

                    m_video_output.Produce(i);
                }
            }

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

void BeebThread::SetVolume(float *scale_var,float db) {
    if(db>MAX_DB) {
        db=MAX_DB;
    }

    if(db<MIN_DB) {
        db=MIN_DB;
    }

    {
        AudioDeviceLock lock(m_sound_device_id);

        *scale_var=powf(10.f,db/20.f);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadWaitForHardReset(const BBCMicro *beeb,const M6502 *cpu,void *context) {
    (void)beeb;
    auto ts=(ThreadState *)context;

    // Watch for OSWORD 0, OSRDCH, or 5 seconds.
    if((cpu->opcode_pc.w==0xfff1&&cpu->a==0)||
       cpu->opcode_pc.w==0xffe0||
       *ts->num_executed_2MHz_cycles>ts->reset_timeout_cycles)
    {
        ts->reset_message=nullptr;
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if HTTP_SERVER
void BeebThread::DebugAsyncCallCallback(bool called,void *context) {
    auto ts=(ThreadState *)context;

    if(!!ts->async_call_message) {
        ts->async_call_message->CallCompletionFun(called);
        ts->async_call_message.reset();
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
