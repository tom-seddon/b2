#ifndef HEADER_73C614F48AE84A78936CD1D7AE2D1876// -*- mode:c++ -*-
#define HEADER_73C614F48AE84A78936CD1D7AE2D1876

#include "conf.h"

#if BBCMICRO_DEBUGGER

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Now a misnomer...
#include "SettingsUI.h"
#include <memory>

class BeebWindow;

std::unique_ptr<SettingsUI> Create6502DebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateMemoryDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateExtMemoryDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateDisassemblyDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateVideoULADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateSystemVIADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateUserVIADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(BeebWindow *beeb_window);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
