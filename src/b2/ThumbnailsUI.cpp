#include <shared/system.h>
#include "ThumbnailsUI.h"
#include "GenerateThumbnailJob.h"
#include <SDL.h>
#include "BeebWindows.h"
#include <beeb/BBCMicro.h>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "ThumbnailsUI_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "ThumbnailsUI_private.inl"
#include <shared/enum_end.h>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

struct ThumbnailsUI::Thumbnail {
    ThumbnailState state=ThumbnailState_Start;
    std::shared_ptr<GenerateThumbnailJob> job;
    bool in_use=false;
    SDLUniquePtr<SDL_Texture> texture;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ThumbnailsUI::ThumbnailsUI(SDL_Renderer *renderer,
                           const SDL_PixelFormat *pixel_format):
m_renderer(renderer),
m_pixel_format(pixel_format)
{
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ThumbnailsUI::~ThumbnailsUI() {
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ImVec2 ThumbnailsUI::GetThumbnailSize() const {
    return ImVec2(TV_TEXTURE_WIDTH/3.0,TV_TEXTURE_HEIGHT/3.0);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

size_t ThumbnailsUI::GetNumThumbnails() const {
    return m_thumbnails.size();
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

size_t ThumbnailsUI::GetNumTextures() const {
    return m_textures.size();
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ThumbnailsUI::Thumbnail(const std::shared_ptr<const BeebState> &beeb_state) {
    struct Thumbnail *t=&m_thumbnails[beeb_state];

    t->in_use=true;

    switch(t->state) {
        case ThumbnailState_Start:
            ASSERT(!t->job);
            t->job=std::make_shared<GenerateThumbnailJob>();
            if(!t->job->Init(beeb_state,NUM_THUMBNAIL_RENDER_FRAMES,m_pixel_format)) {
                t->state=ThumbnailState_Error;
                break;
            }

            BeebWindows::AddJob(t->job);
            t->state=ThumbnailState_WaitForJob;
            break;

        case ThumbnailState_WaitForJob:
        {
            if(!t->job->IsFinished()) {
                break;
            }

            std::shared_ptr<GenerateThumbnailJob> job=std::move(t->job);

            if(job->WasCanceled()) {
                t->state=ThumbnailState_Error;
                break;
            }

            const void *texture_data=job->GetTextureData();
            if(!texture_data) {
                t->state=ThumbnailState_Error;
                break;
            }

            SDLUniquePtr<SDL_Texture> texture=this->GetTexture();
            if(!texture) {
                t->state=ThumbnailState_Error;
                break;
            }

            if(SDL_UpdateTexture(texture.get(),nullptr,texture_data,TV_TEXTURE_WIDTH*4)<0) {
                t->state=ThumbnailState_Error;
                break;
            }

            t->texture=std::move(texture);
            t->state=ThumbnailState_Ready;
        }
            break;

        case ThumbnailState_Ready:
            break;

        case ThumbnailState_Error:
            break;
    }

    switch(t->state) {
        case ThumbnailState_Start:
        case ThumbnailState_WaitForJob:
            ImGui::TextUnformatted("...");
            break;

        case ThumbnailState_Ready:
            ImGui::Image(t->texture.get(),this->GetThumbnailSize());
            break;

        case ThumbnailState_Error:
            ImGui::TextUnformatted(":(");
            break;
    }
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ThumbnailsUI::Update() {
    auto &&it=m_thumbnails.begin();
    while(it!=m_thumbnails.end()) {
        auto next_it=it;
        ++next_it;

        struct Thumbnail *t=&it->second;

        if(t->in_use) {
            t->in_use=false;
        } else {
            if(!!t->job) {
                t->job->Cancel();
            }

            if(!!t->texture) {
                this->ReturnTexture(std::move(t->texture));
            }

            m_thumbnails.erase(it);
        }

        it=next_it;
    }
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

SDLUniquePtr<SDL_Texture> ThumbnailsUI::GetTexture() {
    SDLUniquePtr<SDL_Texture> texture;

    if(m_textures.empty()) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        texture=SDLUniquePtr<SDL_Texture>(SDL_CreateTexture(m_renderer,
                                                            m_pixel_format->format,
                                                            SDL_TEXTUREACCESS_STATIC,
                                                            TV_TEXTURE_WIDTH,
                                                            TV_TEXTURE_HEIGHT));
        if(!texture) {
            return nullptr;
        }
    } else {
        texture=std::move(m_textures.back());
        m_textures.pop_back();
    }

    return texture;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ThumbnailsUI::ReturnTexture(SDLUniquePtr<SDL_Texture> texture) {
    m_textures.push_back(std::move(texture));
}
