#include <shared/system.h>
#include "load_save.h"
#include "misc.h"
#include <SDL.h>
#include <shared/log.h>
#include "Messages.h"
#include <shared/path.h>
#include <shared/system_specific.h>
#if SYSTEM_WINDOWS
#include <ShlObj.h>
#endif
#include <shared/file_io.h>
#include "load_save_config_rapidjson.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image_write.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX

// Forms a path from NSApplicationSupportDirectory, the bundle
// identifier, and PATH. Tries to create the resulting folder.
std::string GetOSXApplicationSupportPath(const std::string &path);

// Forms a path from NSApplicationCacheDirectory, the bundle
// identifier, and PATH. Tries to create the resulting folder.
std::string GetOSXCachePath(const std::string &path);

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(LOADSAVE, "config", "LD/SV ", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// If non-empty, use this as the folder to save config files in.
static std::string g_override_config_folder;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static std::string GetWindowsPath(const GUID &known_folder, const std::string &path) {
    std::string result;
    WCHAR *wpath;

    wpath = nullptr;
    if (FAILED(SHGetKnownFolderPath(known_folder, KF_FLAG_CREATE, nullptr, &wpath))) {
        goto bad;
    }

    result = GetUTF8String(wpath);

    CoTaskMemFree(wpath);
    wpath = NULL;

    if (result.empty()) {
        goto bad;
    }

    return PathJoined(result, "b2", path);

bad:;
    return path;
}
#endif

#if SYSTEM_LINUX
static std::string GetXDGPath(const char *env_name, const char *folder_name, const std::string &path) {
    std::string result;

    if (const char *env_value = getenv(env_name)) {
        result = env_value;
    } else if (const char *home = getenv("HOME")) {
        result = PathJoined(home, folder_name);
    } else {
        result = "."; // *sigh*
    }

    result = PathJoined(result, "b2", path);
    return result;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetConfigFolder(std::string folder) {
    g_override_config_folder = std::move(folder);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetConfigPath(const std::string &path) {
    if (!g_override_config_folder.empty()) {
        return PathJoined(g_override_config_folder, path);
    }

#if SYSTEM_WINDOWS

    // The config file includes a bunch of paths to local files, so it
    // should probably be Local rather than Roaming. But it's
    // debatable.
    return GetWindowsPath(FOLDERID_LocalAppData, path);

#elif SYSTEM_LINUX

    return GetXDGPath("XDG_CONFIG_HOME", ".config", path);

#elif SYSTEM_OSX

    return GetOSXApplicationSupportPath(path);

#else
#error
#endif
}

std::string GetCachePath(const std::string &path) {
#if SYSTEM_WINDOWS

    return GetWindowsPath(FOLDERID_LocalAppData, path);

#elif SYSTEM_LINUX

    return GetXDGPath("XDG_CACHE_HOME", ".cache", path);

#elif SYSTEM_OSX

    return GetOSXCachePath(path);

#else
#error
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetConfigFileName() {
    return GetConfigPath("b2.json");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static const char ASSETS_FOLDER[] = "assets";
#elif SYSTEM_LINUX
static const char ASSETS_FOLDER[] = "assets";
#elif SYSTEM_OSX
static const char ASSETS_FOLDER[] = "../Resources/assets";
#else
#error unknown system
#endif

#if SYSTEM_LINUX
static bool FindAssetLinux(std::string *result,
                           const std::string &prefix,
                           const std::string &suffix) {
    *result = PathJoined(prefix, suffix);
    LOGF(LOADSAVE, "Checking \"%s\": ", result->c_str());

    size_t size;
    bool can_write;
    if (!PathIsFileOnDisk(*result, &size, &can_write)) {
        // Whatever the reason, this path obviously ain't it.
        LOGF(LOADSAVE, "no.\n");
        return false;
    }

    LOGF(LOADSAVE, "yes.\n");
    return true;
}
#endif

static std::string GetAssetPathInternal(const std::string *f0, ...) {
#if SYSTEM_WINDOWS || SYSTEM_OSX

    // Look somewhere relative to the EXE.

    std::string path = PathJoined(PathGetFolder(PathGetEXEFileName()), ASSETS_FOLDER);

    va_list v;
    va_start(v, f0);

    for (const std::string *f = f0; f; f = va_arg(v, const std::string *)) {
        path = PathJoined(path, *f);
    }

    va_end(v);

    return path;

#elif SYSTEM_LINUX

    // Search in the following locations:
    //
    // 1. Relative to the EXE, as per Windows/OS X
    //
    // 2. If the EXE is in a folder called "bin", in ../share/b2 relative
    // to that
    //
    // 3. XDG_DATA_HOME (use "$HOME/.local/share" if not set)
    //
    // 4. XDG_DATA_DIRS (use "/usr/local/share/:/usr/share/ if not set)
    std::string suffix;
    {
        va_list v;
        va_start(v, f0);
        for (const std::string *f = f0; f; f = va_arg(v, const std::string *)) {
            suffix = PathJoined(suffix, *f);
        }

        va_end(v);
    }

    LOGF(LOADSAVE, "Searching for \"%s\": ", suffix.c_str());
    LOGI(LOADSAVE);

    std::string exe_folder = PathGetFolder(PathGetEXEFileName());

    std::string result;

    if (FindAssetLinux(&result, PathJoined(exe_folder, ASSETS_FOLDER), suffix)) {
        return result;
    }

    std::string exe_folder_name = PathGetName(PathWithoutTrailingSeparators(exe_folder));
    // LOGF(LOADSAVE,"EXE folder: %s\n",exe_folder.c_str());
    // LOGF(LOADSAVE,"EXE folder name: %s\n",exe_folder_name.c_str());
    if (exe_folder_name == "bin") {
        if (FindAssetLinux(&result, PathJoined(exe_folder, "../share/b2"), suffix)) {
            return result;
        }
    }

    if (FindAssetLinux(&result, GetXDGPath("XDG_DATA_HOME", ".local/share", ""), suffix)) {
        return result;
    }

    std::string xdg_data_dirs = "/usr/local/share/:/usr/share/";
    if (const char *xdg_data_dirs_env = getenv("XDG_DATA_DIRS")) {
        xdg_data_dirs = xdg_data_dirs_env;
    }

    std::vector<std::string> parts = GetSplitString(xdg_data_dirs, ":");
    for (const std::string &part : parts) {
        if (FindAssetLinux(&result, PathJoined(part, "b2"), suffix)) {
            return result;
        }
    }

    // In desparation...
    return suffix;

#else
#error
#endif
}

std::string GetAssetPath(const std::string &f0) {
    return GetAssetPathInternal(&f0, nullptr);
}

std::string GetAssetPath(const std::string &f0, const std::string &f1) {
    return GetAssetPathInternal(&f0, &f1, nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsR8G8B8(const SDL_PixelFormat *format) {
    if (format->Rmask != (0xff << 0)) {
        return false;
    }

    if (format->Gmask != (0xff << 8)) {
        return false;
    }

    if (format->Bmask != (0xff << 16)) {
        return false;
    }

    return true;
}

static unsigned char *SaveSDLSurface2(int *out_len, SDL_Surface *surface, Messages *messages) {
    SDL_SurfaceLocker locker(surface);

    if (!locker.IsLocked()) {
        messages->e.f("Failed to lock surface: %s\n", SDL_GetError());
        return nullptr;
    }

    if (surface->format->BytesPerPixel == 3 && IsR8G8B8(surface->format)) {
        return stbi_write_png_to_mem((unsigned char *)surface->pixels, surface->pitch, surface->w, surface->h, 3, out_len);
    } else if (surface->format->BytesPerPixel == 4 && IsR8G8B8(surface->format) && surface->format->Amask == (0xffu << 24)) {
        return stbi_write_png_to_mem((unsigned char *)surface->pixels, surface->pitch, surface->w, surface->h, 4, out_len);
    } else {
        // The stb PNG writer can accommodate any pitch. But since this is
        // rearranging the bytes, might as well flatten it at the same time.

        std::vector<unsigned char> file_pixels((size_t)surface->w * (size_t)surface->h * 4u);

        if (SDL_ConvertPixels(surface->w, surface->h, surface->format->format, surface->pixels, surface->pitch,
                              SDL_PIXELFORMAT_ABGR8888, file_pixels.data(), surface->w * 4) < 0) {
            messages->e.f("Failed to convert pixel data: %s\n", SDL_GetError());
            return nullptr;
        }

        return stbi_write_png_to_mem(file_pixels.data(), surface->w * 4, surface->w, surface->h, 4, out_len);
    }
}

bool SaveSDLSurface(SDL_Surface *surface, const std::string &path, Messages *messages) {
    int png_size;
    unsigned char *png = SaveSDLSurface2(&png_size, surface, messages);
    if (!png || png_size < 0) {
        free(png);
        return false;
    }

    bool good = SaveFile(png, (size_t)png_size, path, messages);

    free(png);
    png = nullptr;

    if (!good) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

unsigned char *SaveSDLSurfaceToPNGData(SDL_Surface *surface, size_t *png_size_out, Messages *messages) {
    int png_size;
    unsigned char *png = SaveSDLSurface2(&png_size, surface, messages);
    if (!png || png_size < 0) {
        free(png);
        return nullptr;
    }

    *png_size_out = (size_t)png_size;
    return png;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadGlobalConfig(Messages *messages) {
    bool good = LoadGlobalConfigRapidJSON(messages);
    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveGlobalConfig(Messages *messages) {
    bool good = SaveGlobalConfigRapidJSON(messages);
    return good;
}
