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
#include <shared/mutex.h>
#include "misc.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// this dummy log exists so there's a "vblank" tag that can be enabled
// or disabled from the command line.
LOG_TAGGED_DEFINE(VBLANK_DUMMY,"vblank","",&log_printer_stdout_and_debugger,false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VBlankMonitorWindows:
    public VBlankMonitor
{
public:
    ~VBlankMonitorWindows() {
        this->ResetDisplayList();
    }

    bool Init(Handler *handler,Messages *messages) {
        m_handler=handler;

        HRESULT hr;

        {
            void *factory;
            hr=CreateDXGIFactory(IID_IDXGIFactory,&factory);
            if(FAILED(hr)) {
                messages->e.f("CreateDXGIFactory failed: %s",GetErrorDescription(hr));
                return false;
            }

            m_factory.Attach((IDXGIFactory *)factory);
        }

        {
            UINT adapter_idx;
            CComPtr<IDXGIAdapter> adapter;
            for(adapter_idx=0,adapter=NULL;
                m_factory->EnumAdapters(adapter_idx,&adapter)!=DXGI_ERROR_NOT_FOUND;
                ++adapter_idx)
            {
                UINT output_idx;
                CComPtr<IDXGIOutput> output;
                for(output_idx=0,output=NULL;
                    adapter->EnumOutputs(output_idx,&output)!=DXGI_ERROR_NOT_FOUND;
                    ++output_idx)
                {
                    DXGI_OUTPUT_DESC desc;
                    hr=output->GetDesc(&desc);
                    if(FAILED(hr)) {
                        continue;
                    }

                    if(!desc.Monitor) {
                        continue;
                    }

                    if(!desc.AttachedToDesktop) {
                        continue;
                    }

                    auto &&display=std::make_shared<Display>();

                    display->id=m_next_display_id++;
                    display->hmonitor=desc.Monitor;
                    display->vbm=this;
                    display->output=output;
                    //display->messages_list=messages->GetMessageList();

                    display->mi.cbSize=sizeof display->mi;
                    if(!GetMonitorInfo(display->hmonitor,(MONITORINFO *)&display->mi)) {
                        memset(&display->mi,0,sizeof display->mi);
                    }

                    display->data=m_handler->AllocateDisplayData(display->id);
                    if(!display->data) {
                        messages->e.f("AllocateDisplayData failed for display: %s\n",display->mi.szDevice);
                        return false;
                    }

                    m_displays.push_back(std::move(display));

                    output=nullptr;
                }

                adapter=nullptr;
            }
        }

        if(m_displays.empty()) {
            messages->e.f("No usable displays found");
            return false;
        }

        for(auto &&display:m_displays) {
            auto thread=std::make_shared<Thread>();

            MUTEX_SET_NAME(thread->mutex,strprintf("Mutex for display: %s",display->mi.szDevice));

            thread->display=display;
            thread->display_name=display->mi.szDevice;

            // this has to be a non-std::thread, so that it can just
            // be allowed to leak when/if the DXGI stuff isn't
            // working.
            thread->vblank_thread=(HANDLE)_beginthreadex(NULL,0,&VBlankThread,thread.get(),0,NULL);
            if(!thread->vblank_thread) {
                int e=errno;
                messages->e.f("failed to start vblank thread for display: %s\n",thread->display_name.c_str());
                messages->i.f("(_beginthreadex failed: %s)\n",strerror(e));
                return false;
            }

            BOOL ok=SetThreadPriority(thread->vblank_thread,THREAD_PRIORITY_TIME_CRITICAL);
            ASSERT(ok);//but if it fails, no big deal.
            (void)ok;

            m_threads.push_back(thread);
        }

        for(auto &&thread:m_threads) {
            try {
                thread->monitor_thread=std::thread([thread]() {
                    VBlankMonitorThread(thread);
                });
            } catch(const std::system_error &exc) {
                messages->e.f("failed to start monitoring thread for display: %s\n",thread->display_name.c_str());
                messages->i.f("(std::thread failed: %s)\n",exc.what());
                return false;
            }
        }

        return true;
    }

    void *GetDisplayDataForDisplayID(uint32_t display_id) const {
        for(auto &&display:m_displays) {
            if(display->id==display_id) {
                return display->data;
            }
        }

        return nullptr;
    }

    void *GetDisplayDataForPoint(int x,int y) const {
        POINT pt={x,y};
        HMONITOR hmonitor=MonitorFromPoint(pt,MONITOR_DEFAULTTONULL);
        if(!hmonitor) {
            return nullptr;
        }

        for(auto &&display:m_displays) {
            if(display->hmonitor==hmonitor) {
                return display->data;
            }
        }

        return nullptr;
    }
protected:
private:
    struct Display {
        // id of the display.
        uint32_t id=0;

        // handle to the Win32 monitor, and its info.
        HMONITOR hmonitor=nullptr;
        MONITORINFOEX mi;

        // vblank callback and its context.
        void *data=nullptr;
        VBlankMonitorWindows *vbm=nullptr;

        // IDXGIOutput for this display.
        CComPtr<IDXGIOutput> output;
    };

    struct Thread:
        std::enable_shared_from_this<Thread>
    {
        Mutex mutex;

        HANDLE vblank_thread=nullptr;

        std::thread monitor_thread;

        bool wait_for_vblank=true;
        bool any_vblank=false;

        std::shared_ptr<const Display> display;
        std::string display_name;

        std::shared_ptr<MessageList> message_list;
    };

    Handler *m_handler;
    CComPtr<IDXGIFactory> m_factory;
    std::vector<std::shared_ptr<Thread>> m_threads;
    std::vector<std::shared_ptr<Display>> m_displays;
    uint32_t m_next_display_id=1;

    void ResetDisplayList() {
        std::vector<HANDLE> vblank_threads;

        for(auto &&thread:m_threads) {
            {
                std::lock_guard<Mutex> lock(thread->mutex);

                thread->display=nullptr;
            }

            if(thread->vblank_thread) {
                vblank_threads.push_back(thread->vblank_thread);
            }

            if(thread->monitor_thread.joinable()) {
                thread->monitor_thread.join();
            }
        }

        // If the thread gets stuck waiting for the vblank (see
        // https://github.com/tom-seddon/b2/issues/6) it can't be
        // stopped cleanly, so it has to just leak :(
        //
        // If it resumes later, for some reason, it'll spot the null
        // display pointer and return.
        //
        // There's no point checking the result, because if it didn't
        // work, there's not much you can do...
        WaitForMultipleObjects((DWORD)vblank_threads.size(),vblank_threads.data(),TRUE,500);

        for(auto &&vblank_thread:vblank_threads) {
            CloseHandle(vblank_thread);
        }

        for(auto &&display:m_displays) {
            display->vbm->m_handler->FreeDisplayData(display->id,display->data);
        }

        m_displays.clear();
    }

    static void VBlankThreadTimerMode(const std::shared_ptr<Thread> &thread) {
        for(;;) {
            std::shared_ptr<const Display> display;
            {
                std::lock_guard<Mutex> lock(thread->mutex);

                display=thread->display;

                if(!display) {
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::microseconds(20000));

            CallHandler(display);
        }
    }

    static void VBlankMonitorThread(std::shared_ptr<Thread> thread) {
        Log log(("VBLMON: "+thread->display_name).c_str(),LOG(VBLANK_DUMMY).GetPrinter(),LOG(VBLANK_DUMMY).enabled);
        SetCurrentThreadNamef("%s - Monitor VBlank",thread->display_name.c_str());

        log.f("thread=%p, display=%p\n",thread.get(),thread->display.get());

        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        bool bork=false;
        {
            std::lock_guard<Mutex> lock(thread->mutex);

            if(!thread->any_vblank) {
                thread->wait_for_vblank=false;
                bork=true;
            }
        }

        if(bork) {
            log.f("bork. Using timer mode.\n",thread->display_name.c_str());
            VBlankThreadTimerMode(thread);
        } else {
            log.f("WaitForVBlank appears to be working. Monitor thread will finish.\n",thread->display_name.c_str());
        }
    }

    static void CallHandler(const std::shared_ptr<const Display> &display) {
        if(!display) {
            return;
        }

        if(display->vbm) {
            display->vbm->m_handler->ThreadVBlank(display->id,display->data);
        }
    }

    static unsigned __stdcall VBlankThread(void *arg) {
        auto thread=((Thread *)arg)->shared_from_this();

        Log log(("VBLANK: "+thread->display_name).c_str(),LOG(VBLANK_DUMMY).GetPrinter(),LOG(VBLANK_DUMMY).enabled);
        SetCurrentThreadNamef("%s - VBlank",thread->display_name.c_str());

        std::string device;
        {
            std::lock_guard<Mutex> lock(thread->mutex);

            device=thread->display->mi.szDevice;
        }

        log.f("starting vblank thread.\n");


        for(;;) {
            std::shared_ptr<const Display> display;
            {
                std::lock_guard<Mutex> lock(thread->mutex);

                if(!thread->display) {
                    log.f("vblank thread will end. (display is NULL before WaitForVBlank.)\n");
                    break;
                }

                if(!thread->wait_for_vblank) {
                    log.f("vblank thread will end. (wait_for_vblank is false before WaitForVBlank.)\n");
                    break;
                }

                display=thread->display;
            }

            //for(;;) {
            //    Sleep(2000);
            //}

            display->output->WaitForVBlank();
            thread->any_vblank=true;

            {
                std::lock_guard<Mutex> lock(thread->mutex);

                if(!thread->display) {
                    log.f("vblank thread will end. (display is NULL after WaitForVBlank.)\n");
                    break;
                }

                if(!thread->wait_for_vblank) {
                    log.f("vblank thread will end. (wait_for_vblank is false after WaitForVBlank.)\n");
                    break;
                }

                display=thread->display;
            }

            CallHandler(display);
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
