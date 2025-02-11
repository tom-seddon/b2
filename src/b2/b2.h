#ifndef HEADER_6CE0510D80AC4A17BD606A68EAA242EC // -*- mode:c++ -*-
#define HEADER_6CE0510D80AC4A17BD606A68EAA242EC

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebWindowInitArguments;
class Messages;

#include <functional>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Create new window with the given init arguments.
void PushNewWindowMessage(BeebWindowInitArguments init_arguments);

// Call the given function next time round the loop.
void PushFunctionMessage(std::function<void()> fun);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Global settings, so the load/save stuff can find them.
//
// Some better mechanism to follow, perhaps...

extern bool g_option_vsync;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// there's no return value for this. Check the result of GetHTTPServerListenPort
// for as much information as there is to have.
void StartHTTPServer(Messages *messages);

void StopHTTPServer();

// Returns 0 if server not running.
int GetHTTPServerListenPort();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct DisplayData {
    uint32_t display_id = 0;
    uint64_t num_thread_vblanks = 0;
    uint64_t num_messages_sent = 0;
};

std::vector<DisplayData> GetDisplaysData();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS

bool CanDetachFromWindowsConsole();
bool HasWindowsConsole();
void AllocWindowsConsole();
void FreeWindowsConsole();

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
