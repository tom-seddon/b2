#include <shared/system.h>
#include <shared/debug.h>
#include <shared/system_specific.h>
#include <urlmon.h>
#include "download.h"
#include <vector>
#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Download {
    char *error = nullptr;
    std::string url;
    std::vector<char> fname;
    IBindStatusCallback *bsc = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BindStatusCallback : public IBindStatusCallback {
  public:
    HRESULT hr = E_FAIL;
    std::wstring error;

    explicit BindStatusCallback(const char *url)
        : m_url(url) {
    }

    ~BindStatusCallback() {
    }

    HRESULT STDMETHODCALLTYPE OnStartBinding(
        /* [in] */ DWORD dwReserved,
        /* [in] */ __RPC__in_opt IBinding *pib) {
        (void)dwReserved, (void)pib;

        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetPriority(
        /* [out] */ __RPC__out LONG *pnPriority) {
        (void)pnPriority;

        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnLowResource(
        /* [in] */ DWORD reserved) {
        (void)reserved;

        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnProgress(
        /* [in] */ ULONG ulProgress,
        /* [in] */ ULONG ulProgressMax,
        /* [in] */ ULONG ulStatusCode,
        /* [unique][in] */ __RPC__in_opt LPCWSTR szStatusText) {
        (void)ulStatusCode, (void)szStatusText;

        int percent = 0;
        if (ulProgressMax != 0) {
            percent = (int)((double)ulProgress / ulProgressMax * 100.0);
        }

        if (percent != m_old_percent) {
            printf("%s: %03d%%\r", m_url.c_str(), percent);
            m_old_percent = percent;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnStopBinding(
        /* [in] */ HRESULT hresult,
        /* [unique][in] */ __RPC__in_opt LPCWSTR szError) {
        (void)hresult, (void)szError;

        this->hr = hresult;

        if (szError)
            this->error = szError;

        printf("\n");

        return S_OK;
    }

    /* [local] */ HRESULT STDMETHODCALLTYPE GetBindInfo(
        /* [out] */ DWORD *grfBINDF,
        /* [unique][out][in] */ BINDINFO *pbindinfo) {
        (void)grfBINDF, (void)pbindinfo;

        return E_NOTIMPL;
    }

    /* [local] */ HRESULT STDMETHODCALLTYPE OnDataAvailable(
        /* [in] */ DWORD grfBSCF,
        /* [in] */ DWORD dwSize,
        /* [in] */ FORMATETC *pformatetc,
        /* [in] */ STGMEDIUM *pstgmed) {
        (void)grfBSCF, (void)dwSize, (void)pformatetc, (void)pstgmed;

        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnObjectAvailable(
        /* [in] */ __RPC__in REFIID riid,
        /* [iid_is][in] */ __RPC__in_opt IUnknown *punk) {
        (void)riid, (void)punk;

        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) {
        if (!ppv) {
            return E_INVALIDARG;
        }

        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IBindStatusCallback) {
            *ppv = this;
            this->AddRef();
            return NOERROR;
        }

        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() {
        ULONG n = InterlockedIncrement(&m_numRefs);
        return n;
    }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = InterlockedDecrement(&m_numRefs);
        if (n == 0) {
            delete this;
        }

        return n;
    }

  protected:
  private:
    ULONG m_numRefs = 0;
    int m_old_percent = -1;
    std::string m_url;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int DownloadFile(char **result, const char *url, int force_download) {
    (void)force_download; //TODO: is this ever going to be used???

    auto callback = new BindStatusCallback(url);
    callback->AddRef();

    std::vector<char> fname(1000);
    HRESULT hr = URLDownloadToCacheFileA(nullptr, url, fname.data(), (DWORD)fname.size(), 0, callback);

    callback->Release();
    callback = nullptr;

    if (FAILED(hr)) {
        asprintf(result, "URLDownloadToCacheFileA failed: %s", GetErrorDescription(hr));
        return -1;
    }

    *result = _strdup(fname.data());
    return 0;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
