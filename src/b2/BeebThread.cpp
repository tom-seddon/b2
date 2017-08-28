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

    this->SetPaused(true);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const Message STOP_MESSAGE={BeebThreadEventType_Stop};

void BeebThread::Stop() {
    if(m_thread.joinable()) {
        MessageQueuePush(m_mq,&STOP_MESSAGE);

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

void BeebThread::SendMessage(BeebThreadEventType type,uint32_t u32,uint64_t data) {
    if(m_mq) {
        Message message={};

        ASSERT(type>=0);
#ifdef _MSC_VER
        // Gives a tautological-constant-out-of-range-compare warning
        // on gcc/clang. That makes sense, but I'm not sure how to
        // inhibit it, and I want to check anyway.
        ASSERT(type<=UINT32_MAX);
#endif
        message.type=(uint32_t)type;

        message.u32=u32;
        message.data.u64=data;

        MessageQueuePush(m_mq,&message);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendMessage(BeebThreadEventType type,uint32_t u32,void *data,void (*destroy_fn)(Message *)) {
    Message message={};

    ASSERT(type>=0);
#ifdef _MSC_VER
    // Gives a tautological-constant-out-of-range-compare warning on
    // gcc/clang. That makes sense, but I'm not sure how to inhibit
    // it, and I want to check anyway.
    ASSERT(type<=UINT32_MAX);
#endif
    message.type=(uint32_t)type;

    message.u32=u32;
    message.data.ptr=data;
    message.destroy_fn=destroy_fn;

    if(m_mq) {
        MessageQueuePush(m_mq,&message);
    } else if(destroy_fn) {
        (*destroy_fn)(&message);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::AddSyntheticMessage(BeebThreadSyntheticEventType type,uint64_t data) {
    if(m_mq) {
        Message::Data tmp;

        tmp.u64=data;

        MessageQueueAddSyntheticMessage(m_mq,type,tmp);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendKeyMessage(BeebKey key,bool state) {
    this->SendMessage(BeebThreadEventType_KeyState,(uint32_t)key,state);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendKeySymMessage(BeebKeySym key_sym,bool state) {
    this->SendMessage(BeebThreadEventType_KeySymState,(uint32_t)key_sym,state);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendTimingMessage(uint64_t max_sound_units) {
    this->AddSyntheticMessage(BeebThreadSyntheticEventType_SoundClockTimer,max_sound_units);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendHardResetMessage(bool boot) {
    this->SendMessage(BeebThreadEventType_HardReset,0,!!boot);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ChangeConfigPayload {
    BeebLoadedConfig config;
};

void BeebThread::SendChangeConfigMessage(BeebLoadedConfig config) {
    auto ptr=new ChangeConfigPayload;

    ptr->config=std::move(config);

    this->SendMessageWithPayload(BeebThreadEventType_ChangeConfig,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendSetSpeedLimitingMessage(bool limit_speed) {
    this->SendMessage(BeebThreadEventType_SetSpeedLimiting,0,limit_speed);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct LoadDiscPayload {
    // drive to load the disc into.
    int drive=-1;

    // owning shared_ptr of the disc image in question.
    std::shared_ptr<DiscImage> disc_image;

    // whether to print stuff to the message log as it goes.
    bool verbose=false;
};

void BeebThread::SendLoadDiscMessage(int drive,std::shared_ptr<DiscImage> disc_image,bool verbose) {
    ASSERT(drive>=0&&drive<NUM_DRIVES);

    auto ptr=new LoadDiscPayload;

    ptr->drive=drive;
    ptr->disc_image=std::move(disc_image);
    ptr->verbose=verbose;

    this->SendMessageWithPayload(BeebThreadEventType_LoadDisc,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendDebugFlagsMessage(uint32_t debug_flags) {
    this->SendMessage(BeebThreadEventType_DebugFlags,debug_flags,nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct LoadStateMessagePayload {
    // previous timeline id.
    uint64_t parent_timeline_id=0;

    // the state to load.
    std::shared_ptr<BeebState> state;
};

void BeebThread::SendLoadStateMessage(uint64_t parent_timeline_id,std::shared_ptr<BeebState> state) {
    auto ptr=new LoadStateMessagePayload;

    ptr->parent_timeline_id=parent_timeline_id;
    ptr->state=state;

    this->SendMessageWithPayload(BeebThreadEventType_LoadState,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendGoToTimelineNodeMessage(uint64_t timeline_id) {
    this->SendMessage(BeebThreadEventType_GoToTimelineNode,0,timeline_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SaveStateMessagePayload {
    // whether to print a message about it.
    bool verbose=false;
};

void BeebThread::SendSaveStateMessage(bool verbose) {
    auto ptr=new SaveStateMessagePayload;

    ptr->verbose=verbose;

    this->SendMessageWithPayload(BeebThreadEventType_SaveState,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ReplayMessagePayload {
    Timeline::ReplayData replay_data;
};

void BeebThread::SendReplayMessage(Timeline::ReplayData replay_data) {
    auto ptr=new ReplayMessagePayload;

    ptr->replay_data=std::move(replay_data);

    this->SendMessageWithPayload(BeebThreadEventType_Replay,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendSaveAndReplayFromMessage(uint64_t timeline_start_id) {
    this->SendMessage(BeebThreadEventType_SaveAndReplayFrom,0,timeline_start_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SaveAndVideoFromMessagePayload {
    uint64_t timeline_start_id=0;
    std::unique_ptr<VideoWriter> video_writer;
};

void BeebThread::SendSaveAndVideoFromMessage(uint64_t timeline_start_id,std::unique_ptr<VideoWriter> video_writer) {
    auto ptr=new SaveAndVideoFromMessagePayload;

    ptr->timeline_start_id=timeline_start_id;
    ptr->video_writer=std::move(video_writer);

    this->SendMessageWithPayload(BeebThreadEventType_SaveAndVideoFrom,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SetDiscImageNameAndLoadMethodMessagePayload {
    int drive;
    std::string name;
    std::string load_method;
};

void BeebThread::SendSetDiscImageNameAndLoadMethodMessage(int drive,std::string name,std::string load_method) {
    ASSERT(drive>=0&&drive<NUM_DRIVES);

    auto ptr=new SetDiscImageNameAndLoadMethodMessagePayload;

    ptr->drive=drive;
    ptr->name=std::move(name);
    ptr->load_method=std::move(load_method);

    this->SendMessageWithPayload(BeebThreadEventType_SetDiscImageNameAndLoadMethod,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TURBO_DISC
void BeebThread::SendSetTurboDiscMessage(bool turbo) {
    this->SendMessage(BeebThreadEventType_SetTurboDisc,0,turbo);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TURBO_DISC
bool BeebThread::IsTurboDisc() const {
    return m_is_turbo_disc.load(std::memory_order_acquire);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct StartPasteMessagePayload {
    std::string text;
};

void BeebThread::SendStartPasteMessage(std::string text) {
    auto ptr=new StartPasteMessagePayload;

    ptr->text=std::move(text);

    this->SendMessageWithPayload(BeebThreadEventType_StartPaste,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendStopPasteMessage() {
    this->SendMessage(BeebThreadEventType_StopPaste,0,nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsPasting() const {
    return m_is_pasting.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct StartCopyMessagePayload {
    bool basic=false;
    std::function<void(const std::vector<uint8_t> &)> stop_fun;
};

void BeebThread::SendStartCopyMessage(std::function<void(std::vector<uint8_t>)> stop_fun) {
    auto ptr=new StartCopyMessagePayload;

    ptr->basic=false;
    ptr->stop_fun=std::move(stop_fun);

    this->SendMessageWithPayload(BeebThreadEventType_StartCopy,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendStartCopyBASICMessage(std::function<void(std::vector<uint8_t>)> stop_fun) {
    auto ptr=new StartCopyMessagePayload;

    ptr->basic=true;
    ptr->stop_fun=std::move(stop_fun);

    this->SendMessageWithPayload(BeebThreadEventType_StartCopy,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct StopyCopyMessagePayload {
    std::function<void(const std::vector<uint8_t> &)> fun;
};

void BeebThread::SendStopCopyMessage() {
    this->SendMessage(BeebThreadEventType_StopCopy,0,nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsCopying() const {
    return m_is_copying.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
struct StartTraceMessagePayload {
    TraceConditions conditions;
};
#endif

#if BBCMICRO_TRACE
void BeebThread::SendStartTraceMessage(const TraceConditions &conditions) {
    auto ptr=new StartTraceMessagePayload;

    ptr->conditions=conditions;

    this->SendMessageWithPayload(BeebThreadEventType_StartTrace,ptr);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BeebThread::SendStopTraceMessage() {
    this->SendMessage(BeebThreadEventType_StopTrace,0,nullptr);
}
#endif

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

void BeebThread::SendCloneWindowMessage(BeebWindowInitArguments init_arguments) {
    this->SendMessageWithPayload(BeebThreadEventType_CloneWindow,
                                 new BeebWindowInitArguments(std::move(init_arguments)));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CloneThisThreadMessagePayload {
    // the thread to load the state into.
    std::shared_ptr<BeebThread> dest_thread;
};

void BeebThread::SendCloneThisThreadMessage(std::shared_ptr<BeebThread> dest_thread) {
    auto ptr=new CloneThisThreadMessagePayload;

    ptr->dest_thread=std::move(dest_thread);

    this->SendMessageWithPayload(BeebThreadEventType_CloneThisThread,ptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendLoadLastStateMessage() {
    this->SendMessage(BeebThreadEventType_LoadLastState,0,nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SendCancelReplayMessage() {
    this->SendMessage(BeebThreadEventType_CancelReplay,0,nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsSpeedLimited() const {
    return m_limit_speed.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BeebThread::GetDebugFlags() const {
    return m_debug_flags.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::IsPaused() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    return !!m_paused_ts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::SetPaused(bool paused) {
    std::unique_lock<std::mutex> lock(m_mutex);

    bool was_paused=!!m_paused_ts;

    if(was_paused!=paused) {
        this->SendMessage(BeebThreadEventType_SetPaused,0,paused);

        for(;;) {
            m_paused_cv.wait(lock);

            if(!!m_paused_ts==paused) {
                break;
            }
        }
    }

    return was_paused;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BeebThread::GetDiscImage(std::unique_lock<std::mutex> *lock,int drive) const {
    ASSERT(drive>=0&&drive<NUM_DRIVES);

    *lock=std::unique_lock<std::mutex>(m_mutex);

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
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_nvram_copy;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::HasNVRAM() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    return !m_nvram_copy.empty();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ClearLastTrace() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_last_trace=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<Trace> BeebThread::GetLastTrace() {
    std::lock_guard<std::mutex> lock(m_mutex);

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

    const SoundDataUnit *sa,*sb;
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
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_parent_timeline_event_id;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t BeebThread::GetLastSavedStateTimelineId() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_last_saved_state_timeline_id;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::FlushCallbacks() {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        callbacks=std::move(m_callbacks);
    }

    for(auto &&callback:callbacks) {
        callback();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::SetLastSavedStateTimelineId(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_last_saved_state_timeline_id=id;
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
    
    m_parent_timeline_event_id=Timeline::AddEvent(this->GetParentTimelineEventId(),std::move(event));
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

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        memset(&m_trace_stats,0,sizeof m_trace_stats);
        m_is_tracing.store(true,std::memory_order_release);
        m_last_trace=nullptr;
    }
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

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_last_trace=last_trace;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadReplaceBeeb(ThreadState *ts,std::unique_ptr<BBCMicro> beeb,uint32_t flags) {
    delete ts->beeb;
    ts->beeb=nullptr;

    ts->beeb=beeb.release();

    ts->num_executed_2MHz_cycles=ts->beeb->GetNum2MHzCycles();

    ts->beeb->SetDiscMutex(&m_mutex);

    {
        AudioDeviceLock lock(m_sound_device_id);

        m_audio_thread_data->num_consumed_sound_units=*ts->num_executed_2MHz_cycles>>SOUND_CLOCK_SHIFT;
    }

    // Always take shadow copy of NVRAM.
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_nvram_copy.clear();
        m_nvram_copy.resize(ts->beeb->GetNVRAMSize());
    }

    ts->beeb->SetNVRAMCallback(&ThreadBBCMicroNVRAMCallback,this);

    // Always set debug flags. They don't affect reproducibility.
    ts->beeb->SetDebugFlags(m_debug_flags.load(std::memory_order_acquire));

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
            std::unique_lock<std::mutex> lock;
            m_disc_images[i]=ts->beeb->GetDiscImage(&lock,i);
        }
    }


#if BBCMICRO_TRACE
    if(m_is_tracing) {
        this->ThreadStartTrace(ts);
    }
#endif

    ts->beeb->GetAndResetDiscAccessFlag();
    this->ThreadSetBootState(ts,!!(flags&BeebThreadReplaceFlag_Autoboot));

    this->ThreadSetPaused(ts,false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadBBCMicroNVRAMCallback(BBCMicro *m,size_t offset,uint8_t value,void *context) {
    (void)m;

    auto this_=(BeebThread *)context;

    {
        std::lock_guard<std::mutex> lock(this_->m_mutex);

        ASSERT(offset<this_->m_nvram_copy.size());
        this_->m_nvram_copy[offset]=value;
    }
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
    switch((BeebEventType)event.type) {
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

void BeebThread::ThreadSetPaused(ThreadState *ts,bool paused) {
    (void)ts;

    std::lock_guard<std::mutex> lock(m_mutex);

    if(!!m_paused_ts!=paused) {
        if(paused) {
            m_paused_ts=ts;
        } else {
            m_paused_ts=nullptr;
        }

        m_paused_cv.notify_all();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::ThreadLoadState(ThreadState *ts,uint64_t parent_timeline_id,const std::shared_ptr<BeebState> &state) {
    this->ThreadReplaceBeeb(ts,state->CloneBBCMicro(),0);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_parent_timeline_event_id=parent_timeline_id;
    }

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
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                m_parent_timeline_event_id=re->id;
            }

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

    std::lock_guard<std::mutex> lock(m_mutex);
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

    this->AddCallback([data=std::move(ts->copy_data),fun=std::move(ts->copy_stop_fun)]() {
        fun(std::move(data));
    });

    m_is_copying.store(false,std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebThread::ThreadHandleMessage(
    ThreadState *ts,
    Message *msg,
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
        ASSERT(0);
        // fall through
    case BeebThreadEventType_Stop:
        result=false;
        break;

    case BeebThreadEventType_KeyState:
        {
            if(m_is_replaying) {
                // Ignore.
            } else {
                ASSERT(ts->beeb);

                ASSERT((msg->u32&0x7f)==msg->u32);
                auto key=(BeebKey)msg->u32;
                bool state=msg->data.u64!=0;

                this->ThreadSetKeyState(ts,key,state);
            }
        }
        break;

    case BeebThreadEventType_KeySymState:
        {
            if(m_is_replaying) {
                // Ignore.
            } else {
                ASSERT(ts->beeb);

                ASSERT((msg->u32&0x7f)==msg->u32);
                BeebKeySym key=(BeebKeySym)msg->u32;

                bool state=msg->data.u64!=0;

                BeebKey beeb_key;
                BeebShiftState shift_state;
                if(GetBeebKeyComboForKeySym(&beeb_key,&shift_state,key)) {
                    this->ThreadSetFakeShiftState(ts,state?shift_state:BeebShiftState_Any);
                    this->ThreadSetKeyState(ts,beeb_key,state);
                }
            }
        }
        break;

    case BeebThreadEventType_HardReset:
        {
            uint32_t flags=BeebThreadReplaceFlag_KeepCurrentDiscs;

            if(msg->data.u64) {
                flags|=BeebThreadReplaceFlag_Autoboot;
            }

            ts->log.f("%s: flags=%s\n",GetBeebThreadEventTypeEnumName((int)msg->type),GetFlagsString(flags,&GetBeebThreadReplaceFlagEnumName).c_str());

            // this->ThreadReplaceBeeb(ts,ts->current_config.CreateBBCMicro(),flags|BeebThreadReplaceFlag_ApplyPCState);

            this->ThreadRecordEvent(ts,BeebEvent::MakeHardReset(*ts->num_executed_2MHz_cycles,flags));
        }
        break;

    case BeebThreadEventType_ChangeConfig:
        {
            auto ptr=(ChangeConfigPayload *)msg->data.ptr;

            ts->current_config=std::move(ptr->config);
            
            this->ThreadRecordEvent(ts,BeebEvent::MakeChangeConfig(*ts->num_executed_2MHz_cycles,ts->current_config));
        }
        break;

    case BeebThreadEventType_SetSpeedLimiting:
        {
            *limit_speed=msg->data.u64!=0;
            m_limit_speed.store(*limit_speed,std::memory_order_release);
        }
        break;

    case BeebThreadEventType_LoadDisc:
        {
            //ts->log.f("%s:\n",GetBeebThreadEventTypeEnumName((int)msg->type));

            if(m_is_replaying) {
                // Ignore.
            } else {
                auto ptr=(LoadDiscPayload *)msg->data.ptr;

                if(ptr->verbose) {
                    if(!ptr->disc_image) {
                        ts->msgs.i.f("Drive %d: empty\n",ptr->drive);
                    } else {
                        ts->msgs.i.f("Drive %d: %s\n",ptr->drive,ptr->disc_image->GetName().c_str());

                        std::string hash=ptr->disc_image->GetHash();
                        if(!hash.empty()) {
                            ts->msgs.i.f("(Hash: %s)\n",hash.c_str());
                        }
                    }
                }

                this->ThreadRecordEvent(ts,BeebEvent::MakeLoadDiscImage(*ts->num_executed_2MHz_cycles,ptr->drive,std::move(ptr->disc_image)));
            }
        }
        break;

    case BeebThreadEventType_SetPaused:
        {
            bool is_paused=!!msg->data.u64;

            this->ThreadSetPaused(ts,is_paused);
        }
        break;

    case BeebThreadEventType_DebugFlags:
        {
            ASSERT(ts->beeb);

            uint32_t flags=msg->u32;

            ts->beeb->SetDebugFlags(flags);

            m_debug_flags.store(flags,std::memory_order_release);
        }
        break;

    case BeebThreadEventType_GoToTimelineNode:
        {
            if(m_is_replaying) {
                // ignore
            } else {
                uint64_t timeline_id=msg->data.u64;

                auto &&replay_data=Timeline::CreateReplay(timeline_id,timeline_id,&ts->msgs);

                ThreadStartReplay(ts,replay_data);
            }
        }
        break;

    case BeebThreadEventType_LoadState:
        {
            if(m_is_replaying) {
                // ignore
            } else {
                auto ptr=(LoadStateMessagePayload *)msg->data.ptr;

                this->ThreadLoadState(ts,ptr->parent_timeline_id,ptr->state);
            }
        }
        break;

    case BeebThreadEventType_CloneThisThread:
        {
            auto ptr=(CloneThisThreadMessagePayload *)msg->data.ptr;

            std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);

            ptr->dest_thread->SendLoadStateMessage(m_parent_timeline_event_id,std::move(state));
        }
        break;

    case BeebThreadEventType_CloneWindow:
        {
            BeebWindowInitArguments init_arguments=*(BeebWindowInitArguments *)msg->data.ptr;

            init_arguments.initially_paused=false;
            init_arguments.initial_state=this->ThreadSaveState(ts);

            ASSERT(init_arguments.parent_timeline_event_id==0);
            init_arguments.parent_timeline_event_id=m_parent_timeline_event_id;

            PushNewWindowMessage(init_arguments);
        }
        break;

    case BeebThreadEventType_SaveState:
        {
            auto ptr=(SaveStateMessagePayload *)msg->data.ptr;

            std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);

            if(ptr->verbose) {
                std::string time_str=Get2MHzCyclesString(state->GetEmulated2MHzCycles());
                ts->msgs.i.f("Saved state: %s\n",time_str.c_str());
            }
            
            this->ThreadRecordEvent(ts,BeebEvent::MakeSaveState(*ts->num_executed_2MHz_cycles,state));

            this->SetLastSavedStateTimelineId(this->GetParentTimelineEventId());

        }
        break;

#if BBCMICRO_TRACE
    case BeebThreadEventType_StartTrace:
        {
            ASSERT(ts->beeb);

            {
                auto ptr=(StartTraceMessagePayload *)msg->data.ptr;

                ts->trace_conditions=ptr->conditions;
            }

            this->ThreadStartTrace(ts);
        }
        break;
#endif

#if BBCMICRO_TRACE
    case BeebThreadEventType_StopTrace:
        {
            this->ThreadStopTrace(ts);
        }
        break;
#endif

    case BeebThreadEventType_SetTurboDisc:
        {
            this->ThreadRecordEvent(ts,
                                    BeebEvent::MakeSetTurboDisc(*ts->num_executed_2MHz_cycles,
                                                                !!msg->data.u64));
        }
        break;

    case BeebThreadEventType_Replay:
        {
            auto ptr=(ReplayMessagePayload *)msg->data.ptr;

            this->ThreadStartReplay(ts,std::move(ptr->replay_data));
        }
        break;

    case BeebThreadEventType_SaveAndReplayFrom:
        {
            uint64_t start_timeline_id=msg->data.u64;

            // Add a placeholder event that will serve as the finish event for the replay.
            m_parent_timeline_event_id=Timeline::AddEvent(m_parent_timeline_event_id,BeebEvent::MakeNone(*ts->num_executed_2MHz_cycles));

            std::shared_ptr<BeebState> state=this->ThreadSaveState(ts);

            // Save the current state so the replay can be canceled.
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                m_pre_replay_parent_timeline_event_id=m_parent_timeline_event_id;
                m_pre_replay_state=state;
            }

            auto &&replay_data=Timeline::CreateReplay(start_timeline_id,m_parent_timeline_event_id,&ts->msgs);

            if(!replay_data.events.empty()) {
                this->ThreadStartReplay(ts,replay_data);
            }
        }
        break;

    case BeebThreadEventType_SaveAndVideoFrom:
        {
            auto ptr=(SaveAndVideoFromMessagePayload  *)msg->data.ptr;

            this->ThreadRecordEvent(ts,BeebEvent::MakeNone(*ts->num_executed_2MHz_cycles));

            auto &&replay_data=Timeline::CreateReplay(ptr->timeline_start_id,m_parent_timeline_event_id,&ts->msgs);

            if(!replay_data.events.empty()) {
                auto job=std::make_shared<WriteVideoJob>(std::move(replay_data),std::move(ptr->video_writer),m_message_list);
                BeebWindows::AddJob(job);
            }
        }
        break;

    case BeebThreadEventType_SetDiscImageNameAndLoadMethod:
        {
            auto ptr=(SetDiscImageNameAndLoadMethodMessagePayload *)msg->data.ptr;

            std::unique_lock<std::mutex> lock;

            if(std::shared_ptr<DiscImage> disc_image=ts->beeb->GetMutableDiscImage(&lock,ptr->drive)) {
                disc_image->SetName(std::move(ptr->name));
                disc_image->SetLoadMethod(std::move(ptr->load_method));
            }
        }
        break;

    case BeebThreadEventType_LoadLastState:
        {
            if(uint64_t id=this->GetLastSavedStateTimelineId()) {
                this->SendGoToTimelineNodeMessage(id);
            }
        }
        break;

    case BeebThreadEventType_CancelReplay:
        {
            if(m_is_replaying.load(std::memory_order_acquire)) {
                this->ThreadStopReplay(ts);

                std::shared_ptr<BeebState> state;
                uint64_t timeline_id;

                {
                    std::lock_guard<std::mutex> lock(m_mutex);

                    state=std::move(m_pre_replay_state);
                    ASSERT(!m_pre_replay_state);

                    timeline_id=m_pre_replay_parent_timeline_event_id;
                    m_pre_replay_parent_timeline_event_id=0;
                }

                if(!!state) {
                    this->ThreadLoadState(ts,timeline_id,state);
                }

                Timeline::DidChange();
            }
        }
        break;

    case BeebThreadEventType_StartPaste:
        {
            auto ptr=(StartPasteMessagePayload *)msg->data.ptr;

            if(m_is_replaying) {
                // Ignore.
            } else {
                this->ThreadStartPaste(ts,std::move(ptr->text));
            }
        }
        break;

    case BeebThreadEventType_StopPaste:
        {
            if(m_is_replaying) {
                // Ignore.
            } else {
                this->ThreadRecordEvent(ts,BeebEvent::MakeStopPaste(*ts->num_executed_2MHz_cycles));
            }
        }
        break;

    case BeebThreadEventType_StartCopy:
        {
            auto ptr=(StartCopyMessagePayload *)msg->data.ptr;

            // StartCopy and StartCopyBASIC really aren't the same
            // sort of thing, but they share enough code that it felt
            // a bit daft whichever way round they went.

            if(ptr->basic) {
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

            ts->copy_basic=ptr->basic;
            ts->copy_stop_fun=std::move(ptr->stop_fun);
            m_is_copying.store(true,std::memory_order_release);
        }
        break;

    case BeebThreadEventType_StopCopy:
        {
            this->ThreadStopCopy(ts);
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

    SetCurrentThreadNamef("BeebThread");

    ThreadState ts;

    ts.beeb_thread=this;
    ts.msgs=Messages(m_message_list);

    this->ThreadSetPaused(&ts,true);

    for(;;) {
        Message msg;
        int got_msg;

        if(m_paused_ts||(limit_speed&&next_stop_2MHz_cycles<=*ts.num_executed_2MHz_cycles)) {
        wait_for_message:
            MessageQueueWaitForMessage(m_mq,&msg);
            got_msg=1;
        } else {
            got_msg=MessageQueuePollForMessage(m_mq,&msg);
        }

        if(got_msg) {
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

        if(!m_paused_ts&&stop_2MHz_cycles>*ts.num_executed_2MHz_cycles) {
            ASSERT(ts.beeb);

            uint64_t num_2MHz_cycles=stop_2MHz_cycles-*ts.num_executed_2MHz_cycles;
            if(num_2MHz_cycles>RUN_2MHz_CYCLES) {
                num_2MHz_cycles=RUN_2MHz_CYCLES;
            }

            // One day I'll clear up the units mismatch...
            VideoDataUnit *va,*vb;
            size_t num_va,num_vb;
            size_t num_video_units=num_2MHz_cycles>>1;
            ASSERT((num_2MHz_cycles&1)==0);
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

            // Fill part A.
            {
                VideoDataUnit *vunit=va;

                for(uint64_t i=0;i<num_va;++i) {
                    if(ts.beeb->Update(vunit++,sunit)) {
                        ++sunit;

                        if(sunit==sa+num_sa) {
                            sunit=sb;
                        } else if(sunit==sb+num_sb) {
                            sunit=nullptr;
                        }

                        ++num_sunits_produced;
                    }
                }
            }

            // Fill part B.
            {
                VideoDataUnit *vunit=vb;

                for(uint64_t i=0;i<num_vb;++i) {
                    if(ts.beeb->Update(vunit++,sunit)) {
                        ++sunit;

                        if(sunit==sa+num_sa) {
                            sunit=sb;
                        } else if(sunit==sb+num_sb) {
                            sunit=nullptr;
                        }

                        ++num_sunits_produced;
                    }
                }
            }

            m_video_output.ProducerUnlock(num_va+num_vb);
            m_sound_output.ProducerUnlock(num_sunits_produced);

            // It's a bit dumb having multiple copies.
            m_num_2MHz_cycles.store(*ts.num_executed_2MHz_cycles,std::memory_order_release);
        }
    }
done:;

    delete ts.beeb;
    ts.beeb=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class PayloadType>
void BeebThread::DeleteOuMessagePayload(Message *msg) {
    auto payload=(PayloadType *)msg->data.ptr;
    msg->data.ptr=nullptr;

    delete payload;
    payload=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class PayloadType>
void BeebThread::SendMessageWithPayload(BeebThreadEventType type,PayloadType *payload) {
    this->SendMessage(type,0,payload,&DeleteOuMessagePayload<PayloadType>);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebThread::AddCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_callbacks.emplace_back(std::move(callback));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
