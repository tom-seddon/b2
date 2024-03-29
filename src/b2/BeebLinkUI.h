#ifndef HEADER_05C5BF1E6A7B4B1FA480491A3B9A43BE // -*- mode:c++ -*-
#define HEADER_05C5BF1E6A7B4B1FA480491A3B9A43BE

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>

class SettingsUI;
class BeebWindow;

std::unique_ptr<SettingsUI> CreateBeebLinkUI(BeebWindow *beeb_window);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
