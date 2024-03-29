#ifndef HEADER_06DEB6C4943744F6A8B6E6E0A65972E0 // -*- mode:c++ -*-
#define HEADER_06DEB6C4943744F6A8B6E6E0A65972E0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>

class BeebWindow;
class SettingsUI;
struct SDL_Renderer;

std::unique_ptr<SettingsUI> CreateSavedStatesUI(BeebWindow *beeb_window, SDL_Renderer *renderer);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
