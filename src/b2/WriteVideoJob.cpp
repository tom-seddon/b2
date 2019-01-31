#include <shared/system.h>
#include "WriteVideoJob.h"
#include "TVOutput.h"
#include "conf.h"
#include <SDL.h>
#include <beeb/OutputData.h>
#include "VideoWriter.h"
#include "BeebState.h"
#include <beeb/video.h>
#include <shared/debug.h>
#include "dear_imgui.h"
#include "BeebThread.h"
#include "TVOutput.h"
#include <shared/load_store.h>
#include "load_save.h"
#include <shared/path.h>
#include <beeb/sound.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define WAV 0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if WAV
static void AddChunkHeader(std::vector<uint8_t> *data,const char *fourcc) {
    ASSERT(strlen(fourcc)==4);

    size_t size=data->size();
    ASSERT(size<=UINT32_MAX);

    data->insert(data->begin(),8,0);
    memcpy(data->data()+0,fourcc,4);
    Store32LE(data->data()+4,(uint32_t)size);
}
#endif

#if WAV
static void SaveWAV(
    const std::string &file_name,
    const char *ext,
    const std::vector<uint8_t> data,
    SDL_AudioFormat format,
    uint8_t channels,
    int freq,
    uint16_t wFormatTag,
    Messages *msg)
{
    std::vector<uint8_t> wav=data;
    AddChunkHeader(&wav,"data");

    uint16_t nChannels=channels;
    uint32_t nSamplesPerSec=freq;
    uint16_t wBitsPerSample=SDL_AUDIO_BITSIZE(format);
    uint16_t nBlockAlign=nChannels*wBitsPerSample/8;
    uint32_t nAvgBytesPerSec=nSamplesPerSec*nBlockAlign;

    {
        std::vector<uint8_t> fmt_chunk;
        fmt_chunk.resize(18);

        uint8_t *p=fmt_chunk.data();
        Store16LE(p+0,wFormatTag);
        Store16LE(p+2,nChannels);
        Store32LE(p+4,nSamplesPerSec);
        Store32LE(p+8,nAvgBytesPerSec);
        Store16LE(p+12,nBlockAlign);
        Store16LE(p+14,wBitsPerSample);
        Store16LE(p+16,0);

        AddChunkHeader(&fmt_chunk,"fmt ");

        wav.insert(wav.begin(),fmt_chunk.begin(),fmt_chunk.end());
    }

    for(size_t i=0;i<4;++i) {
        wav.insert(wav.begin(),"EVAW"[i]);
    }

    AddChunkHeader(&wav,"RIFF");

    std::string f=PathWithoutExtension(file_name)+ext;

    SaveFile(wav,f,msg);

    free(f);
    f=nullptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WriteVideoJob::WriteVideoJob(BeebThread::TimelineEventList event_list,
                             std::unique_ptr<VideoWriter> writer):
    m_event_list(std::move(event_list)),
    m_writer(std::move(writer)),
    m_msg(m_writer->GetMessageList())
{
    m_file_name=m_writer->GetFileName();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WriteVideoJob::~WriteVideoJob() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool WriteVideoJob::Error(const char *fmt,...) {
    m_msg.e.f("failed to write video to: %s\n",m_file_name.c_str());

    m_msg.i.f("(");

    {
        va_list v;
        va_start(v,fmt);
        m_msg.i.v(fmt,v);
        va_end(v);
    }

    m_msg.i.f(")\n");

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool WriteVideoJob::WasSuccessful() const {
    return m_success;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool WriteVideoJob::HasImGui() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteVideoJob::DoImGui() {
    ImGui::TextUnformatted(m_file_name.c_str());

    uint64_t cycles_done=m_cycles_done.load(std::memory_order_acquire);

    double real_seconds=GetSecondsFromTicks(m_ticks.load(std::memory_order_acquire));
    double emu_seconds=cycles_done/2.e6;

    char label[50];
    if(real_seconds==0.) {
        label[0]=0;
    } else {
        snprintf(label,sizeof label,"%.3fx",emu_seconds/real_seconds);
    }

    float percentage=(float)cycles_done/m_cycles_total.load(std::memory_order_acquire);
    ImGui::ProgressBar(percentage,ImVec2(-1,0),label);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if WAV
static const int NUM_CHANNELS=4;

static void PushBackFloat(int index,float f,void *context) {
    ASSERT(index>=-1&&index<NUM_CHANNELS);
    auto vec=(std::vector<uint8_t> *)context;
    vec+=index+1;

    uint8_t tmp[4];
    memcpy(tmp,&f,4);

    for(size_t i=0;i<4;++i) {
        vec->push_back(tmp[i]);
    }
}
#endif

void WriteVideoJob::ThreadExecute() {
    //OutputData *video_output_data=m_beeb_thread->GetVideoOutputData();

    ASSERT(!m_event_list.events.empty());
    uint64_t start_cycles=m_event_list.state_event.time_2MHz_cycles;
    uint64_t finish_cycles=m_event_list.events.back().time_2MHz_cycles;
    ASSERT(finish_cycles>=start_cycles);

    uint64_t start_ticks=GetCurrentTickCount();

    m_cycles_done.store(0,std::memory_order_release);
    m_ticks.store(0,std::memory_order_release);
    m_cycles_total.store(finish_cycles-start_cycles,std::memory_order_release);

    SDL_AudioSpec afmt;
    SDL_AudioCVT cvt;
    std::vector<char> audio_buf;
    static const size_t NUM_SAMPLES=4096;
    TVOutput tv_output;
    std::shared_ptr<BeebThread> beeb_thread;
    std::shared_ptr<const BeebState> start_state;
    std::vector<BeebThread::TimelineEventList> event_lists;

    if(!m_writer->BeginWrite()) {
        goto done;
    }

#if WAV
    std::vector<uint8_t> wav_float_data,wav_pcm_data,wav_full_float_data[1+NUM_CHANNELS];
#endif

    int vwidth,vheight;
    {
        if(!m_writer->GetAudioFormat(&afmt)) {
            this->Error("couldn't get audio output format");
            goto done;
        }

        if(SDL_BuildAudioCVT(&cvt,AUDIO_FORMAT,AUDIO_NUM_CHANNELS,afmt.freq,afmt.format,afmt.channels,afmt.freq)<0) {
            this->Error("SDL_BuildAudioCVT failed: %s",SDL_GetError());
            goto done;
        }

        uint32_t vformat;
        if(!m_writer->GetVideoFormat(&vformat,&vwidth,&vheight)) {
            this->Error("couldn't get video output format");
            goto done;
        }

        if(vformat!=SDL_PIXELFORMAT_ARGB8888) {
            this->Error("video output format not ARGB8888: %s\n",SDL_GetPixelFormatName(vformat));
            goto done;
        }

        cvt.len=NUM_SAMPLES*sizeof(float);

        ASSERT(cvt.len>=0);
        ASSERT(cvt.len_mult>=0);
        audio_buf.resize((size_t)(cvt.len*cvt.len_mult));
        cvt.buf=(uint8_t *)audio_buf.data();

        //if(!m_writer->BeginWrite()) {
        //    goto done;
        //}
    }

    {
        std::unique_ptr<SDL_PixelFormat,SDL_Deleter> pixel_format(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888));

        if(!tv_output.InitTexture(pixel_format.get())) {
            this->Error("couldn't initialise TV output texture");
            goto done;
        }
    }

    start_state=m_event_list.state_event.message->GetBeebState();

    event_lists.push_back(std::move(m_event_list));

    beeb_thread=std::make_shared<BeebThread>(m_msg.GetMessageList(),
                                             0,
                                             afmt.freq,
                                             NUM_SAMPLES,
                                             BeebLoadedConfig(),
                                             std::move(event_lists));

    if(!beeb_thread->Start()) {
        this->Error("couldn't start BBC thread");
        goto done;
    }

    {
        bool replaying=true;
        bool was_vblank=tv_output.IsInVerticalBlank();
        OutputDataBuffer<VideoDataUnit> *video_output=beeb_thread->GetVideoOutput();

        beeb_thread->Send(std::make_shared<BeebThread::StartReplayMessage>(start_state));
        //beeb_thread->Send(std::make_shared<BeebThread::PauseMessage>(false));

        for(;;) {
            uint64_t cycles=beeb_thread->GetEmulated2MHzCycles();

            // TODO - this isn't the right finish condition. Should just keep
            // going until out of events.
            if(cycles>=finish_cycles) {
                beeb_thread->Stop();
                replaying=false;
            } else {
                m_cycles_done.store(cycles-start_cycles,std::memory_order_release);
            }

            m_ticks.store(GetCurrentTickCount()-start_ticks,std::memory_order_release);

            size_t num_samples;
            {
                BeebThread *tmp=beeb_thread.get();
                float *dest=(float *)audio_buf.data();
                num_samples=tmp->AudioThreadFillAudioBuffer(dest,NUM_SAMPLES,true,
#if WAV
                                                            &PushBackFloat,&wav_full_float_data
#else
                                                            nullptr,nullptr
#endif
                );
            }

            ASSERT(num_samples==0||num_samples==NUM_SAMPLES);
            if(num_samples==NUM_SAMPLES) {
#if WAV
                wav_float_data.insert(wav_float_data.end(),cvt.buf,cvt.buf+cvt.len);
#endif
                size_t num_bytes;
                if(cvt.needed) {
                    SDL_ConvertAudio(&cvt);
                    ASSERT(cvt.len_cvt>=0);
                    num_bytes=(size_t)cvt.len_cvt;
                } else {
                    ASSERT(cvt.len>=0);
                    num_bytes=(size_t)cvt.len;
                }

                if(!m_writer->WriteSound(cvt.buf,num_bytes)) {
                    goto done;
                }

#if WAV
                wav_pcm_data.insert(wav_pcm_data.end(),cvt.buf,cvt.buf+num_bytes);
#endif
            }

            const VideoDataUnit *vp[2];
            size_t vn[2];
            if(video_output->GetConsumerBuffers(&vp[0],&vn[0],&vp[1],&vn[1])) {
                for(size_t i=0;i<2;++i) {
                    const VideoDataUnit *v=vp[i];
                    size_t n=vn[i];

                    ASSERT((n&1)==0);

                    for(size_t j=0;j<n;++j) {
                        tv_output.Update(v++,1);

                        bool is_vblank=tv_output.IsInVerticalBlank();
                        if(is_vblank&&!was_vblank) {
                            const void *data=tv_output.GetTexturePixels(nullptr);

                            if(!m_writer->WriteVideo(data)) {
                                goto done;
                            }

                            if(this->WasCanceled()) {
                                goto done;
                            }
                        }

                        was_vblank=is_vblank;
                    }
                }

                video_output->Consume(vn[0]+vn[1]);
            } else {
                if(num_samples==0&&!replaying) {
                    break;
                }
            }
        }
    }

    if(!m_writer->EndWrite()) {
        goto done;
    }

    m_success=true;

done:

#if WAV
    SaveWAV(m_file_name,".pcm.wav",wav_pcm_data,afmt.format,afmt.channels,afmt.freq,1,&m_msg);//1 = WAVE_FORMAT_PCM
    SaveWAV(m_file_name,".float.wav",wav_float_data,AUDIO_F32SYS,afmt.channels,afmt.freq,3,&m_msg);//3 = WAVE_FORMAT_IEEE_FLOAT

    for(int i=-1;i<NUM_CHANNELS;++i) {
        std::string ext=strprintf(".float.ch%d.wav",i);
        SaveWAV(m_file_name,ext.c_str(),wav_full_float_data[1+i],AUDIO_F32SYS,afmt.channels,SOUND_CLOCK_HZ,3,&m_msg);//3 = WAVE_FORMAT_IEEE_FLOAT   
    }
#endif

    m_writer=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
