#include <shared/system.h>
#include "GenerateThumbnailJob.h"
#include <beeb/BBCMicro.h>
#include "BeebState.h"
#include <beeb/DiscImage.h>
#include "conf.h"
#include <beeb/video.h>
#include <beeb/sound.h>
#include <shared/debug.h>
#include <SDL.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::unique_ptr<BBCMicro> beeb,
                                int num_frames) {
    return this->Init(&beeb, nullptr, num_frames);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::shared_ptr<const BeebState> beeb_state,
                                int num_frames) {
    return this->Init(nullptr, &beeb_state, num_frames);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GenerateThumbnailJob::ThreadExecute() {
    if (!m_beeb) {
        m_beeb = std::make_unique<BBCMicro>(*m_beeb_state);
    }

    // Discard the first frame - it'll probably be junk.
    BBCMicro *beeb = m_beeb.get();

    SoundDataUnit sunit;
    static constexpr size_t MAX_NUM_VUNITS = 1000;
    VideoDataUnit vunits[MAX_NUM_VUNITS];
    VideoDataUnit *const vunits_end = vunits + MAX_NUM_VUNITS;
    VideoDataUnit *vunit = vunits;

    for (int i = 0; i < m_num_frames; ++i) {
        while (m_tv_output.IsInVerticalBlank()) {
            uint32_t update_result = beeb->Update(vunit, &sunit);
            if (update_result & BBCMicroUpdateResultFlag_VideoUnit) {
                ++vunit;
                if (vunit == vunits_end) {
                    m_tv_output.Update(vunits, (size_t)(vunit - vunits));
                    vunit = vunits;
                }
            }
        }

        while (!m_tv_output.IsInVerticalBlank()) {
            uint32_t update_result = beeb->Update(vunit, &sunit);
            if (update_result & BBCMicroUpdateResultFlag_VideoUnit) {
                ++vunit;
                if (vunit == vunits_end) {
                    m_tv_output.Update(vunits, (size_t)(vunit - vunits));
                    vunit = vunits;
                }
            }
        }
    }

    if (vunit > vunits) {
        m_tv_output.Update(vunits, (size_t)(vunit - vunits));
    }

    // Don't bother keeping these around any longer than necessary...
    m_beeb.reset();
    m_beeb_state.reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const void *GenerateThumbnailJob::GetTexturePixels() const {
    return m_tv_output.GetTexturePixels(nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::unique_ptr<BBCMicro> *beeb,
                                std::shared_ptr<const BeebState> *beeb_state,
                                int num_frames) {
    ASSERT(!!beeb != !!beeb_state);

    if (beeb) {
        m_beeb = std::move(*beeb);
    } else {
        m_beeb_state = std::move(*beeb_state);
    }

    m_num_frames = num_frames;

    return true;
}
