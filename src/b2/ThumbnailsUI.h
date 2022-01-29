#ifndef HEADER_9F375919D1B34DCBA8601EF851D04B47 // -*- mode:c++ -*-
#define HEADER_9F375919D1B34DCBA8601EF851D04B47

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class BeebState;
struct SDL_Renderer;
struct SDL_PixelFormat;

#include <memory>
#include <map>
#include <vector>
#include "dear_imgui.h"
#include "misc.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class ThumbnailsUI {
  public:
    ThumbnailsUI(SDL_Renderer *renderer, const SDL_PixelFormat *pixel_format);
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

    SDL_Renderer *m_renderer;
    const SDL_PixelFormat *m_pixel_format;

    std::map<std::shared_ptr<const BeebState>, struct Thumbnail> m_thumbnails;
    std::vector<SDLUniquePtr<SDL_Texture>> m_textures;

    SDLUniquePtr<SDL_Texture> GetTexture();
    void ReturnTexture(SDLUniquePtr<SDL_Texture> texture);
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#endif
