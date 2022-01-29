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
                                int num_frames,
                                const SDL_PixelFormat *pixel_format) {
    return this->Init(&beeb, nullptr, num_frames, pixel_format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::shared_ptr<const BeebState> beeb_state,
                                int num_frames,
                                const SDL_PixelFormat *pixel_format) {
    return this->Init(nullptr, &beeb_state, num_frames, pixel_format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GenerateThumbnailJob::ThreadExecute() {
    if (!m_beeb) {
        m_beeb = m_beeb_state->CloneBBCMicro();
    }

    // Discard the first frame - it'll probably be junk.
    BBCMicro *beeb = m_beeb.get();

    SoundDataUnit sunit;
    VideoDataUnit vunits[2];

    for (int i = 0; i < m_num_frames; ++i) {
        while (m_tv_output.IsInVerticalBlank()) {
            beeb->Update(&vunits[0], &sunit);
            beeb->Update(&vunits[1], &sunit);

            m_tv_output.Update(vunits, 2);
        }

        while (!m_tv_output.IsInVerticalBlank()) {
            beeb->Update(&vunits[0], &sunit);
            beeb->Update(&vunits[1], &sunit);

            m_tv_output.Update(vunits, 2);
        }
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
                                int num_frames,
                                const SDL_PixelFormat *pixel_format) {
    ASSERT(!!beeb != !!beeb_state);

    m_tv_output.Init(pixel_format->Rshift, pixel_format->Gshift, pixel_format->Bshift);

    if (beeb) {
        m_beeb = std::move(*beeb);
    } else {
        m_beeb_state = std::move(*beeb_state);
    }

    m_num_frames = num_frames;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
