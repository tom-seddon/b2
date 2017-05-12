#include <shared/system.h>
#include <shared/system_windows.h>
#include <shared/testing.h>
#include <shared/log.h>
#include <vector>
#include <stdio.h>
#include <initguid.h>
#include <cguid.h>
#include <atlbase.h>
#include <thread>
#include <dxgi.h>

#include <shared/enum_def.h>
#define ENAME DXGI_MODE_ROTATION
NBEGIN(DXGI_MODE_ROTATION)
NNS(DXGI_MODE_ROTATION_UNSPECIFIED,"UNSPECIFIED")
NNS(DXGI_MODE_ROTATION_IDENTITY,"IDENTITY")
NNS(DXGI_MODE_ROTATION_ROTATE90,"ROTATE90")
NNS(DXGI_MODE_ROTATION_ROTATE180,"ROTATE180")
NNS(DXGI_MODE_ROTATION_ROTATE270,"ROTATE270")
NEND()
#undef ENAME
#include <shared/enum_end.h>

LOG_DEFINE(TEST,"",&log_printer_stdout_and_debugger)

static int TestSucceededHR(HRESULT hr,const char *expr,const char *file,int line) {
    if(SUCCEEDED(hr)) {
        return 1;
    } else {
        TestFailed(file,line,NULL);

        LOGF(TESTING,"FAILED: %s\n",expr);
        LOGF(TESTING,"    HRESULT: 0x%08X (%s)\n",hr,GetErrorDescription(hr));

        return 0;
    }
}

#define TEST_SHR(EXPR) TEST(TestSucceededHR((EXPR),#EXPR,__FILE__,__LINE__))

// N.B. argument X appears multiple times in the expansion.
#define RELEASE(X)\
BEGIN_MACRO {\
    if(X) {\
        (X)->Release();\
        (X)=NULL;\
    }\
} END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Thread {
    CComPtr<IDXGIOutput> output;
    int num_vbls=0;
    double seconds=0.;
    std::thread thread;
};

static void WaitVBlankThread(Thread *t) {
    t->output->WaitForVBlank();

    uint64_t a=GetCurrentTickCount();

    for(int i=0;i<t->num_vbls;++i) {
        t->output->WaitForVBlank();
    }

    uint64_t b=GetCurrentTickCount();

    t->seconds=GetSecondsFromTicks(b-a);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(void) {
    CComPtr<IDXGIFactory> factory;
    {
        void *tmp;
        TEST_SHR(CreateDXGIFactory(IID_IDXGIFactory,&tmp));
        factory=(IDXGIFactory *)tmp;
    }

    std::vector<CComPtr<IDXGIAdapter>> adapters;
    {
        UINT i=0;
        CComPtr<IDXGIAdapter> adapter;
        while(factory->EnumAdapters(i,&adapter)!=DXGI_ERROR_NOT_FOUND) {
            adapters.push_back(std::move(adapter));
            ++i;
        }
    }

    LOGF(TEST,"%zu adapters:\n",adapters.size());
    for(size_t adapter_idx=0;adapter_idx<adapters.size();++adapter_idx) {
        CComPtr<IDXGIAdapter> adapter=adapters[adapter_idx];

        DXGI_ADAPTER_DESC adapter_desc;
        TEST_SHR(adapter->GetDesc(&adapter_desc));

        LOGF(TEST,"%zu. ",adapter_idx);
        LOGI(TEST);
        LOGF(TEST,"Description: %S\n",adapter_desc.Description);
        LOGF(TEST,"DedicatedVideoMemory: %.1f MBytes\n",adapter_desc.DedicatedVideoMemory/1024.);
        LOGF(TEST,"DedicatedSystemMemory: %.1f MBytes\n",adapter_desc.DedicatedSystemMemory/1024.);
        LOGF(TEST,"SharedSystemMemory: %.1f MBytes\n",adapter_desc.SharedSystemMemory/1024.);

        LOGF(TEST,"Outputs:\n");

        std::vector<CComPtr<IDXGIOutput>> outputs;
        {
            UINT i=0;
            CComPtr<IDXGIOutput> output;
            while(adapter->EnumOutputs(i,&output)!=DXGI_ERROR_NOT_FOUND) {
                outputs.push_back(std::move(output));
                ++i;
            }
        }

        for(size_t output_idx=0;output_idx<outputs.size();++output_idx) {
            CComPtr<IDXGIOutput> output=outputs[output_idx];

            DXGI_OUTPUT_DESC desc;
            TEST_SHR(output->GetDesc(&desc));

            LOGF(TEST,"%zu. ",output_idx);
            LOGI(TEST);

            LOGF(TEST,"DeviceName: %S\n",desc.DeviceName);
            {
                const RECT *r=&desc.DesktopCoordinates;
                LOGF(TEST,"DesktopCoordinates: (%ld,%ld)-(%ld,%ld) (%ldx%ld)\n",r->left,r->top,r->right,r->bottom,r->right-r->left,r->bottom-r->top);
            }
            LOGF(TEST,"AttachedToDesktop: %s\n",BOOL_STR(desc.AttachedToDesktop));
            LOGF(TEST,"Rotation: %s\n",GetDXGI_MODE_ROTATIONEnumName(desc.Rotation));
        }

        uint64_t measure_begin_ticks=GetCurrentTickCount();

        std::vector<Thread *> threads;

        for(size_t i=0;i<outputs.size();++i) {
            auto t=new Thread;

            t->num_vbls=100;
            t->output=outputs[i];
            t->thread=std::thread([t,i]() {
                SetCurrentThreadNamef("output %zu",i);
                WaitVBlankThread(t);
            });

            threads.push_back(t);
        }

        for(size_t i=0;i<threads.size();++i) {
            auto &&t=threads[i];

            t->thread.join();

            LOGF(TEST,"Output %zu: Est. refresh rate: %.1fHz\n",i,t->num_vbls/t->seconds);
        }

        for(Thread *t:threads) {
            delete t;
        }
        threads.clear();

        uint64_t measure_end_ticks=GetCurrentTickCount();
        LOGF(TEST,"(total measure time: %.1f sec)\n",GetSecondsFromTicks(measure_end_ticks-measure_begin_ticks));
    }
}
