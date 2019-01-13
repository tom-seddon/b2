#ifndef HEADER_CEB00A5BE87C4598882E08BD09DE979B// -*- mode:c++ -*-
#define HEADER_CEB00A5BE87C4598882E08BD09DE979B

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Some overlap between this and BeebThread...

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebState;
class DiscImage;
class BBCMicro;
struct SDL_PixelFormat;

#include <beeb/conf.h>
#include <memory>
#include "JobQueue.h"
#include "TVOutput.h"
#include "BeebConfig.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class GenerateThumbnailJob:
    public JobQueue::Job
{
public:
    //GenerateThumbnailJob()=default;

    bool Init(std::unique_ptr<BBCMicro> beeb,
              int num_frames,
              const SDL_PixelFormat *pixel_format);

    bool Init(std::shared_ptr<const BeebState> beeb_state,
              int num_frames,
              const SDL_PixelFormat *pixel_format);
    
    void ThreadExecute() override;

    const void *GetTexturePixels() const;
private:
    std::shared_ptr<const BeebState> m_beeb_state;
    std::unique_ptr<BBCMicro> m_beeb;
    TVOutput m_tv_output;
    int m_num_frames=2;
    
    bool Init(std::unique_ptr<BBCMicro> *beeb,
              std::shared_ptr<const BeebState> *beeb_state,
              int num_frames,
              const SDL_PixelFormat *pixel_format);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
