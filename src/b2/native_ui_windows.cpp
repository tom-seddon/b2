#include <shared/system.h>
#include <shared/debug.h>
#include <shared/system_specific.h>
#include <shared/mutex.h>
#include "native_ui.h"
#include "native_ui_windows.h"
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <SDL.h>
#include "misc.h"
#include "Messages.h"
#include "load_save.h"
#include <SDL_syswm.h>
#include "native_ui_private.h"
#include "b2.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static HWND GetHWNDForSDLWindow(SDL_Window *window) {
    if (window) {
        SDL_SysWMinfo wmi;
        SDL_VERSION(&wmi.version);
        SDL_GetWindowWMInfo(window, &wmi);

        return wmi.info.win.window;
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetClipboardImage(SDL_Surface *surface, Messages *messages) {
    SDL_SurfaceLocker locker(surface);
    if (!locker.IsLocked()) {
        messages->e.f("Failed to lock surface: %s\n", SDL_GetError());
        return;
    }

    BITMAPV5HEADER header = {};
    header.bV5Size = sizeof header;
    header.bV5Width = surface->w;
    header.bV5Height = surface->h;
    header.bV5Planes = 1;
    header.bV5BitCount = 24;
    header.bV5Compression = BI_RGB;

    std::vector<char> dibits_buffer;
    const void *dibits;
    if (surface->format->format == SDL_PIXELFORMAT_XRGB8888 && surface->pitch == surface->w * 4) {
        dibits = surface->pixels;
        // This is a top-down 32 bpp DI bitmap.
        header.bV5BitCount = 32;
        header.bV5Height = -header.bV5Height;
    } else if (surface->format->format == SDL_PIXELFORMAT_BGR24 &&
               surface->pitch == ((surface->w * 3 + 3) / 4 * 4)) {
        dibits = surface->pixels;
        // This is a top-down 24 bpp DI bitmap.
        header.bV5Height = -header.bV5Height;
    } else {
        size_t stride = (((header.bV5Width * header.bV5BitCount) + 31) & ~31) >> 3;
        dibits_buffer.resize(stride * header.bV5Height, 0);
        for (int y = 0; y < header.bV5Height; ++y) {
            // Top-down SDL surface.
            auto src = (const char *)surface->pixels + y * surface->pitch;

            // Form a bottom-up DI bitmap.
            auto dest = &dibits_buffer[(header.bV5Height - 1 - y) * stride];

            SDL_ConvertPixels(surface->w, 1, surface->format->format, src, surface->pitch, SDL_PIXELFORMAT_BGR24, dest, (int)stride);
        }
        dibits = dibits_buffer.data();
    }

    HDC screen_dc = nullptr;
    HBITMAP bitmap = nullptr;

    screen_dc = CreateDC("DISPLAY", nullptr, nullptr, nullptr);
    if (!screen_dc) {
        messages->e.f("CreateDC failed: %s\n", GetLastErrorDescription());
        goto done;
    }

    bitmap = CreateCompatibleBitmap(screen_dc, surface->w, surface->h);
    if (!bitmap) {
        messages->e.f("CreateCompatibleBitmap failed: size was %d x %d\n", surface->w, surface->h);
        goto done;
    }

    int n = SetDIBits(screen_dc, bitmap, 0, surface->h, dibits, (BITMAPINFO *)&header, DIB_RGB_COLORS);
    if (n != surface->h) {
        messages->e.f("SetDIBits failed: result was %d\n", n);
        goto done;
    }

    if (!OpenClipboard(nullptr)) {
        messages->e.f("OpenClipboard failed: %s\n", GetLastErrorDescription());
        goto done;
    }

    if (!EmptyClipboard()) {
        messages->e.f("EmptyClipboard failed: %s\n", GetLastErrorDescription());
        goto done;
    }

    if (!SetClipboardData(CF_BITMAP, bitmap)) {
        messages->e.f("SetClipboardData failed: %s\n", GetLastErrorDescription());
        goto done;
    }

    CloseClipboard();

done:
    if (bitmap) {
        DeleteObject(bitmap);
        bitmap = nullptr;
    }

    if (screen_dc) {
        DeleteDC(screen_dc);
        screen_dc = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CommonItemDialogFilterSpecs {
    std::vector<std::wstring> name_strings, spec_strings;
    std::vector<COMDLG_FILTERSPEC> filter_specs;
};

static void GetCommonItemDialogFilterSpecsForFilters(CommonItemDialogFilterSpecs *specs, const std::vector<FileDialog::Filter> &filters) {
    ASSERT(filters.size() <= UINT_MAX);

    for (const OpenFileDialog::Filter &filter : filters) {
        std::wstring spec;
        for (size_t i = 0; i < filter.extensions.size(); ++i) {
            if (i > 0) {
                spec += L";";
            }

            spec += L"*" + GetWideString(filter.extensions[i]);
        }

        specs->name_strings.push_back(GetWideString(filter.title) + L" (" + spec + L")");
        specs->spec_strings.push_back(spec);
    }

    ASSERT(specs->name_strings.size() == filters.size());
    ASSERT(specs->name_strings.size() == specs->spec_strings.size());

    for (size_t i = 0; i < specs->name_strings.size(); ++i) {
        COMDLG_FILTERSPEC filter_spec;

        filter_spec.pszName = specs->name_strings[i].c_str();
        filter_spec.pszSpec = specs->spec_strings[i].c_str();

        specs->filter_specs.push_back(filter_spec);
    }

    ASSERT(specs->filter_specs.size() == specs->name_strings.size());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static CComPtr<IOleWindow> g_current_modal_window;

bool CloseModalDialogLocked() {
    ASSERT(!IsMainThread());
    ASSERT(g_native_ui_modal_state == NativeUiModalState_Open);

    if (!g_current_modal_window) {
        // no IOleWindow! Stuck!
        return false;
    }

    // IFileDialog::Close doesn't seem to work from a background thread, hence
    // all this HWND stuff.
    //
    // The docs don't say whether IOleWindow::GetWindow is any safer to use from
    // a background thread, but it does at least actually seem to work, so what
    // could possibly go wrong?
    HWND hwnd;
    if (FAILED(g_current_modal_window->GetWindow(&hwnd))) {
        // there's one open, but no HWND for it, so no.
        return false;
    }

    PostMessage(hwnd, WM_CLOSE, 0, 0);

    g_current_modal_window = nullptr;

    // Fingers crossed, the window will become closed in due course.
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static HRESULT InitAndShowFileDialog(const std::vector<FileDialog::Filter> &filters, SDL_Window *parent, IFileDialog *dialog) {
    DWORD flags;
    dialog->GetOptions(&flags);
    dialog->SetOptions(flags | FOS_FORCEFILESYSTEM);

    CommonItemDialogFilterSpecs specs;
    GetCommonItemDialogFilterSpecsForFilters(&specs, filters);
    dialog->SetFileTypes((UINT)specs.filter_specs.size(), specs.filter_specs.data());

    {
        LockGuard lock(g_native_ui_globals_mutex);

        g_native_ui_modal_state = NativeUiModalState_Open;

        if (FAILED(dialog->QueryInterface(IID_PPV_ARGS(&g_current_modal_window)))) {
            g_current_modal_window = nullptr; //be extra sure to reset it...
        }
    }

    HRESULT hr = dialog->Show(GetHWNDForSDLWindow(parent));

    {
        LockGuard lock(g_native_ui_globals_mutex);

        g_native_ui_modal_state = NativeUiModalState_NotOpen;
        g_current_modal_window = nullptr;
    }

    return hr;
}

static std::string GetShellItemPath(IShellItem *item) {
    std::wstring path;
    PWSTR path_tmp;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path_tmp))) {
        path = path_tmp;
        CoTaskMemFree(path_tmp), path_tmp = nullptr;
    }

    std::string path_utf8 = GetUTF8String(path);
    return path_utf8;
}

std::string OpenFileDialogWindows(SDL_Window *parent,
                                  const std::vector<FileDialog::Filter> &filters,
                                  const std::string &default_path) {
    CComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return "";
    }

    if (FAILED(InitAndShowFileDialog(filters, parent, dialog.p))) {
        return "";
    }

    CComPtr<IShellItemArray> results;
    if (FAILED(dialog->GetResults(&results))) {
        return "";
    }

    DWORD num_results;
    results->GetCount(&num_results);
    if (num_results < 1) {
        return "";
    }

    CComPtr<IShellItem> result;
    results->GetItemAt(0, &result);

    std::string path = GetShellItemPath(result.p);
    return path;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialogWindows(SDL_Window *parent,
                                  const std::vector<OpenFileDialog::Filter> &filters,
                                  const std::string &default_path) {

    CComPtr<IFileSaveDialog> dialog;

    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return "";
    }

    if (FAILED(InitAndShowFileDialog(filters, parent, dialog.p))) {
        return "";
    }

    CComPtr<IShellItem> result;
    if (FAILED(dialog->GetResult(&result))) {
        return "";
    }

    std::string path = GetShellItemPath(result);
    return path;
}

// (old notes regarding GetSaveFileNameW)

//// Not only is lpstrDefExt prety restricted, but it doesn't even
//// appear to work in any useful fashion :( - GetSaveFileName is
//// supposed to append the extension if it doesn't exist, but that
//// doesn't actually appear to happen...
////
//// (The extension is appended manually later, so it does work if
//// you just type in a name and no extension. But this sucks,
//// because you don't get the "File exists" message box if the
//// name+extension does actually exist.)

//if (!filters.empty()) {
//    default_ext = filters[0].extensions[0];

//    got_default_ext = true;

//    for (size_t i = 0; i < filters.size(); ++i) {
//        if (filters[i].extensions.size() != 1) {
//            got_default_ext = false;
//            break;
//        }

//        // Ignore the all files wildcard.
//        if (filters[i].extensions[0] == ".*") {
//            continue;
//        }

//        if (i > 0 && filters[i].extensions[0] != filters[i - 1].extensions[0]) {
//            got_default_ext = false;
//            break;
//        }
//    }

//    if (got_default_ext && default_ext.size() >= 1 && default_ext.size() <= 4 && default_ext[0] == '.') {
//        default_ext = default_ext.substr(1);
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
