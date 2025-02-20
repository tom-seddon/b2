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
#include <shared/log.h>
#include <inttypes.h>
#include "b2.h"
#include "misc.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_def.h>
#define ENAME DWORD
NBEGIN(DXGI_MODE_ROTATION)
NN(DXGI_MODE_ROTATION_UNSPECIFIED)
NN(DXGI_MODE_ROTATION_IDENTITY)
NN(DXGI_MODE_ROTATION_ROTATE90)
NN(DXGI_MODE_ROTATION_ROTATE180)
NN(DXGI_MODE_ROTATION_ROTATE270)
NEND()
#undef ENAME
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(VBLANK);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define MBYTES_FROM_BYTES(X) ((X) / 1024. / 1024.)

class VBlankMonitorWindows : public VBlankMonitor {
  public:
    ~VBlankMonitorWindows() {
        this->ResetDisplayList();
    }

    bool Init(Handler *handler, Messages *messages) {
        (void)messages;

        m_handler = handler;

        return true;
    }

    bool RefreshDisplayList(Messages *messages) override {
        ++m_num_refreshes;
        LOGF(VBLANK, "VBlankMonitorWindows::RefreshDisplayList: %" PRIu64 " calls\n", m_num_refreshes);

        this->ResetDisplayList();

        HRESULT hr;

        {
            void *factory;
            hr = CreateDXGIFactory1(IID_IDXGIFactory1, &factory);
            if (FAILED(hr)) {
                messages->e.f("CreateDXGIFactory failed: %s\n", GetErrorDescription(hr));
                return false;
            }

            m_factory.Attach((IDXGIFactory1 *)factory);
        }

        {
            UINT adapter_idx;
            CComPtr<IDXGIAdapter1> adapter;
            for (adapter_idx = 0, adapter = NULL;
                 m_factory->EnumAdapters1(adapter_idx, &adapter) != DXGI_ERROR_NOT_FOUND;
                 ++adapter_idx) {
                DXGI_ADAPTER_DESC1 adesc;

                LOGF(VBLANK, "DXGI adapter %u: ", adapter_idx);
                LOGI(VBLANK);

                hr = adapter->GetDesc1(&adesc);
                if (FAILED(hr)) {
                    LOGF(VBLANK, "failed to get description: %s\n", GetErrorDescription(hr));
                    // ...but this isn't necessarily fatal.
                } else {
                    // LOGF isn't actually designed for UTF8, but, hopefully, close enough...
                    LOGF(VBLANK, "Description: %s\n", GetUTF8String(adesc.Description).c_str());
                    LOGF(VBLANK, "VendorId=0x%x; DeviceId=0x%x; SubSysId=0x%x; Revision=0x%x\n", adesc.VendorId, adesc.DeviceId, adesc.SubSysId, adesc.Revision);
                    LOGF(VBLANK, "DedicatedVideoMemory=%.3f MBytes; DedicatedSystemMemory=%.3f MBytes; SharedSystemMemory=%.3f MBytes\n", MBYTES_FROM_BYTES(adesc.DedicatedVideoMemory), MBYTES_FROM_BYTES(adesc.DedicatedSystemMemory), MBYTES_FROM_BYTES(adesc.SharedSystemMemory));
                    LOGF(VBLANK, "Flags: 0x%08" PRIx32 "\n", adesc.Flags);
                }

                UINT output_idx;
                CComPtr<IDXGIOutput> output;
                for (output_idx = 0, output = NULL;
                     adapter->EnumOutputs(output_idx, &output) != DXGI_ERROR_NOT_FOUND;
                     ++output_idx) {
                    LOGF(VBLANK, "Output %u: ", output_idx);
                    LOGI(VBLANK);

                    DXGI_OUTPUT_DESC odesc;
                    hr = output->GetDesc(&odesc);
                    if (FAILED(hr)) {
                        LOGF(VBLANK, "failed to get description: %s\n", GetErrorDescription(hr));
                        continue;
                    }

                    LOGF(VBLANK, "DeviceName: %s\n", GetUTF8String(odesc.DeviceName).c_str());
                    LOGF(VBLANK, "DesktopCoordinates: (%ld,%ld)-(%ld,%ld) (%ld x %ld)\n", odesc.DesktopCoordinates.left, odesc.DesktopCoordinates.top, odesc.DesktopCoordinates.right, odesc.DesktopCoordinates.bottom, odesc.DesktopCoordinates.right - odesc.DesktopCoordinates.left, odesc.DesktopCoordinates.bottom - odesc.DesktopCoordinates.top);
                    LOGF(VBLANK, "AttachedToDesktop: %s\n", BOOL_STR(odesc.AttachedToDesktop));
                    LOGF(VBLANK, "Rotation: %s\n", GetDXGI_MODE_ROTATIONEnumName(odesc.Rotation));
                    LOGF(VBLANK, "Monitor: %p\n", odesc.Monitor);

                    if (!odesc.Monitor) {
                        continue;
                    }

                    if (!odesc.AttachedToDesktop) {
                        continue;
                    }

                    auto &&display = std::make_unique<Display>();

                    display->id = m_next_display_id++;
                    display->hmonitor = odesc.Monitor;
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
                LOGF(VBLANK, "Outputs found: %u\n", output_idx);

                adapter = nullptr;
            }
        }

        if (m_displays.empty()) {
            messages->e.f("No usable displays found\n");
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

    bool NeedsRefreshDisplayList() const override {
        if (m_factory->IsCurrent()) {
            return false;
        } else {
            return true;
        }
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
    CComPtr<IDXGIFactory1> m_factory;
    std::vector<std::unique_ptr<Display>> m_displays;
    uint32_t m_next_display_id = 1;
    uint64_t m_num_refreshes = 0;

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

        Log log(strprintf("display %" PRIu32, display->id).c_str(), LOG(VBLANK));

        bool first_update = true;
        bool probably_sleeping = false;

        while (!display->stop_thread) {
            // If the display is asleep, WaitForVBlank successfully returns
            // immediately. (Try to) detect this case by timing the call.
            //
            // Assume a 50 Hz frame time if the display is asleep. It's as good
            // a guess as any.
            
            uint64_t start_ticks = GetCurrentTickCount();
            HRESULT hr = display->output->WaitForVBlank();
            if (FAILED(hr)) {
                // Just pick some random value.
                SleepMS(20);
            } else {
                uint64_t wait_ticks = GetCurrentTickCount() - start_ticks;
                double wait_ms = GetMillisecondsFromTicks(wait_ticks);
                if (wait_ms < 1. && !first_update) {
                    if (!probably_sleeping) {
                        log.f("frame time was ~%.1fms: probably now asleep.\n", wait_ms);
                        probably_sleeping = true;
                    }

                    // pick some random value.
                    SleepMS(20);
                } else {
                    if (probably_sleeping) {
                        log.f("frame time was ~%.1fms: probably now awake.\n", wait_ms);
                        probably_sleeping = false;
                    }
                }
            }

            first_update = false;

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
