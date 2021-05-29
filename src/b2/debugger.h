#ifndef HEADER_73C614F48AE84A78936CD1D7AE2D1876// -*- mode:c++ -*-
#define HEADER_73C614F48AE84A78936CD1D7AE2D1876

#include "conf.h"

#if BBCMICRO_DEBUGGER

#define ENABLE_6502_DEBUG_WINDOW 1
#define ENABLE_MEMORY_DEBUG_WINDOW 1
#define ENABLE_EXT_MEMORY_DEBUG_WINDOW 1
#define ENABLE_DISASSEMBLY_DEBUG_WINDOW 1
#define ENABLE_CRTC_DEBUG_WINDOW 1
#define ENABLE_VIDEO_ULA_DEBUG_WINDOW 1
#define ENABLE_VIA_DEBUG_WINDOW 1
#define ENABLE_NVRAM_DEBUG_WINDOW 1
#define ENABLE_SN76489_DEBUG_WINDOW 1
#define ENABLE_PAGING_DEBUG_WINDOW 1
#define ENABLE_BREAKPOINTS_DEBUG_WINDOW 1
#define ENABLE_PIXEL_METADATA_DEBUG_WINDOW 1
#define ENABLE_STACK_DEBUG_WINDOW 1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Now a misnomer...
#include "SettingsUI.h"
#include <memory>
#include <beeb/type.h>

class SDLBeebWindow;

#if ENABLE_6502_DEBUG_WINDOW
std::unique_ptr<SettingsUI> Create6502DebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_MEMORY_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateMemoryDebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_EXT_MEMORY_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateExtMemoryDebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_DISASSEMBLY_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateDisassemblyDebugWindow(SDLBeebWindow *beeb_window,bool initial_track_pc);
#endif

#if ENABLE_CRTC_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_VIDEO_ULA_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateVideoULADebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_VIA_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateSystemVIADebugWindow(SDLBeebWindow *beeb_window);
std::unique_ptr<SettingsUI> CreateUserVIADebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_NVRAM_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_SN76489_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateSN76489DebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_PAGING_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreatePagingDebugWindow(SDLBeebWindow *beeb_window);
#endif

#if ENABLE_BREAKPOINTS_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateBreakpointsDebugWindow(SDLBeebWindow *beeb_window);
#endif

#if VIDEO_TRACK_METADATA
#if ENABLE_PIXEL_METADATA_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreatePixelMetadataDebugWindow(SDLBeebWindow *beeb_window);
#endif
#endif

#if ENABLE_STACK_DEBUG_WINDOW
std::unique_ptr<SettingsUI> CreateStackDebugWindow(SDLBeebWindow *beeb_window);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
