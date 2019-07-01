#ifndef HEADER_73C614F48AE84A78936CD1D7AE2D1876// -*- mode:c++ -*-
#define HEADER_73C614F48AE84A78936CD1D7AE2D1876

#include "conf.h"

#if BBCMICRO_DEBUGGER

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Now a misnomer...
#include "SettingsUI.h"
#include <memory>
#include <beeb/type.h>

class BeebWindow;

class MemoryViewUI {
public:
    virtual void SetAddress(uint16_t addr,uint32_t dpo)=0;
protected:
private:
};

std::unique_ptr<SettingsUI> Create6502DebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateMemoryDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateExtMemoryDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateDisassemblyDebugWindow(BeebWindow *beeb_window,bool initial_track_pc);
std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateVideoULADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateSystemVIADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateUserVIADebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateSN76489DebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreatePagingDebugWindow(BeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateBreakpointsDebugWindow(BeebWindow *beeb_window);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
