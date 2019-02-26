#ifndef HEADER_A25A26E02BAC4AB39772A688E1F32816// -*- mode:c++ -*-
#define HEADER_A25A26E02BAC4AB39772A688E1F32816

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>
//
class SettingsUI;
class BeebWindow;
//struct BeebWindowInitArguments;
struct SDL_Renderer;
struct SDL_PixelFormat;
//
//// beeb_window is the window this timeline UI is going to be displayed
//// in. (This is used to disable buttons that aren't appropriate for
//// the timeline entry corresponding to the current window.)
//std::unique_ptr<SettingsUI> CreateTimelineUI(BeebWindow *beeb_window,BeebWindowInitArguments init_arguments,SDL_Renderer *renderer,const SDL_PixelFormat *pixel_format);

std::unique_ptr<SettingsUI> CreateTimelineUI(BeebWindow *beeb_window,
                                             SDL_Renderer *renderer,
                                             const SDL_PixelFormat *pixel_format);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
