#include <shared/system.h>
#include <shared/debug.h>
#include <shared/system_specific.h>
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

static std::wstring GetFiltersWin32(const std::vector<OpenFileDialog::Filter> &filters) {
    std::wstring result;

    for (const OpenFileDialog::Filter &filter : filters) {
        std::string extensions;

        for (size_t i = 0; i < filter.extensions.size(); ++i) {
            if (i > 0) {
                extensions += ";";
            }

            extensions += "*" + filter.extensions[i];
        }

        result += GetWideString(filter.title + " (" + extensions + ")");
        result.push_back(0);
        result += GetWideString(extensions);
        result.push_back(0);
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string DoFileDialogWindows(const std::vector<OpenFileDialog::Filter> &filters,
                                       const std::string &default_path,
                                       DWORD flags,
                                       const std::wstring &default_ext,
                                       BOOL(APIENTRY *fn)(LPOPENFILENAMEW)) {
    std::wstring filters_win32 = GetFiltersWin32(filters);
    std::wstring wdefault_path = GetWideString(default_path);

    OPENFILENAMEW ofn{};
    wchar_t file_name[MAX_PATH]{};

    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = sizeof file_name;
    ofn.lpstrFilter = filters_win32.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = wdefault_path.empty() ? nullptr : wdefault_path.c_str();
    ofn.Flags = flags;
    ofn.lpstrDefExt = default_ext.empty() ? nullptr : default_ext.c_str();

    int ret = (*fn)(&ofn);
    if (ret == 0) {
        return "";
    } else {
        return GetUTF8String(file_name);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OpenFileDialogWindows(const std::vector<OpenFileDialog::Filter> &filters,
                                  const std::string &default_path) {
    return DoFileDialogWindows(filters,
                               default_path,
                               OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST,
                               L"",
                               &GetOpenFileNameW);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialogWindows(const std::vector<OpenFileDialog::Filter> &filters,
                                  const std::string &default_path) {
    std::string default_ext;
    bool got_default_ext = false;

    // Not only is lpstrDefExt prety restricted, but it doesn't even
    // appear to work in any useful fashion :( - GetSaveFileName is
    // supposed to append the extension if it doesn't exist, but that
    // doesn't actually appear to happen...
    //
    // (The extension is appended manually later, so it does work if
    // you just type in a name and no extension. But this sucks,
    // because you don't get the "File exists" message box if the
    // name+extension does actually exist.)

    if (!filters.empty()) {
        default_ext = filters[0].extensions[0];

        got_default_ext = true;

        for (size_t i = 0; i < filters.size(); ++i) {
            if (filters[i].extensions.size() != 1) {
                got_default_ext = false;
                break;
            }

            // Ignore the all files wildcard.
            if (filters[i].extensions[0] == ".*") {
                continue;
            }

            if (i > 0 && filters[i].extensions[0] != filters[i - 1].extensions[0]) {
                got_default_ext = false;
                break;
            }
        }

        if (got_default_ext && default_ext.size() >= 1 && default_ext.size() <= 4 && default_ext[0] == '.') {
            default_ext = default_ext.substr(1);
        }
    }

    return DoFileDialogWindows(filters,
                               default_path,
                               OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
                               got_default_ext ? GetWideString(default_ext) : L"",
                               &GetSaveFileNameW);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SelectFolderDialogWindows(const std::string &default_path) {
    CComPtr<IFileDialog> f;

    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog,
                                nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_IFileDialog,
                                (void **)&f))) {
        return "";
    }

    DWORD options;
    f->GetOptions(&options);
    f->SetOptions(options | FOS_PICKFOLDERS);

    if (!default_path.empty()) {
        CComPtr<IShellItem> default_path_item;

        std::wstring wdefault_path = GetWideString(default_path);
        if (!wdefault_path.empty()) {
            if (SUCCEEDED(SHCreateItemFromParsingName(wdefault_path.c_str(),
                                                      nullptr,
                                                      IID_IShellItem,
                                                      (void **)&default_path_item))) {
                f->SetFolder(default_path_item);
            }
        }
    }

    if (FAILED(f->Show(nullptr))) {
        return "";
    }

    CComPtr<IShellItem> item;
    if (FAILED(f->GetResult(&item))) {
        return "";
    }

    WCHAR *item_wname;
    if (FAILED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &item_wname))) {
        return "";
    }

    std::string result_utf8 = GetUTF8String(item_wname);

    CoTaskMemFree(item_wname);
    item_wname = nullptr;

    return result_utf8;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
