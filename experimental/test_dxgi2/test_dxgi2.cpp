#include <shared/system.h>
#include <shared/system_windows.h>
#include <shared/testing.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <stdio.h>
#include <initguid.h>
#include <cguid.h>
#include <atlbase.h>
#include <dxgi.h>
#include <vector>
#include <thread>
#include <atomic>

#include <shared/enum_def.h>
#define ENAME DXGI_MODE_ROTATION
NBEGIN(DXGI_MODE_ROTATION)
NNS(DXGI_MODE_ROTATION_UNSPECIFIED, "UNSPECIFIED")
NNS(DXGI_MODE_ROTATION_IDENTITY, "IDENTITY")
NNS(DXGI_MODE_ROTATION_ROTATE90, "ROTATE90")
NNS(DXGI_MODE_ROTATION_ROTATE180, "ROTATE180")
NNS(DXGI_MODE_ROTATION_ROTATE270, "ROTATE270")
NEND()
#undef ENAME
#include <shared/enum_end.h>

LOG_DEFINE(OUTPUT, "", &log_printer_stdout_and_debugger);

static int TestSucceededHR(HRESULT hr, const char *expr, const char *file, int line) {
    if (SUCCEEDED(hr)) {
        return 1;
    } else {
        TestFailed(file, line, NULL);

        LOGF(TESTING, "FAILED: %s\n", expr);
        LOGF(TESTING, "    HRESULT: 0x%08X (%s)\n", hr, GetErrorDescription(hr));

        return 0;
    }
}

#define TEST_SHR(EXPR) TEST(TestSucceededHR((EXPR), #EXPR, __FILE__, __LINE__))

// N.B. argument X appears multiple times in the expansion.
//#define RELEASE(X)\
//BEGIN_MACRO {\
//    if(X) {\
//        (X)->lpVtbl->Release(X);\
//        (X)=NULL;\
//    }\
//} END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//struct Thread {
//    CComPtr<IDXGIOutput> output;
//    int num_vbls=0;
//    double seconds=0.;
//};
//
//static void WaitVBlankThread(Thread *t) {
//    t->output->WaitForVBlank();
//
//    uint64_t a=GetCurrentTickCount();
//
//    for(int i=0;i<state->num_vbls;++i) {
//        t->output->WaitForVBlank();
//    }
//
//    uint64_t b=GetCurrentTickCount();
//
//    t->seconds=GetSecondsFromTicks(b-a);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char MAIN_WND_CLASS_NAME[] = "MainWnd-ED367BE1-848F-4851-95BC-6B730E314ED1";

struct MainWndCreateParams {
    size_t index = 0;
};

struct MainWndState {
    size_t index = 0;
    LONG x = 0;
    const char *message = nullptr;
};

static void UpdateMainWnd(HWND h) {
    auto ws = (MainWndState *)GetWindowLongPtr(h, GWLP_USERDATA);
    if (!ws) {
        return;
    }

    RECT client;
    GetClientRect(h, &client);

    ++ws->x;
    ws->x %= client.right;
}

static void PaintMainWnd(HWND h) {
    auto ws = (MainWndState *)GetWindowLongPtr(h, GWLP_USERDATA);
    if (!ws) {
        return;
    }

    HDC dc = GetDC(h);
    SaveDC(dc);

    RECT client;
    GetClientRect(h, &client);

    FillRect(dc, &client, (HBRUSH)GetStockObject(WHITE_BRUSH));

    SelectObject(dc, GetStockObject(BLACK_PEN));
    MoveToEx(dc, ws->x, client.top, NULL);
    LineTo(dc, ws->x, client.bottom);

    if (ws->message) {
        SelectObject(dc, GetStockObject(SYSTEM_FIXED_FONT));
        SetTextAlign(dc, TA_CENTER | TA_BASELINE);
        TextOut(dc, client.right / 2, client.bottom / 2, ws->message, (int)strlen(ws->message));
    }

    RestoreDC(dc, -1);
    ReleaseDC(h, dc);
    dc = NULL;
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto ws = (MainWndState *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_CREATE: {
        CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
        MainWndCreateParams *params = (MainWndCreateParams *)cs->lpCreateParams;

        ws = new MainWndState;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ws);

        ws->index = params->index;

        char tmp[100];
        snprintf(tmp, sizeof tmp, "Window %zu", ws->index);
        SetWindowText(hWnd, tmp);
    } break;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hWnd, &ps);
        if (dc) {
            //SaveDC(dc);

            //RestoreDC(dc,-1);
            EndPaint(hWnd, &ps);
        }
    } break;

    case WM_LBUTTONUP:
        ws->x = 0;
        break;

    case WM_DESTROY: {
        delete ws;
        ws = nullptr;
    } break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void RegisterMainWndClass(void) {
    WNDCLASSEX w = {};

    w.cbSize = sizeof w;
    w.style = CS_VREDRAW | CS_HREDRAW;
    w.lpfnWndProc = &MainWndProc;
    w.hInstance = GetModuleHandle(NULL);
    w.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    w.hCursor = LoadCursor(NULL, IDC_ARROW);
    w.lpszClassName = MAIN_WND_CLASS_NAME;
    w.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&w);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Thread {
    HMONITOR hmonitor = nullptr;
    MONITORINFOEX miex = {};
    CComPtr<IDXGIOutput> output;
    HANDLE event = nullptr;
    std::thread thread;
    std::atomic<bool> stop_thread{false};
};

