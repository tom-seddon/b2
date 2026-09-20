#ifndef HEADER_9F375919D1B34DCBA8601EF851D04B47 // -*- mode:c++ -*-
#define HEADER_9F375919D1B34DCBA8601EF851D04B47

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class BeebState;
struct SDL_Renderer;

#include <memory>
#include <map>
#include <vector>
#include "dear_imgui.h"
#include "misc.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class ThumbnailsUI {
  public:
    explicit ThumbnailsUI(ImGuiStuff *imgui_stuff);
    ~ThumbnailsUI();

    ThumbnailsUI(const ThumbnailsUI &) = delete;
    ThumbnailsUI &operator=(const ThumbnailsUI &) = delete;
    ThumbnailsUI(ThumbnailsUI &&) = delete;
    ThumbnailsUI &operator=(ThumbnailsUI &&) = delete;

    ImVec2 GetThumbnailSize() const;
    size_t GetNumThumbnails() const;
    size_t GetNumTextures() const;

    void Thumbnail(const std::shared_ptr<const BeebState> &beeb_state);

    void Update();

  protected:
  private:
    struct Thumbnail;

    ImGuiStuff *m_imgui_stuff = nullptr;

    std::map<std::shared_ptr<const BeebState>, struct Thumbnail> m_thumbnails;
    std::vector<ImGuiTexture> m_textures;

    ImGuiTexture GetTexture();
    void ReturnTexture(ImGuiTexture texture);
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#endif
