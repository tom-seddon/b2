#include <shared/system.h>
#include "GenerateThumbnailJob.h"
#include <beeb/BBCMicro.h>
#include "BeebState.h"
#include <beeb/DiscImage.h>
#include "conf.h"
#include <beeb/video.h>
#include <beeb/sound.h>
#include <shared/debug.h>
#include "beeb_events.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::unique_ptr<BBCMicro> beeb,
                                int num_frames,
                                const SDL_PixelFormat *pixel_format)
{
    return this->Init(&beeb,nullptr,num_frames,pixel_format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::shared_ptr<const BeebState> beeb_state,
                                int num_frames,
                                const SDL_PixelFormat *pixel_format)
{
    return this->Init(nullptr,&beeb_state,num_frames,pixel_format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GenerateThumbnailJob::ThreadExecute() {
    if(!m_beeb) {
        m_beeb=m_beeb_state->CloneBBCMicro();
    }

    // Discard the first frame - it'll probably be junk.
    BBCMicro *beeb=m_beeb.get();

    SoundDataUnit sunit;
    VideoDataUnit vunits[2];

    for(int i=0;i<m_num_frames;++i) {
        while(m_tv_output.IsInVerticalBlank()) {
            beeb->Update(&vunits[0],&sunit);
            beeb->Update(&vunits[1],&sunit);

            m_tv_output.UpdateOneUnit(&vunits[0],1.f);
            m_tv_output.UpdateOneUnit(&vunits[1],1.f);
        }

        while(!m_tv_output.IsInVerticalBlank()) {
            beeb->Update(&vunits[0],&sunit);
            beeb->Update(&vunits[1],&sunit);

            m_tv_output.UpdateOneUnit(&vunits[0],1.f);
            m_tv_output.UpdateOneUnit(&vunits[1],1.f);
        }
    }

    // Don't bother keeping these around any longer than necessary...
    m_beeb.reset();
    m_beeb_state.reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const void *GenerateThumbnailJob::GetTextureData() const {
    return m_tv_output.GetTextureData(nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::unique_ptr<BBCMicro> *beeb,
                                std::shared_ptr<const BeebState> *beeb_state,
                                int num_frames,
                                const SDL_PixelFormat *pixel_format)
{
    ASSERT(!!beeb!=!!beeb_state);

    if(!m_tv_output.InitTexture(pixel_format)) {
        return false;
    }

    if(beeb) {
        m_beeb=std::move(*beeb);
    } else {
        m_beeb_state=std::move(*beeb_state);
    }

    m_num_frames=num_frames;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