static std::vector<std::unique_ptr<Thread>> g_threads;

static void StartThreads() {
    CComPtr<IDXGIFactory> factory;
    {
        void *tmp;
        TEST_SHR(CreateDXGIFactory(IID_IDXGIFactory, &tmp));
        factory = (IDXGIFactory *)tmp;
    }

    UINT adapter_idx = 0;
    for (;;) {
        CComPtr<IDXGIAdapter> adapter;
        if (factory->EnumAdapters(adapter_idx++, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        UINT output_idx = 0;
        for (;;) {
            CComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(output_idx++, &output) == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            DXGI_OUTPUT_DESC desc;
            TEST_SHR(output->GetDesc(&desc));

            if (!desc.Monitor || !desc.AttachedToDesktop) {
                continue;
            }

            auto &&t = std::make_unique<Thread>();

            t->hmonitor = desc.Monitor;
            t->output = output;
            // FALSE=not auto-reset; FALSE=initial state
            t->event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            TEST_NON_NULL(t->event);
            t->miex.cbSize = sizeof t->miex;
            TEST_TRUE(GetMonitorInfo(t->hmonitor, (MONITORINFO *)&t->miex));

            g_threads.push_back(std::move(t));
        }
    }

    for (size_t i = 0; i < g_threads.size(); ++i) {
        Thread *t = g_threads[i].get();

        t->thread = std::thread([t, i]() {
            SetCurrentThreadNamef("Display %zu", i);

            while (!t->stop_thread) {
                t->output->WaitForVBlank();
                SetEvent(t->event);
            }
        });
    }
}

static void StopThreads() {
    for (auto &&t : g_threads) {
        t->thread.join();
        CloseHandle(t->event);
    }
    g_threads.clear();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static void *AllocDisplayDataEvent(void *context) {
//    (void)context;
//
//    // FALSE=not auto-reset; FALSE=initial state
//    return CreateEvent(NULL,FALSE,FALSE,NULL);
//}

//static void FreeDisplayDataEvent(void *display_data,void *context) {
//    (void)context;
//
//    CloseHandle(display_data);
//}

//static void VBlankDisplay(void *display_data,void *context) {
//    (void)context;
//
//    SetEvent(display_data);
//}

//static const VBlankMonitorCallbacks VBLANK_MONITOR_CALLBACKS={
//    .alloc_fn=&AllocDisplayDataEvent,
//    .free_fn=&FreeDisplayDataEvent,
//    .vblank_fn=&VBlankDisplay,
//};

// must be up to 64, and <=MAXIMUM_WAIT_OBJECTS
static const size_t MAX_NUM_MONITORS = 64;

int main(void) {
    RegisterMainWndClass();

    //VBlankMonitor *monitor=VBlankMonitorAlloc(&VBLANK_MONITOR_CALLBACKS);

    StartThreads();

    std::vector<HWND> windows;

    for (size_t i = 0; i < g_threads.size(); ++i) {
        MainWndCreateParams params;
        params.index = i;
        HWND h = CreateWindow(MAIN_WND_CLASS_NAME, "", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, -1, 320, 200, NULL, NULL, GetModuleHandle(NULL), &params);
        windows.push_back(h);
        ShowWindow(h, SW_SHOW);
    }

    HBRUSH coloured_brushes[8];
    for (size_t i = 0; i < 8; ++i) {
        coloured_brushes[i] = CreateSolidBrush(RGB(i & 1 ? 255 : 0, i & 2 ? 255 : 0, i & 4 ? 255 : 0));
        ASSERT(coloured_brushes[i]);
    }

    //size_t brush_index=0;

    uint64_t run_begin_ticks = GetCurrentTickCount();
    size_t num_waits = 0;
    size_t num_vblank_wakeups = 0, num_message_wakeups = 0;

    for (;;) {
        HANDLE wait_events[MAXIMUM_WAIT_OBJECTS];
        uint64_t window_masks[MAXIMUM_WAIT_OBJECTS];
        DWORD event_idx_by_display_idx[MAXIMUM_WAIT_OBJECTS];
        DWORD num_wait_events = 0;

        for (size_t i = 0; i < MAXIMUM_WAIT_OBJECTS; ++i) {
            event_idx_by_display_idx[i] = MAXDWORD;
        }

        for (size_t i = 0; i < windows.size(); ++i) {
        }

        for (size_t i = 0; i < windows.size(); ++i) {
            auto ws = (MainWndState *)GetWindowLongPtr(windows[i], GWLP_USERDATA);
            ws->message = nullptr;

            HMONITOR hmonitor = MonitorFromWindow(windows[i], MONITOR_DEFAULTTONULL);
            if (!hmonitor) {
                continue;
            }

            size_t display_idx = SIZE_MAX;
            for (size_t j = 0; j < g_threads.size(); ++j) {
                if (g_threads[j]->hmonitor == hmonitor) {
                    display_idx = j;
                    break;
                }
            }
            TEST_TRUE(display_idx != SIZE_MAX);

            Thread *display_thread = g_threads[display_idx].get();

            ws->message = display_thread->miex.szDevice;

            if (event_idx_by_display_idx[display_idx] == MAXDWORD) {
                DWORD event_idx = num_wait_events++;
                event_idx_by_display_idx[display_idx] = event_idx;
                wait_events[event_idx] = display_thread->event;

                window_masks[event_idx] = (uint64_t)1 << i;
            } else {
                DWORD event_idx = event_idx_by_display_idx[display_idx];
                ASSERT(wait_events[event_idx] == display_thread->event);
                window_masks[event_idx] |= (uint64_t)1 << i;
            }
        }

        ++num_waits;
        DWORD wait_result = MsgWaitForMultipleObjectsEx(num_wait_events, wait_events, INFINITE, QS_ALLEVENTS, 0);
        if (wait_result >= WAIT_OBJECT_0 && wait_result < WAIT_OBJECT_0 + num_wait_events) {
            ++num_vblank_wakeups;
            DWORD64 mask = window_masks[wait_result - WAIT_OBJECT_0];
            int index;
            while ((index = GetHighestSetBitIndex64(mask)) != -1) {
                ASSERT(index < windows.size());
                HWND h = windows[index];

                UpdateMainWnd(h);
                PaintMainWnd(h);

                mask &= ~((DWORD64)1 << index);
            }
        } else if (wait_result == WAIT_OBJECT_0 + num_wait_events) {
            ++num_message_wakeups;
            // message
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
                int get_result = GetMessage(&msg, NULL, 0, 0);
                if (get_result == 0 || get_result == -1) {
                    goto done;
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
done:;

    StopThreads();

    uint64_t run_end_ticks = GetCurrentTickCount();
    LOGF(OUTPUT, "%.3f waits/sec\n", (double)num_waits / GetSecondsFromTicks(run_end_ticks - run_begin_ticks));
    LOGF(OUTPUT, "    %zu/%zu wakeups total\n", num_vblank_wakeups + num_message_wakeups, num_waits);
    LOGF(OUTPUT, "    %zu/%zu vblank wakeups\n", num_vblank_wakeups, num_waits);
    LOGF(OUTPUT, "    %zu/%zu message wakeups\n", num_message_wakeups, num_waits);

    for (HWND h : windows) {
        DestroyWindow(h);
    }
    windows.clear();

    for (size_t i = 0; i < 8; ++i) {
        DeleteObject(coloured_brushes[i]);
    }
}
