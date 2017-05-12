#include <shared/system.h>
#include <shared/system_specific.h>
#include <shared/testing.h>
#include <shared/log.h>
#include <vector>
#include <stdio.h>
#include <mmsystem.h>
#include <initguid.h>
#include <cguid.h>
#include <atlbase.h>
#include <dxgi.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(TEST,"",&log_printer_stdout_and_debugger);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define TEST_SHR(EXPR) TEST(TestSucceededHR((EXPR),#EXPR,__FILE__,__LINE__))

// N.B. argument X appears multiple times in the expansion.
#define RELEASE(X)\
BEGIN_MACRO {\
    if(X) {\
        (X)->lpVtbl->Release(X);\
        (X)=NULL;\
    }\
} END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static CComPtr<IDXGIOutput> FindOutputByHMONITOR(const CComPtr<IDXGIFactory> &factory,HMONITOR hm)
{
    UINT adapter_idx=0;
    CComPtr<IDXGIAdapter> adapter;
    while(factory->EnumAdapters(adapter_idx,&adapter)!=DXGI_ERROR_NOT_FOUND) {
        UINT output_idx=0;
        CComPtr<IDXGIOutput> output;
        while(adapter->EnumOutputs(output_idx,&output)!=DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC desc;
            TEST_SHR(output->GetDesc(&desc));

            if(desc.Monitor==hm) {
                return output;
            }

            output=nullptr;
            ++output_idx;
        }

        adapter=nullptr;
        ++adapter_idx;
    }

    return nullptr;
}

static const size_t NUM_BUCKETS=50;

int main(void) {
    CComPtr<IDXGIFactory> factory;
    {
        void *tmp;
        TEST_SHR(CreateDXGIFactory(IID_IDXGIFactory,&tmp));
        factory=(IDXGIFactory *)tmp;
    }

    timeBeginPeriod(1);

    for(;;) {
        HMONITOR hm=MonitorFromWindow(GetConsoleWindow(),MONITOR_DEFAULTTONULL);
        if(!hm) {
            printf("unknown monitor...\n");
            Sleep(1000);
        } else {
            CComPtr<IDXGIOutput> output=FindOutputByHMONITOR(factory,hm);
            if(!output) {
                printf("failed to find output...\n");
                Sleep(1000);
            } else {
                size_t num_vblanks=150;

                std::vector<uint64_t> ticks_per_vblank;
                ticks_per_vblank.reserve(num_vblanks);

                printf("%zu vblanks...\n",num_vblanks);

                output->WaitForVBlank();
                uint64_t old_ticks=GetCurrentTickCount();

                for(size_t i=0;i<num_vblanks;++i) {
                    output->WaitForVBlank();
                    uint64_t new_ticks=GetCurrentTickCount();
                    ticks_per_vblank.push_back(new_ticks-old_ticks);
                    old_ticks=new_ticks;
                }

                size_t buckets[NUM_BUCKETS]={};

                uint64_t mini=UINT64_MAX,maxi=0,total=0;

                for(size_t i=0;i<ticks_per_vblank.size();++i) {
                    total+=ticks_per_vblank[i];

                    if(ticks_per_vblank[i]<mini) {
                        mini=ticks_per_vblank[i];
                    }

                    if(ticks_per_vblank[i]>maxi) {
                        maxi=ticks_per_vblank[i];
                    }

                    int ms=(int)(GetSecondsFromTicks(ticks_per_vblank[i])*1000.+.5);
                    TEST_TRUE(ms>=0);
                    if(ms>=NUM_BUCKETS-1) {
                        ms=NUM_BUCKETS-1;
                    }

                    ++buckets[ms];
                }

                printf("Avg: %.1fms\n",GetSecondsFromTicks(total)/ticks_per_vblank.size()*1000.);
                printf("Min: %.1fms\n",GetSecondsFromTicks(mini)*1000.);
                printf("Max: %.1fms\n",GetSecondsFromTicks(maxi)*1000.);
                for(size_t i=0;i<NUM_BUCKETS;++i) {
                    if(buckets[i]>0) {
                        printf("%s%zums: %zu\n",i==NUM_BUCKETS-1?">=":"~",i,buckets[i]);
                    }
                }
            }
        }
    }
}