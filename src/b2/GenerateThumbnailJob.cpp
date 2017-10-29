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

GenerateThumbnailJob::~GenerateThumbnailJob() {
    delete m_beeb;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GenerateThumbnailJob::Init(std::unique_ptr<BBCMicro> beeb,int num_frames,const SDL_PixelFormat *pixel_format) {
    if(!m_tv_output.InitTexture(pixel_format)) {
        return false;
    }

    ASSERT(!m_beeb);
    m_beeb=beeb.release();
    if(!m_beeb) {
        return false;
    }

    m_num_frames=num_frames;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GenerateThumbnailJob::ThreadExecute() {
    // Discard the first frame - it'll probably be junk.
    
    for(int i=0;i<m_num_frames;++i) {
        while(m_tv_output.IsInVerticalBlank()) {
            this->Tick();
        }

        while(!m_tv_output.IsInVerticalBlank()) {
            this->Tick();
        }
    }

    delete m_beeb;
    m_beeb=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const void *GenerateThumbnailJob::GetTextureData() const {
    return m_tv_output.GetTextureData(nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GenerateThumbnailJob::Tick() {
    SoundDataUnit sunit;
    VideoDataUnit vunits[2];

    m_beeb->UpdateCycle0(&vunits[0]);
    m_beeb->UpdateCycle1(&vunits[1],&sunit);

    m_tv_output.UpdateOneUnit(&vunits[0],1.f);
    m_tv_output.UpdateOneUnit(&vunits[1],1.f);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
