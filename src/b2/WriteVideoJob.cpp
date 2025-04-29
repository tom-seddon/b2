#include <shared/system.h>
#include "WriteVideoJob.h"
#include <beeb/TVOutput.h>
#include "conf.h"
#include <SDL.h>
#include <beeb/OutputData.h>
#include "VideoWriter.h"
#include "BeebState.h"
#include <beeb/video.h>
#include <shared/debug.h>
#include "dear_imgui.h"
#include "BeebThread.h"
#include <shared/load_store.h>
#include "load_save.h"
#include <shared/path.h>
#include <beeb/sound.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WriteVideoJob::WriteVideoJob(BeebThread::TimelineEventList event_list,
                             std::unique_ptr<VideoWriter> writer)
    : m_event_list(std::move(event_list))
    , m_writer(std::move(writer))
    , m_msg(m_writer->GetMessageList()) {
    m_file_name = m_writer->GetFileName();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WriteVideoJob::~WriteVideoJob() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool WriteVideoJob::Error(const char *fmt, ...) {
    m_msg.e.f("failed to write video to: %s\n", m_file_name.c_str());

    m_msg.i.f("(");

    {
        va_list v;
        va_start(v, fmt);
        m_msg.i.v(fmt, v);
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

    CycleCount cycles_done = m_cycles_done.load(std::memory_order_acquire);

    double real_seconds = GetSecondsFromTicks(m_ticks.load(std::memory_order_acquire));
    double emu_seconds = cycles_done.n / (double)CYCLES_PER_SECOND;

    char label[50];
    if (real_seconds == 0.) {
        label[0] = 0;
    } else {
        snprintf(label, sizeof label, "%.3fx", emu_seconds / real_seconds);
    }

    float percentage = (float)cycles_done.n / m_cycles_total.load(std::memory_order_acquire).n;
    ImGui::ProgressBar(percentage, ImVec2(-1, 0), label);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WriteVideoJob::ThreadExecute() {
    //OutputData *video_output_data=m_beeb_thread->GetVideoOutputData();

    ASSERT(!m_event_list.events.empty());
    CycleCount start_cycles = m_event_list.state_event.time_cycles;
    CycleCount finish_cycles = m_event_list.events.back().time_cycles;
    ASSERT(finish_cycles.n >= start_cycles.n);

    uint64_t start_ticks = GetCurrentTickCount();

    m_cycles_done.store({0}, std::memory_order_release);
    m_ticks.store(0, std::memory_order_release);
    m_cycles_total.store({finish_cycles.n - start_cycles.n}, std::memory_order_release);

    SDL_AudioSpec afmt;
    SDL_AudioCVT cvt;
    std::vector<char> audio_buf;
    static const size_t NUM_SAMPLES = 4096;
    TVOutput tv_output;
    std::shared_ptr<BeebThread> beeb_thread;
    std::shared_ptr<const BeebState> start_state;
    std::vector<BeebThread::TimelineEventList> event_lists;

    if (!m_writer->BeginWrite()) {
        goto done;
    }

    bool low_pass_filter;
    int vwidth, vheight;
    {
        if (!m_writer->GetAudioFormat(&afmt)) {
            this->Error("couldn't get audio output format");
            goto done;
        }

        if (SDL_BuildAudioCVT(&cvt, AUDIO_FORMAT, AUDIO_NUM_CHANNELS, afmt.freq, afmt.format, afmt.channels, afmt.freq) < 0) {
            this->Error("SDL_BuildAudioCVT failed: %s", SDL_GetError());
            goto done;
        }

        // When saving out full rate sound data, skip the low pass filter.
        // Presumably the output is destined for some video processing tool,
        // which can do a better job of it.
        low_pass_filter = true;
        if (afmt.freq == SOUND_CLOCK_HZ) {
            low_pass_filter = false;
        }

        uint32_t vformat;
        if (!m_writer->GetVideoFormat(&vformat, &vwidth, &vheight)) {
            this->Error("couldn't get video output format");
            goto done;
        }

        if (vformat != SDL_PIXELFORMAT_ARGB8888) {
            this->Error("video output format not ARGB8888: %s\n", SDL_GetPixelFormatName(vformat));
            goto done;
        }

        cvt.len = NUM_SAMPLES * sizeof(float);

        ASSERT(cvt.len >= 0);
        ASSERT(cvt.len_mult >= 0);
        audio_buf.resize((size_t)(cvt.len * cvt.len_mult));
        cvt.buf = (uint8_t *)audio_buf.data();

        //if(!m_writer->BeginWrite()) {
        //    goto done;
        //}
    }

    start_state = m_event_list.state_event.message->GetBeebState();

    event_lists.push_back(std::move(m_event_list));

    beeb_thread = std::make_shared<BeebThread>(m_msg.GetMessageList(),
                                               0,
                                               afmt.freq,
                                               NUM_SAMPLES,
                                               BeebLoadedConfig(),
                                               std::move(event_lists));

    beeb_thread->SetLowPassFilter(low_pass_filter);

    if (!beeb_thread->Start()) {
        this->Error("couldn't start BBC thread");
        goto done;
    }

    // It's probably not what you want - but this should really be
    // configurable.
    beeb_thread->SetDiscVolume(MIN_DB);

    {
        bool replaying = true;
        bool was_vblank = tv_output.IsInVerticalBlank();
        OutputDataBuffer<VideoDataUnit> *video_output = beeb_thread->GetVideoOutput();

        beeb_thread->Send(std::make_shared<BeebThread::StartReplayMessage>(start_state));
        //beeb_thread->Send(std::make_shared<BeebThread::PauseMessage>(false));

        for (;;) {
            CycleCount cycles = beeb_thread->GetEmulatedCycles();

            // TODO - this isn't the right finish condition. Should just keep
            // going until out of events.
            if (cycles.n >= finish_cycles.n) {
                beeb_thread->Stop();
                replaying = false;
            } else {
                m_cycles_done.store({cycles.n - start_cycles.n}, std::memory_order_release);
            }

            m_ticks.store(GetCurrentTickCount() - start_ticks, std::memory_order_release);

            size_t num_samples;
            {
                BeebThread *tmp = beeb_thread.get();
                float *dest = (float *)audio_buf.data();
                num_samples = tmp->AudioThreadFillAudioBuffer(dest, NUM_SAMPLES, true);
            }

            ASSERT(num_samples == 0 || num_samples == NUM_SAMPLES);
            if (num_samples == NUM_SAMPLES) {
                size_t num_bytes;
                if (cvt.needed) {
                    SDL_ConvertAudio(&cvt);
                    ASSERT(cvt.len_cvt >= 0);
                    num_bytes = (size_t)cvt.len_cvt;
                } else {
                    ASSERT(cvt.len >= 0);
                    num_bytes = (size_t)cvt.len;
                }

                if (!m_writer->WriteSound(cvt.buf, num_bytes)) {
                    goto done;
                }
            }

            const VideoDataUnit *vp[2];
            size_t vn[2];
            if (video_output->GetConsumerBuffers(&vp[0], &vn[0], &vp[1], &vn[1])) {
                for (size_t i = 0; i < 2; ++i) {
                    const VideoDataUnit *v = vp[i];
                    size_t n = vn[i];

                    ASSERT((n & 1) == 0);

                    for (size_t j = 0; j < n; ++j) {
                        tv_output.Update(v++, 1);

                        bool is_vblank = tv_output.IsInVerticalBlank();
                        if (is_vblank && !was_vblank) {
                            VideoDataUnitCount vsync_time;
                            const void *data = tv_output.GetTexturePixels(&vsync_time);
                            
                            if (!m_writer->WriteVideo(data,(int64_t)vsync_time.n*5)) {
                                goto done;
                            }

                            if (this->WasCanceled()) {
                                goto done;
                            }
                        }

                        was_vblank = is_vblank;
                    }
                }

                video_output->Consume(vn[0] + vn[1]);
            } else {
                if (num_samples == 0 && !replaying) {
                    break;
                }
            }
        }
    }

    if (!m_writer->EndWrite()) {
        goto done;
    }

    m_success = true;

done:

    m_writer = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
