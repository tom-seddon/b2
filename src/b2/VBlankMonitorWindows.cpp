#include <shared/system.h>
#include "VBlankMonitor.h"
#include "VBlankMonitorWindows.h"
#include <shared/system_windows.h>
#include <dxgi.h>
#include <shared/log.h>
#include "Messages.h"
#include <atlbase.h>
#include <atomic>
#include <shared/debug.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VBlankMonitorWindows : public VBlankMonitor {
  public:
    ~VBlankMonitorWindows() {
        this->ResetDisplayList();
    }

    bool Init(Handler *handler, Messages *messages) {
        m_handler = handler;

        HRESULT hr;

        {
            void *factory;
            hr = CreateDXGIFactory(IID_IDXGIFactory, &factory);
            if (FAILED(hr)) {
                messages->e.f("CreateDXGIFactory failed: %s", GetErrorDescription(hr));
                return false;
            }

            m_factory.Attach((IDXGIFactory *)factory);
        }

        {
            UINT adapter_idx;
            CComPtr<IDXGIAdapter> adapter;
            for (adapter_idx = 0, adapter = NULL;
                 m_factory->EnumAdapters(adapter_idx, &adapter) != DXGI_ERROR_NOT_FOUND;
                 ++adapter_idx) {
                UINT output_idx;
                CComPtr<IDXGIOutput> output;
                for (output_idx = 0, output = NULL;
                     adapter->EnumOutputs(output_idx, &output) != DXGI_ERROR_NOT_FOUND;
                     ++output_idx) {
                    DXGI_OUTPUT_DESC desc;
                    hr = output->GetDesc(&desc);
                    if (FAILED(hr)) {
                        continue;
                    }

                    if (!desc.Monitor) {
                        continue;
                    }

                    if (!desc.AttachedToDesktop) {
                        continue;
                    }

                    auto &&display = std::make_unique<Display>();

                    display->id = m_next_display_id++;
                    display->hmonitor = desc.Monitor;
                    display->vbm = this;
                    display->output = output;

                    display->mi.cbSize = sizeof display->mi;
                    if (!GetMonitorInfo(display->hmonitor, (MONITORINFO *)&display->mi)) {
                        memset(&display->mi, 0, sizeof display->mi);
                    }

                    display->data = m_handler->AllocateDisplayData(display->id);
                    if (!display->data) {
                        messages->e.f("AllocateDisplayData failed for monitor: %s\n", display->mi.szDevice);
                        return false;
                    }

                    m_displays.push_back(std::move(display));

                    output = nullptr;
                }

                adapter = nullptr;
            }
        }

        if (m_displays.empty()) {
            messages->e.f("No usable displays found");
            return false;
        }

        for (auto &&display : m_displays) {
            // (HANDLE)std::thread::native_handle would be an option
            // for VC++. Don't know how standard that is, though...
            // would be nice if it didn't obviously not work on MinGW.
            display->thread = (HANDLE)_beginthreadex(NULL, 0, &VBlankThread, display.get(), 0, NULL);
            if (!display->thread) {
                int e = errno;
                messages->e.f("failed to start thread for monitor: %s\n", display->mi.szDevice);
                messages->i.f("(_beginthreadex failed: %s\n)", strerror(e));
                return false;
            }

            BOOL ok = SetThreadPriority(display->thread, THREAD_PRIORITY_TIME_CRITICAL);
            ASSERT(ok); //but if it fails, no big deal.
            (void)ok;
        }

        return true;
    }

    void *GetDisplayDataForDisplayID(uint32_t display_id) const {
        for (auto &&display : m_displays) {
            if (display->id == display_id) {
                return display->data;
            }
        }

        return nullptr;
    }

    void *GetDisplayDataForPoint(int x, int y) const {
        POINT pt = {x, y};

        HMONITOR hmonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

        for (auto &&display : m_displays) {
            if (display->hmonitor == hmonitor) {
                return display->data;
            }
        }

        return nullptr;
    }

  protected:
  private:
    struct Display {
        uint32_t id = 0;
        HMONITOR hmonitor = nullptr;
        void *data = nullptr;
        VBlankMonitorWindows *vbm = nullptr;
        CComPtr<IDXGIOutput> output;
        MONITORINFOEX mi = {};
        std::atomic<bool> stop_thread = false;
        HANDLE thread = INVALID_HANDLE_VALUE;
    };

    Handler *m_handler;
    CComPtr<IDXGIFactory> m_factory;
    std::vector<std::unique_ptr<Display>> m_displays;
    uint32_t m_next_display_id = 1;

    void ResetDisplayList() {
        for (auto &&display : m_displays) {
            if (display->thread != 0) {
                display->stop_thread = true;

                WaitForSingleObject(display->thread, INFINITE);

                CloseHandle(display->thread);
                display->thread = nullptr;
            }

            m_handler->FreeDisplayData(display->id, display->data);
            display->data = nullptr;
        }

        m_displays.clear();
    }

    static unsigned __stdcall VBlankThread(void *arg) {
        auto display = (const Display *)arg;

        SetCurrentThreadNamef("VBlank Monitor: %s", display->mi.szDevice);

        while (!display->stop_thread) {
            // If the display sleeps, WaitForVBlank returns immediately. What
            // do? The answer seems to be: not much that's particularly good.
            // This copies
            // https://github.com/juce-framework/JUCE/commit/fb670d209b3a80b3a3d61d1edce0e2fd5ae89b2c
            // by imposing a frame rate limit. In this case, 250 Hz (4
            // ms/frame).
            //
            // Each ThreadVBlank call represents a fair amount of work, and 250
            // Hz isn't ideal, so this does need revisiting.

            uint64_t start_ticks = GetCurrentTickCount();
            uint64_t wait_ticks;
            for (;;) {
                display->output->WaitForVBlank();
                wait_ticks = GetCurrentTickCount() - start_ticks;
                if (GetMillisecondsFromTicks(wait_ticks) >= 4) {
                    break;
                }
                SleepMS(1);
            }

            display->vbm->m_handler->ThreadVBlank(display->id, display->data);
        }

        return 0;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VBlankMonitor> CreateVBlankMonitorWindows() {
    return std::make_unique<VBlankMonitorWindows>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
