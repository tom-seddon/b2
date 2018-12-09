#include <shared/system.h>
#include "SavedStatesUI.h"
#include "dear_imgui.h"
#include "SettingsUI.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class SavedStatesUI:
public SettingsUI
{
public:
    explicit SavedStatesUI(SDL_Renderer *renderer,
                           const SDL_PixelFormat *pixel_format):
    m_renderer(renderer),
    m_pixel_format(pixel_format)
    {
    }

    void DoImGui(CommandContextStack *cc_stack) override {

    }

    bool OnClose() override {
        return false;
    }
protected:
private:
    SDL_Renderer *m_renderer;
    const SDL_PixelFormat *m_pixel_format;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateSavedStatesUI(SDL_Renderer *renderer,
                                                const SDL_PixelFormat *pixel_format)
{
    return std::make_unique<SavedStatesUI>(renderer,pixel_format);
}
