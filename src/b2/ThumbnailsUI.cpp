#include <shared/system.h>
#include "ThumbnailsUI.h"
#include "GenerateThumbnailJob.h"
#include <SDL.h>
#include "BeebWindows.h"
#include <beeb/BBCMicro.h>
#include "conf.h"

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
    ThumbnailState state = ThumbnailState_Start;
    std::shared_ptr<GenerateThumbnailJob> job;
    bool in_use = false;
    ImGuiTexture imgui_texture;
    std::string error;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ThumbnailsUI::ThumbnailsUI(ImGuiStuff *imgui_stuff)
    : m_imgui_stuff(imgui_stuff) {
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ThumbnailsUI::~ThumbnailsUI() {
    for (ImGuiTexture texture : m_textures) {
        m_imgui_stuff->DestroyTexture(texture);
    }
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ImVec2 ThumbnailsUI::GetThumbnailSize() const {
    return ImVec2(TV_TEXTURE_WIDTH / 3.f, TV_TEXTURE_HEIGHT / 3.f);
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
    struct Thumbnail *t = &m_thumbnails[beeb_state];

    t->in_use = true;

    switch (t->state) {
    case ThumbnailState_Start:
        ASSERT(!t->job);
        t->job = std::make_shared<GenerateThumbnailJob>();
        if (!t->job->Init(beeb_state, NUM_THUMBNAIL_RENDER_FRAMES)) {
            t->state = ThumbnailState_Error;
            break;
        }

        BeebWindows::AddJob(t->job);
        t->state = ThumbnailState_WaitForJob;
        break;

    case ThumbnailState_WaitForJob:
        {
            if (!t->job->IsFinished()) {
                break;
            }

            std::shared_ptr<GenerateThumbnailJob> job = std::move(t->job);

            if (job->WasCanceled()) {
                t->state = ThumbnailState_Error;
                t->error = "Canceled";
                break;
            }

            const void *texture_data = job->GetTexturePixels();
            ASSERT(texture_data);

            ImGuiTexture imgui_texture = this->GetTexture();
            SDL_Texture *sdl_texture = m_imgui_stuff->GetSDLTexture(imgui_texture);
            if (!sdl_texture) {
                t->state = ThumbnailState_Error;
                t->error = std::string("Failed to create texture: ") + SDL_GetError();
                break;
            }

            if (SDL_UpdateTexture(sdl_texture, nullptr, texture_data, TV_TEXTURE_WIDTH * 4) < 0) {
                t->state = ThumbnailState_Error;
                t->error = std::string("Failed to initialise texture: ") + SDL_GetError();
                break;
            }

            t->imgui_texture = imgui_texture;
            t->state = ThumbnailState_Ready;
        }
        break;

    case ThumbnailState_Ready:
        break;

    case ThumbnailState_Error:
        break;
    }

    switch (t->state) {
    case ThumbnailState_Start:
    case ThumbnailState_WaitForJob:
        ImGui::TextUnformatted("Creating preview...");
        break;

    case ThumbnailState_Ready:
        {
            static const char THUMBNAIL_POPUP[] = "thumbnail_popup";

            ImGuiIDPusher pusher(t);

            ImTextureID texture_id = m_imgui_stuff->GetImTextureID(t->imgui_texture);
            ImGui::Image(texture_id, this->GetThumbnailSize());

            if (ImGui::IsItemClicked()) {
                ImGui::OpenPopup(THUMBNAIL_POPUP);
            }

            if (ImGui::BeginPopup(THUMBNAIL_POPUP)) {
                ImGui::Image(texture_id, ImVec2((float)TV_TEXTURE_WIDTH, (float)TV_TEXTURE_HEIGHT));
                ImGui::EndPopup();
            }
        }
        break;

    case ThumbnailState_Error:
        ImGui::TextUnformatted(t->error.c_str());
        break;
    }
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ThumbnailsUI::Update() {
    auto &&it = m_thumbnails.begin();
    while (it != m_thumbnails.end()) {
        auto next_it = it;
        ++next_it;

        struct Thumbnail *t = &it->second;

        if (t->in_use) {
            t->in_use = false;
        } else {
            if (!!t->job) {
                t->job->Cancel();
            }

            if (t->imgui_texture.value != 0) {
                this->ReturnTexture(t->imgui_texture);
            }

            m_thumbnails.erase(it);
        }

        it = next_it;
    }
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ImGuiTexture ThumbnailsUI::GetTexture() {
    ImGuiTexture texture;

    if (m_textures.empty()) {
        SetRenderScaleQualityHint(true);

        m_imgui_stuff->CreateTexture(&texture,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STATIC,
                                     TV_TEXTURE_WIDTH,
                                     TV_TEXTURE_HEIGHT);
    } else {
        texture = m_textures.back();
        m_textures.pop_back();
    }

    return texture;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ThumbnailsUI::ReturnTexture(ImGuiTexture texture) {
    m_textures.push_back(texture);
}
