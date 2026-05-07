#ifndef HEADER_6CE0510D80AC4A17BD606A68EAA242EC // -*- mode:c++ -*-
#define HEADER_6CE0510D80AC4A17BD606A68EAA242EC

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebWindowInitArguments;
class Messages;
#ifdef IMGUI_ENABLE_TEST_ENGINE
class BeebWindow;
class ImGuiStuff;
#endif
struct Guid;

#include <functional>
#include <vector>
#include "json.h"
#include "BeebConfig.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Printable product name, including version string.
extern const char DEFAULT_PRODUCT_NAME[];

// Name of the game controller database file.
extern const char GAMECONTROLLER_DB_FILE_NAME[];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Handle communication between b2 and the actual app it's embedded in.
//
// The objective is that the b2 app proper won't need to have much logic in its
// implementation, but the b2 test app might.
class AppHandler {
  public:
    AppHandler() = default;
    virtual ~AppHandler() = 0;

    AppHandler(const AppHandler &) = delete;
    AppHandler &operator=(const AppHandler &) = delete;
    AppHandler(AppHandler &&) = delete;
    AppHandler &operator=(AppHandler &&) = delete;

    // Get the product name.
    virtual std::string GetProductName() const = 0;

#if SYSTEM_OSX
    // Get the name to save the window placement data under - see -[NSWindow saveFrameUsingName:]
    //
    // If empty, opt out of the mechanism entirely.
    virtual std::string GetFrameName() const = 0;
#else
    // The window placement data is saved to the config file, so it participates in the same persistence mechanism as everything else.
#endif

    // Whether running headless or not. Must always return the same value for a given
    // run.
    virtual bool IsHeadless() const = 0;

    // Whether to allow high-DPI support.
    virtual bool IsHighDPIEnabled() const = 0;

    // If not running headless, whether to allow sound.
    virtual bool IsSoundEnabled() const = 0;

    // argc/argv access. Return value is the full argv, including argv[0].
    //
    // Anything in here is passed to the ordinary b2 app command line argument
    // handling, the idea being that the test code can ensure it's exercising
    // the b2 app command line processing.
    //
    // This is a bit inconvenient for b2_headless.
    virtual std::vector<std::string> GetCommandLineArgs() const = 0;

    // Folder for config and cache files. Return false if none (and b2 will use
    // defaults).
    //
    // (folder may be null, just to query whether an override was actually
    // specified.)
    virtual bool GetConfigAndCacheOverrideFolder(std::string *folder) const = 0;

    // Return HTTP server listen port. May be 0 to specify any, or <0 to
    // indicate that the HTTP server can't be started.
    virtual int GetRequestedHttpServerListenPort() const = 0;

    // Indicate actual HTTP server listen port chosen, or 0 if the HTTP server
    // didn't start.
    //
    // Default impl does nothing.
    virtual void SetActualHttpServerListenPort(int port);

    // Return port to use for HTTP launch requests.
    virtual int GetLaunchRequestHttpServerPort() const = 0;

    // Indicate BeebWindow was created and its Init function succeeded.
    //
    // Default impl does nothing.
    virtual void HandleBeebWindowPostInit(BeebWindow *beeb_window);

    // Indicate message loop is about to start.
    //
    // HandleBeebWindowPostInit was called for the first window, and the first
    // window is the MRU window.
    //
    // Default impl does nothing.
    virtual void MessageLoopWillStart();

#ifdef IMGUI_ENABLE_TEST_ENGINE
    // Whether to initialise Dear ImGui Test Engine.
    virtual bool IsDearImGuiTestEngineEnabled() const = 0;

    // Called when the Dear ImGui Test Engine is ready for use.
    //
    // Default impl does nothing.
    virtual void DearImGuiTestEngineDidBecomeReady(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff);

    // Whether to quit once the test queue becomes empty.
    virtual bool ShouldQuitWhenTestQueueEmpty() const = 0;
#endif

    // Indicate selector dialog has been opened.
    //
    // Return true to override dialog behaviour. Fill in *result with file selectod.
    //
    // Return false to pass through to default native UI handling.
    virtual bool HandleSelectorDialogOpen(std::string *result, const Guid &guid) = 0;

    // Indicate selector dialog result obtained.
    //
    // Default impl does nothing.
    virtual void SetSelectorDialogResult(const Guid &guid, const std::string &result);

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// App handler to make the thing behave largely like ordinary b2.

class OrdinaryAppHandler : public AppHandler {
  public:
    OrdinaryAppHandler(int argc, char *argv[]);

    std::string GetProductName() const override; //returns DEFAULT_PRODUCT_NAME
#if SYSTEM_OSX
    std::string GetFrameName() const override; //returns "b2Frame"
#endif
    bool IsHeadless() const override;       //returns false
    bool IsHighDPIEnabled() const override; //returns true
    bool IsSoundEnabled() const override;   //returns true
    std::vector<std::string> GetCommandLineArgs() const override;
    bool GetConfigAndCacheOverrideFolder(std::string *folder) const override; //returns false
    int GetRequestedHttpServerListenPort() const override;                    //returns 0xbbcb
    int GetLaunchRequestHttpServerPort() const override;                      //returns 0xbbcb
#ifdef IMGUI_ENABLE_TEST_ENGINE
    bool IsDearImGuiTestEngineEnabled() const override; //returns false
#endif
    bool HandleSelectorDialogOpen(std::string *result, const Guid &guid) override; //returns false
    bool ShouldQuitWhenTestQueueEmpty() const override;                            //returns false
  protected:
  private:
    std::vector<std::string> m_argv;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MainThreadMessage {
  public:
    MainThreadMessage() = default;
    virtual ~MainThreadMessage() = default;

    MainThreadMessage(const MainThreadMessage &) = delete;
    MainThreadMessage &operator=(const MainThreadMessage &) = delete;
    MainThreadMessage(MainThreadMessage &&) = delete;
    MainThreadMessage &operator=(MainThreadMessage &&) = delete;

    virtual void HandleMessage() = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushMainThreadMessage(std::unique_ptr<MainThreadMessage> message);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class FunctionMessage : public MainThreadMessage {
  public:
    explicit FunctionMessage(std::function<void()> fun);

    void HandleMessage() override;

  protected:
  private:
    std::function<void()> m_fun;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Create new window with the given init arguments.
//
// Factory function for this one as BeebWindowInitArguments is forward-declared.
void PushNewWindowMessage(BeebWindowInitArguments init_arguments);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Request a quit with the given error code.
void PushQuitMessage(int exit_code);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct GlobalSettings {
    bool vsync = true;
    EnumFlags<BeebConfigFeatureFlag> feature_flags;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GlobalSettings, vsync, feature_flags);

extern GlobalSettings g_global_settings;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// there's no return value for this. Check the result of GetHTTPServerListenPort
// for as much information as there is to have.
void StartHTTPServer(Messages *messages);

void StopHTTPServer();

// Returns 0 if server not running.
int GetHTTPServerListenPort();

// Whether it's valid to call StartHTTPServer.
bool CanStartHTTPServer();

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

// Do one tick of a placeholder no-op message loop, that consumes all messages
// available and largely ignores them. One exception: returns false if a quit
// event was encountered, and sets *exit_code to the exit code to use.
bool TickNoopMessageLoop(int *exit_code);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The main thread is the one that runs the SDL message loop.
bool IsMainThread();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int b2_main(AppHandler *handler);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
