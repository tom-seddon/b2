#ifndef HEADER_73C614F48AE84A78936CD1D7AE2D1876 // -*- mode:c++ -*-
#define HEADER_73C614F48AE84A78936CD1D7AE2D1876

#include "conf.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Now a misnomer...
#include <memory>
#include <beeb/type.h>

class BeebWindow;
class SettingsUI;

// If the relevant features are compiled out, the Create... function will return
// nullptr.

std::unique_ptr<SettingsUI> CreateSystemDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateHost6502DebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateParasite6502DebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateHostMemoryDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateParasiteMemoryDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateExtMemoryDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateHostDisassemblyDebugWindow(BeebWindow *beeb_window, bool initial_track_pc);
std::unique_ptr<SettingsUI> CreateParasiteDisassemblyDebugWindow(BeebWindow *beeb_window, bool initial_track_pc);
std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateVideoULADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateSystemVIADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateUserVIADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateSN76489DebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreatePagingDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateBreakpointsDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreatePixelMetadataDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateHostStackDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateParasiteStackDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateTubeDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateADCDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateDigitalJoystickDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateKeyboardDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateMouseDebugWindow(BeebWindow *beeb_window);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
