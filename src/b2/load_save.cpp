#include <shared/system.h>
#include "load_save.h"
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include "misc.h"
#include "native_ui.h"
#include <rapidjson/error/en.h>
#include "keymap.h"
#include "keys.h"
#include <SDL.h>
#include "BeebWindows.h"
#include <shared/log.h>
#include <stdio.h>
#include "Messages.h"
#include <shared/debug.h>
#include <shared/path.h>
#include <beeb/DiscInterface.h>
#include "TraceUI.h"
#include "BeebWindow.h"
#include <shared/system_specific.h>
#if SYSTEM_WINDOWS
#include <ShlObj.h>
#endif
#include <beeb/BBCMicro.h>
#include "b2.h"
#include "BeebLinkHTTPHandler.h"
#include "joysticks.h"
#include <shared/file_io.h>

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

// (sigh)
#define PRIsizetype "u"
CHECK_SIZEOF(rapidjson::SizeType, sizeof(unsigned));

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

template <class T>
using JSONWriter = rapidjson::PrettyWriter<T>;

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

static std::string GetConfigFileName() {
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

static int GetHexCharValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return -1;
    }
}

static std::string GetHexStringFromData(const std::vector<uint8_t> &data) {
    std::string hex;

    hex.reserve(data.size() * 2);

    for (uint8_t byte : data) {
        hex.append(1, HEX_CHARS_LC[byte >> 4]);
        hex.append(1, HEX_CHARS_LC[byte & 15]);
    }

    return hex;
}

// TODO - should really clear the vector out if there's an error...
static bool GetDataFromHexString(std::vector<uint8_t> *data, const std::string &str) {
    if (str.size() % 2 != 0) {
        return false;
    }

    data->reserve(str.size() / 2);

    for (size_t i = 0; i < str.size(); i += 2) {
        int a = GetHexCharValue(str[i + 0]);
        if (a < 0) {
            return false;
        }

        int b = GetHexCharValue(str[i + 1]);
        if (b < 0) {
            return false;
        }

        data->push_back((uint8_t)a << 4 | (uint8_t)b);
    }

    return true;
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

// RapidJSON-friendly stream class that writes its output into a
// std::string.

class StringStream {
  public:
    typedef std::string::value_type Ch;

    StringStream(std::string *str);

    void Put(char c);
    void Flush();

  protected:
  private:
    std::string *m_str;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

StringStream::StringStream(std::string *str)
    : m_str(str) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void StringStream::Put(char c) {
    m_str->push_back(c);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void StringStream::Flush() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindMember(rapidjson::Value *member, rapidjson::Value *object, const char *key, Messages *msg, bool (rapidjson::Value::*is_type_mfn)() const, const char *type_name) {
    rapidjson::Document::MemberIterator it = object->FindMember(key);
    if (it == object->MemberEnd()) {
        if (msg) {
            // It doesn't hurt to print this somewhere; it just
            // doesn't want to pop up as part of the UI.
            //
            //msg->w.f("%s not found: %s\n",type_name,key);
            LOGF(LOADSAVE, "%s not found: %s\n", type_name, key);
        }

        return false;
    }

    if (!(it->value.*is_type_mfn)()) {
        if (msg) {
            msg->w.f("not %s: %s\n", type_name, key);
        }

        return false;
    }

    *member = it->value;
    return true;
}

static bool FindArrayMember(rapidjson::Value *arr, rapidjson::Value *src, const char *key, Messages *msg) {
    return FindMember(arr, src, key, msg, &rapidjson::Value::IsArray, "array");
}

static bool FindObjectMember(rapidjson::Value *obj, rapidjson::Value *src, const char *key, Messages *msg) {
    return FindMember(obj, src, key, msg, &rapidjson::Value::IsObject, "object");
}

//static bool FindStringMember(rapidjson::Value *value,rapidjson::Value *object,const char *key,Messages *msg) {
//    return FindMember(value,object,key,msg,&rapidjson::Value::IsString,"string");
//}

static bool FindStringMember(std::string *value, rapidjson::Value *object, const char *key, Messages *msg) {
    rapidjson::Value tmp;
    if (!FindMember(&tmp, object, key, msg, &rapidjson::Value::IsString, "string")) {
        return false;
    }

    *value = tmp.GetString();
    return true;
}

static bool FindBoolMember(bool *value, rapidjson::Value *object, const char *key, Messages *msg) {
    rapidjson::Value tmp;
    if (!FindMember(&tmp, object, key, msg, &rapidjson::Value::IsBool, "bool")) {
        return false;
    }

    *value = tmp.GetBool();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static bool FindDoubleMember(double *value,rapidjson::Value *object,const char *key,Messages *msg) {
//    rapidjson::Value tmp;
//    if(!FindMember(&tmp,object,key,msg,&rapidjson::Value::IsNumber,"number")) {
//        return false;
//    }
//
//    *value=tmp.GetDouble();
//    return true;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindFloatMember(float *value, rapidjson::Value *object, const char *key, Messages *msg) {
    rapidjson::Value tmp;
    if (!FindMember(&tmp, object, key, msg, &rapidjson::Value::IsNumber, "number")) {
        return false;
    }

    *value = tmp.GetFloat();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindUInt64Member(uint64_t *value, rapidjson::Value *object, const char *key, Messages *msg) {
    rapidjson::Value tmp;
    if (!FindMember(&tmp, object, key, msg, &rapidjson::Value::IsNumber, "number")) {
        return false;
    }

    *value = tmp.GetUint64();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindUIntMember(unsigned *value, rapidjson::Value *object, const char *key, Messages *msg) {
    rapidjson::Value tmp;
    if (!FindMember(&tmp, object, key, msg, &rapidjson::Value::IsNumber, "number")) {
        return false;
    }

    *value = tmp.GetUint();
    return true;
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindUInt16Member(uint16_t *value, rapidjson::Value *object, const char *key, Messages *msg) {
    uint64_t tmp;
    if (!FindUInt64Member(&tmp, object, key, msg)) {
        return false;
    }

    *value = (uint16_t)tmp;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindUInt8Member(uint8_t *value, rapidjson::Value *object, const char *key, Messages *msg) {
    uint64_t tmp;
    if (!FindUInt64Member(&tmp, object, key, msg)) {
        return false;
    }

    *value = (uint8_t)tmp;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Bit-indexed flags are flags where the enum values relate to the bit
// indexes rather than the mask values.
//
// This is indeed some crappy naming.

template <class T>
static void SaveBitIndexedFlags(JSONWriter<StringStream> *writer, T flags, const char *(*get_name_fn)(int)) {
    for (int i = 0; i < (int)(sizeof(T) * CHAR_BIT); ++i) {
        const char *name = (*get_name_fn)(i);
        if (name[0] == '?') {
            continue;
        }

        if (!(flags & (T)1 << i)) {
            continue;
        }

        writer->String(name);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static bool FindBitIndexedFlagsMember(T *flags, rapidjson::Value *object, const char *key, const char *what, const char *(*get_name_fn)(int), Messages *msg) {
    rapidjson::Value array;
    if (!FindArrayMember(&array, object, key, msg)) {
        return false;
    }

    bool good = true;
    *flags = 0;

    for (rapidjson::SizeType i = 0; i < array.Size(); ++i) {
        if (array[i].IsString()) {
            bool found = false;
            const char *bit_name = array[i].GetString();

            for (int j = 0; j < (int)(sizeof(T) * CHAR_BIT); ++j) {
                const char *name = (*get_name_fn)(j);
                if (name[0] == '?') {
                    continue;
                }

                if (strcmp(bit_name, name) == 0) {
                    found = true;
                    *flags |= (T)1 << j;
                    break;
                }
            }

            if (!found) {
                msg->e.f("unknown %s: %s\n", what, bit_name);
                good = false;
            }
        }
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveFlags(JSONWriter<StringStream> *writer, uint32_t flags, const char *(*get_name_fn)(uint32_t)) {
    for (uint32_t mask = 1; mask != 0; mask <<= 1) {
        const char *name = (*get_name_fn)(mask);
        if (name[0] == '?') {
            continue;
        }

        if (!(flags & mask)) {
            continue;
        }

        writer->String(name);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindFlagsMember(uint32_t *flags, rapidjson::Value *object, const char *key, const char *what, const char *(*get_name_fn)(uint32_t), Messages *msg) {
    rapidjson::Value array;
    if (!FindArrayMember(&array, object, key, msg)) {
        return false;
    }

    bool good = true;
    *flags = 0;

    for (rapidjson::SizeType i = 0; i < array.Size(); ++i) {
        if (array[i].IsString()) {
            bool found = false;
            const char *flag_name = array[i].GetString();

            for (uint32_t mask = 1; mask != 0; mask <<= 1) {
                const char *name = (*get_name_fn)(mask);
                if (name[0] == '?') {
                    continue;
                }

                if (strcmp(flag_name, name) == 0) {
                    found = true;
                    *flags |= mask;
                    break;
                }
            }

            if (!found) {
                msg->e.f("unknown %s: %s\n", what, flag_name);
                good = false;
            }
        }
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class EnumType, class EnumBaseType>
static void SaveEnum(JSONWriter<StringStream> *writer, EnumType value, const char *(*get_name_fn)(EnumBaseType)) {
    const char *name = (*get_name_fn)(value);
    if (name[0] != '?') {
        writer->String(name);
    } else {
        // Have to save something...
        writer->String("?");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// To be suitable for use with this function, thie enum values must
// start from 0 and be contiguous.
template <class EnumType, class EnumBaseType>
static bool LoadEnum(EnumType *value, const std::string &str, const char *what, const char *(*get_name_fn)(EnumBaseType), Messages *msg) {
    EnumBaseType i = 0;
    for (;;) {
        const char *name = (*get_name_fn)(i);
        if (name[0] == '?') {
            break;
        }

        if (str == name) {
            *value = (EnumType)i;
            return true;
        }

        ++i;
    }

    msg->e.f("unknown %s: %s\n", what, str.c_str());
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class EnumType, class EnumBaseType>
static bool FindEnumMember(EnumType *value, rapidjson::Value *object, const char *key, const char *what, const char *(*get_name_fn)(EnumBaseType), Messages *msg) {
    std::string str;
    if (!FindStringMember(&str, object, key, msg)) {
        return false;
    }

    if (!LoadEnum(value, str, what, get_name_fn, msg)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::unique_ptr<rapidjson::Document> LoadDocument(
    std::vector<char> *data,
    Messages *msg) {
    std::unique_ptr<rapidjson::Document> doc = std::make_unique<rapidjson::Document>();

    doc->ParseInsitu(data->data());

    if (doc->HasParseError()) {
        msg->e.f("JSON error: +%zu: %s\n",
                 doc->GetErrorOffset(),
                 rapidjson::GetParseError_En(doc->GetParseError()));
        return nullptr;
    }

    return doc;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct WriterHelper {
    bool valid = true;
    std::function<void(void)> fun;

    WriterHelper(std::function<void(void)> fun_)
        : fun(fun_) {
    }

    WriterHelper(WriterHelper &&oth) noexcept
        : valid(oth.valid)
        , fun(oth.fun) {
        oth.valid = false;
    }

    ~WriterHelper() {
        if (this->valid) {
            this->fun();
        }
    }
};

template <class WriterType>
static WriterHelper DoWriter(WriterType *writer,
                             const char *name,
                             bool (WriterType::*begin_mfn)(),
                             bool (WriterType::*end_mfn)(rapidjson::SizeType)) {
    if (name) {
        writer->Key(name);
    }

    (writer->*begin_mfn)();

    return WriterHelper(
        [writer, end_mfn]() {
            (writer->*end_mfn)(0);
        });
}

template <class WriterType>
static WriterHelper ObjectWriter(WriterType *writer,
                                 const char *name = nullptr) {
    return DoWriter(writer, name, &WriterType::StartObject, &WriterType::EndObject);
}

template <class WriterType>
static WriterHelper ArrayWriter(WriterType *writer,
                                const char *name = nullptr) {
    return DoWriter(writer, name, &WriterType::StartArray, &WriterType::EndArray);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char RECENT_PATHS[] = "recent_paths";
static const char PATHS[] = "paths";
static const char OLD_KEYMAPS[] = "keymaps";
static const char NEW_KEYMAPS[] = "new_keymaps";
static const char KEYS[] = "keys";
static const char PLACEMENT[] = "window_placement";
static const char OLD_CONFIGS[] = "configs";
static const char NEW_CONFIGS[] = "new_configs";
static const char OS[] = "os";
static const char TYPE[] = "type";
static const char ROMS[] = "roms";
static const char WRITEABLE[] = "writeable";
static const char FILE_NAME[] = "file_name";
static const char NAME[] = "name";
static const char DISC_INTERFACE[] = "disc_interface";
static const char TRACE[] = "trace";
static const char FLAGS[] = "flags";
static const char START[] = "start";
static const char STOP[] = "stop";
static const char WINDOWS[] = "windows";
static const char KEYMAP[] = "keymap";
static const char KEYSYMS[] = "keysyms";
static const char KEYCODE[] = "keycode";
static const char BBC_VOLUME[] = "bbc_volume";
static const char DISC_VOLUME[] = "disc_volume";
static const char FILTER_BBC[] = "filter_bbc";
static const char SHORTCUTS[] = "shortcuts";
static const char PREFER_SHORTCUTS[] = "prefer_shortcuts";
//static const char DOCK_CONFIG[]="dock_config";
static const char CORRECT_ASPECT_RATIO[] = "correct_aspect_ratio";
static const char AUTO_SCALE[] = "auto_scale";
static const char MANUAL_SCALE[] = "manual_scale";
static const char POPUPS[] = "popups";
static const char GLOBALS[] = "globals";
static const char VSYNC[] = "vsync";
static const char EXT_MEM[] = "ext_mem";
static const char UNLIMITED[] = "unlimited";
static const char BEEBLINK[] = "beeblink";
static const char URLS[] = "urls";
static const char NVRAM[] = "nvram";
static const char START_INSTRUCTION_ADDRESS[] = "start_address"; //yes, inconsistent naming...
static const char START_WRITE_ADDRESS[] = "start_write_address";
static const char STOP_WRITE_ADDRESS[] = "stop_write_address";
static const char STOP_NUM_CYCLES[] = "stop_num_cycles";
static const char OUTPUT_FLAGS[] = "output_flags";
static const char POWER_ON_TONE[] = "power_on_tone";
static const char STANDARD_ROM[] = "standard_rom";
static const char CONFIG[] = "config";
static const char INTERLACE[] = "interlace";
static const char LEDS_POPUP_MODE[] = "leds_popup_mode";
static const char PARASITE[] = "parasite";
static const char PARASITE_OS[] = "parasite_os";
static const char NVRAM_TYPE[] = "nvram_type";
static const char AUTO_SAVE[] = "auto_save";
static const char AUTO_SAVE_PATH[] = "auto_save_path";
#if SYSTEM_WINDOWS
static const char UNIX_LINE_ENDINGS[] = "unix_line_endings";
#endif
static const char FEATURE_FLAGS[] = "feature_flags";
static const char PARASITE_TYPE[] = "parasite_type";
static const char JOYSTICKS[] = "joysticks";
static const char DEVICE_NAMES[] = "device_names";
static const char GUI_FONT_SIZE[] = "gui_font_size";
static const char SCREENSHOT_FILTER[] = "screenshot_filter";
static const char SCREENSHOT_CORRECT_ASPECT_RATIO[] = "screenshot_correct_aspect_ratio";
static const char SCREENSHOT_LAST_VSYNC[] = "screenshot_last_vsync";
static const char SWAP_JOYSTICKS_WHEN_SHARED[] = "swap_joysticks_when_shared";
#if ENABLE_SDL_FULL_SCREEN
static const char FULL_SCREEN[] = "full_screen";
#endif
static const char ADJI[] = "adji";
static const char ADJI_DIP_SWITCHES[] = "adji_dip_switches";
static const char TEXT_UTF8_CONVERT_MODE[] = "text_utf8_convert_mode";
static const char PRINTER_UTF8_CONVERT_MODE[] = "printer_utf8_convert_mode";
static const char TEXT_HANDLE_DELETE[] = "text_handle_delete";
static const char PRINTER_HANDLE_DELETE[] = "printer_handle_delete";
static const char MOUSE[] = "mouse";
static const char CAPTURE_MOUSE_ON_CLICK[] = "capture_mouse_on_click";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadKeycodeFromObject(uint32_t *keycode, rapidjson::Value *keycode_json, Messages *msg, const char *name_fmt, ...) {
    if (!keycode_json->IsObject()) {
        msg->e.f("not an object: ");

        va_list v;
        va_start(v, name_fmt);
        msg->e.v(name_fmt, v);
        va_end(v);

        msg->e.f("\n");
        return false;
    }

    std::string keycode_name;
    if (!FindStringMember(&keycode_name, keycode_json, KEYCODE, nullptr)) {
        *keycode = 0;
        return true;
    }

    *keycode = (uint32_t)SDL_GetKeyFromName(keycode_name.c_str());
    if (*keycode == 0) {
        msg->w.f("unknown keycode: %s\n", keycode_name.c_str());
        return false;
    }

    for (uint32_t mask = PCKeyModifier_Begin; mask != PCKeyModifier_End; mask <<= 1) {
        bool value;
        if (FindBoolMember(&value, keycode_json, GetPCKeyModifierEnumName((int)mask), nullptr)) {
            if (value) {
                *keycode |= mask;
            }
        }
    }

    return true;
}

static void SaveKeycodeObject(JSONWriter<StringStream> *writer, uint32_t keycode) {
    auto keycode_json = ObjectWriter(writer);

    if ((keycode & ~PCKeyModifier_All) != 0) {
        for (uint32_t mask = PCKeyModifier_Begin; mask != PCKeyModifier_End; mask <<= 1) {
            if (keycode & mask) {
                writer->Key(GetPCKeyModifierEnumName((int)mask));
                writer->Bool(true);
            }
        }

        writer->Key(KEYCODE);

        uint32_t sdl_keycode = keycode & ~PCKeyModifier_All;
        const char *keycode_name = SDL_GetKeyName((SDL_Keycode)sdl_keycode);
        if (strlen(keycode_name) == 0) {
            writer->Int64((int64_t)sdl_keycode);
        } else {
            writer->String(keycode_name);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddDefaultBeebKeymaps() {
    for (size_t i = 0; i < GetNumDefaultBeebKeymaps(); ++i) {
        BeebWindows::AddBeebKeymap(*GetDefaultBeebKeymapByIndex(i));
    }
}

static void AddDefaultBeebConfigs(bool force) {
    const uint32_t old_features_seen = BeebWindows::GetBeebConfigFeatureFlags();
    uint32_t new_features_seen = old_features_seen;

    for (size_t i = 0; i < GetNumDefaultBeebConfigs(); ++i) {
        const BeebConfig *config = GetDefaultBeebConfigByIndex(i);
        if (force || config->feature_flags != 0) {
            if (force || (old_features_seen & config->feature_flags) == 0) {
                BeebWindows::AddConfig(*config);
                new_features_seen |= config->feature_flags;
            }
        }
    }

    BeebWindows::SetBeebConfigFeatureFlags(new_features_seen);
}

static void EnsureDefaultBeebKeymapsAvailable() {
    if (BeebWindows::GetNumBeebKeymaps() == 0) {
        AddDefaultBeebKeymaps();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadGlobals(rapidjson::Value *globals,
                        Messages *msg) {
    (void)msg;

    FindBoolMember(&g_option_vsync, globals, VSYNC, nullptr);

    uint32_t feature_flags = 0;
    if (FindFlagsMember(&feature_flags, globals, FEATURE_FLAGS, "feature flag", &GetBeebConfigFeatureFlagEnumName, msg)) {
        BeebWindows::SetBeebConfigFeatureFlags(feature_flags);
    }

    return true;
}

static void SaveGlobals(JSONWriter<StringStream> *writer) {
    auto globals_json = ObjectWriter(writer, GLOBALS);
    {
        writer->Key(VSYNC);
        writer->Bool(g_option_vsync);

        {
            auto feature_flags_json = ArrayWriter(writer, FEATURE_FLAGS);

            uint32_t feature_flags = BeebWindows::GetBeebConfigFeatureFlags();
            SaveFlags(writer, feature_flags, &GetBeebConfigFeatureFlagEnumName);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadRecentPaths(rapidjson::Value *recent_paths,
                            Messages *msg) {
    for (rapidjson::Value::MemberIterator it = recent_paths->MemberBegin();
         it != recent_paths->MemberEnd();
         ++it) {
        std::string tag = it->name.GetString();

        LOGF(LOADSAVE, "Loading recent paths for: %s\n", tag.c_str());

        rapidjson::Value paths;
        if (!FindArrayMember(&paths, &it->value, PATHS, nullptr)) {
            LOGF(LOADSAVE, "(no data found.)\n");
            return false;
        }

        RecentPaths recents;

        for (rapidjson::SizeType json_index = 0; json_index < paths.Size(); ++json_index) {
            rapidjson::SizeType i = paths.Size() - 1 - json_index;

            if (!paths[i].IsString()) {
                msg->e.f("not a string: %s.%s.paths[%u]\n",
                         RECENT_PATHS, tag.c_str(), i);
                return false;
            }

            LOGF(LOADSAVE, "    %" PRIsizetype ". %s\n", i, paths[i].GetString());
            recents.AddPath(paths[i].GetString());
        }

        SetRecentPathsByTag(std::move(tag), std::move(recents));
    }

    return true;
}

static void SaveRecentPaths(JSONWriter<StringStream> *writer) {
    auto recent_paths_json = ObjectWriter(writer, RECENT_PATHS);

    ForEachRecentPaths(
        [writer](const std::string &tag,
                 const RecentPaths &recents) {
            auto tag_json = ObjectWriter(writer, tag.c_str());

            auto paths_json = ArrayWriter(writer, PATHS);

            for (size_t i = 0; i < recents.GetNumPaths(); ++i) {
                const std::string &path = recents.GetPathByIndex(i);
                writer->String(path.c_str());
            }
        });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadKeymaps(rapidjson::Value *keymaps_json, const char *keymaps_name, Messages *msg) {
    for (rapidjson::SizeType keymap_idx = 0; keymap_idx < keymaps_json->Size(); ++keymap_idx) {
        rapidjson::Value *keymap_json = &(*keymaps_json)[keymap_idx];

        if (!keymap_json->IsObject()) {
            msg->e.f("not an object: %s[%" PRIsizetype "]\n",
                     keymaps_name, keymap_idx);
            continue;
        }

        std::string keymap_name;
        if (!FindStringMember(&keymap_name, keymap_json, NAME, msg)) {
            continue;
        }

        rapidjson::Value keys_json;
        if (!FindObjectMember(&keys_json, keymap_json, KEYS, msg)) {
            LOGF(LOADSAVE, "(no data found.)\n");
            return false;
        }

        bool keysyms;
        if (!FindBoolMember(&keysyms, keymap_json, KEYSYMS, msg)) {
            return false;
        }

        BeebKeymap keymap(keymap_name, keysyms);
        if (keysyms) {
            for (rapidjson::Value::MemberIterator keys_it = keys_json.MemberBegin();
                 keys_it != keys_json.MemberEnd();
                 ++keys_it) {
                const char *beeb_sym_name = keys_it->name.GetString();
                BeebKeySym beeb_sym = GetBeebKeySymByName(beeb_sym_name);
                if (beeb_sym < 0) {
                    msg->w.f("unknown BBC keysym: %s\n", beeb_sym_name);
                    continue;
                }

                if (!keys_it->value.IsArray()) {
                    msg->e.f("not an array: %s.%s.keys.%s\n",
                             KEYS, keymap_name.c_str(), beeb_sym_name);
                    continue;
                }

                for (rapidjson::SizeType i = 0; i < keys_it->value.Size(); ++i) {
                    uint32_t keycode;
                    if (!LoadKeycodeFromObject(&keycode, &keys_it->value[i], msg, "%s.%s.keys[%" PRIsizetype "]", KEYS, keymap_name.c_str(), i)) {
                        continue;
                    }

                    keymap.SetMapping(keycode, (int8_t)beeb_sym, true);
                }
            }
        } else {
            for (rapidjson::Value::MemberIterator keys_it = keys_json.MemberBegin();
                 keys_it != keys_json.MemberEnd();
                 ++keys_it) {
                LOGF(LOADSAVE, "    Loading scancodes for: %s\n", keys_it->name.GetString());
                const char *beeb_key_name = keys_it->name.GetString();
                BeebKey beeb_key = GetBeebKeyByName(beeb_key_name);
                if (beeb_key < 0) {
                    msg->w.f("unknown BBC key: %s\n", beeb_key_name);
                    continue;
                }

                if (!keys_it->value.IsArray()) {
                    msg->e.f("not an array: %s.%s.keys.%s\n",
                             KEYS, keymap_name.c_str(), beeb_key_name);
                    continue;
                }

                for (rapidjson::SizeType i = 0; i < keys_it->value.Size(); ++i) {
                    if (keys_it->value[i].IsNumber()) {
                        uint32_t scancode = (uint32_t)keys_it->value[i].GetInt64();
                        keymap.SetMapping(scancode, (int8_t)beeb_key, true);
                    } else if (keys_it->value[i].IsString()) {
                        const char *scancode_name = keys_it->value[i].GetString();
                        uint32_t scancode = SDL_GetScancodeFromName(scancode_name);
                        if (scancode == SDL_SCANCODE_UNKNOWN) {
                            msg->w.f("unknown scancode: %s\n",
                                     scancode_name);
                        } else {
                            keymap.SetMapping(scancode, (int8_t)beeb_key, true);
                        }
                    } else {
                        msg->e.f("not number/string: %s.%s.keys.%s[%" PRIsizetype "]\n",
                                 KEYS, keymap_name.c_str(), beeb_key_name, i);
                        continue;
                    }
                }
            }
        }

        bool prefer_shortcuts;
        if (FindBoolMember(&prefer_shortcuts, keymap_json, PREFER_SHORTCUTS, nullptr)) {
            keymap.SetPreferShortcuts(prefer_shortcuts);
        }

        BeebWindows::AddBeebKeymap(std::move(keymap));
    }

    return true;
}

static void SaveKeymaps(JSONWriter<StringStream> *writer) {
    auto keymaps_json = ArrayWriter(writer, NEW_KEYMAPS);

    for (size_t i = 0; i < BeebWindows::GetNumBeebKeymaps(); ++i) {
        const BeebKeymap *keymap = BeebWindows::GetBeebKeymapByIndex(i);
        auto keymap_json = ObjectWriter(writer);

        writer->Key(NAME);
        writer->String(keymap->GetName().c_str());

        writer->Key(KEYSYMS);
        writer->Bool(keymap->IsKeySymMap());

        writer->Key(PREFER_SHORTCUTS);
        writer->Bool(keymap->GetPreferShortcuts());

        auto keys_json = ObjectWriter(writer, KEYS);

        if (keymap->IsKeySymMap()) {
            for (int beeb_sym = 0; beeb_sym < 128; ++beeb_sym) {
                const char *beeb_sym_name = GetBeebKeySymName((BeebKeySym)beeb_sym);
                if (!beeb_sym_name) {
                    continue;
                }

                const uint32_t *keycodes = keymap->GetPCKeysForValue((int8_t)beeb_sym);
                if (!keycodes) {
                    continue;
                }

                auto key_json = ArrayWriter(writer, beeb_sym_name);

                for (const uint32_t *keycode = keycodes; *keycode != 0; ++keycode) {
                    SaveKeycodeObject(writer, *keycode);
                }
            }
        } else {
            for (int beeb_key = 0; beeb_key < 128; ++beeb_key) {
                const char *beeb_key_name = GetBeebKeyName((BeebKey)beeb_key);
                if (!beeb_key_name) {
                    continue;
                }

                const uint32_t *pc_keys = keymap->GetPCKeysForValue((int8_t)beeb_key);
                if (!pc_keys) {
                    continue;
                }

                auto key_json = ArrayWriter(writer, beeb_key_name);

                for (const uint32_t *scancode = pc_keys;
                     *scancode != 0;
                     ++scancode) {
                    const char *scancode_name = SDL_GetScancodeName((SDL_Scancode)*scancode);
                    if (strlen(scancode_name) == 0) {
                        writer->Int64((int64_t)*scancode);
                    } else {
                        writer->String(scancode_name);
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadShortcuts(rapidjson::Value *shortcuts_json, Messages *msg) {
    for (rapidjson::Value::MemberIterator table_it = shortcuts_json->MemberBegin();
         table_it != shortcuts_json->MemberEnd();
         ++table_it) {

        CommandTable2 *table2 = FindCommandTable2ByName(table_it->name.GetString());

        if (!table2) {
            msg->w.f("unknown command table: %s\n", table_it->name.GetString());
            continue;
        }

        if (!table_it->value.IsObject()) {
            msg->e.f("not an object: %s.%s\n",
                     SHORTCUTS, table_it->name.GetString());
            continue;
        }

        for (rapidjson::Value::MemberIterator command_it = table_it->value.MemberBegin();
             command_it != table_it->value.MemberEnd();
             ++command_it) {

            Command2 *command2 = table2->FindCommandByName(command_it->name.GetString());
            if (!command2) {
                msg->w.f("unknown %s command: %s\n", table_it->name.GetString(), command_it->name.GetString());
                continue;
            }

            if (!command_it->value.IsArray()) {
                msg->e.f("not an array: %s.%s.%s\n",
                         SHORTCUTS, table_it->name.GetString(), command_it->name.GetString());
                continue;
            }

            table2->ClearMappingsByCommand(command2);

            for (rapidjson::SizeType i = 0; i < command_it->value.Size(); ++i) {
                uint32_t keycode;
                if (!LoadKeycodeFromObject(&keycode, &command_it->value[i], msg,
                                           "%s.%s.%s[%" PRIsizetype "]", SHORTCUTS, table_it->name.GetString(), command_it->name.GetString(), i)) {
                    continue;
                }

                table2->AddMapping(keycode, command2);
            }
        }
    }

    return true;
}

template <class CommandTableType>
static void SaveCommandTableShortcuts(JSONWriter<StringStream> *writer, const CommandTableType *table) {
    if (!table) {
        return;
    }

    table->ForEachCommand([writer, table](typename CommandTableType::CommandType *command) {
        bool are_defaults;
        if (const std::vector<uint32_t> *pc_keys = table->GetPCKeysForCommand(&are_defaults, command)) {
            if (!are_defaults) {
                auto command_json = ArrayWriter(writer, command->GetName().c_str());

                for (uint32_t pc_key : *pc_keys) {
                    SaveKeycodeObject(writer, pc_key);
                }
            }
        }
    });
}

static void SaveShortcuts(JSONWriter<StringStream> *writer) {
    auto shortcuts_json = ObjectWriter(writer, SHORTCUTS);

    ForEachCommandTable2([writer](CommandTable2 *table) {
        auto commands_json = ObjectWriter(writer, table->GetName().c_str());

        table->ForEachCommand([table, writer](Command2 *command) {
            bool are_defaults;
            if (const std::vector<uint32_t> *pc_keys = table->GetPCKeysForCommand(&are_defaults, command)) {
                if (!are_defaults) {
                    auto command_json = ArrayWriter(writer, command->GetName().c_str());

                    for (uint32_t pc_key : *pc_keys) {
                        SaveKeycodeObject(writer, pc_key);
                    }
                }
            }
        });
    });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SetROMDefaults(bool *writeable, ROMType *type) {
    if (writeable) {
        *writeable = false;
    }

    if (type) {
        *type = ROMType_16KB;
    }
}

static bool LoadROM(rapidjson::Value *rom_json,
                    BeebConfig::ROM *rom,
                    bool *writeable,
                    ROMType *type,
                    const std::string &json_path,
                    Messages *msg) {
    if (rom_json->IsNull()) {
        rom->file_name.clear();
        rom->standard_rom = nullptr;
        SetROMDefaults(writeable, type);

        return true;
    } else if (rom_json->IsString()) {
        // legacy format - as previously used for OS.
        rom->file_name = rom_json->GetString();

        // it might actually be a standard ROM, but...
        rom->standard_rom = nullptr;

        SetROMDefaults(writeable, type);

        return true;
    } else if (rom_json->IsObject()) {
        rom->standard_rom = nullptr;
        rom->file_name.clear();

        StandardROM standard_rom;
        if (FindEnumMember(&standard_rom, rom_json, STANDARD_ROM, "StandardROM", &GetStandardROMEnumName, msg)) {
            rom->standard_rom = FindBeebROM(standard_rom);
        } else if (FindStringMember(&rom->file_name, rom_json, FILE_NAME, msg)) {
            // ...
        }

        SetROMDefaults(writeable, type);

        if (writeable) {
            FindBoolMember(writeable, rom_json, WRITEABLE, msg);
        }

        if (type) {
            FindEnumMember(type, rom_json, TYPE, "ROM type", &GetROMTypeEnumName, msg);
        }

        return true;
    } else {
        if (msg) {
            msg->w.f("not object or null: %s\n", json_path.c_str());
        }
        return false;
    }
}

static void SaveROM(JSONWriter<StringStream> *writer,
                    const BeebConfig::ROM &rom,
                    const bool *writeable_,
                    const ROMType *type_) {
    bool writeable = writeable_ && *writeable_;
    ROMType type = type_ ? *type_ : ROMType_16KB;

    if (!writeable && !rom.standard_rom && rom.file_name.empty()) {
        writer->Null();
    } else {
        auto rom_json = ObjectWriter(writer);

        if (writeable) {
            writer->Key(WRITEABLE);
            writer->Bool(true);
        }

        if (rom.standard_rom) {
            writer->Key(STANDARD_ROM);
            SaveEnum(writer, rom.standard_rom->rom, &GetStandardROMEnumName);
        } else {
            if (!rom.file_name.empty()) {
                writer->Key(FILE_NAME);
                writer->String(rom.file_name.c_str());

                writer->Key(TYPE);
                SaveEnum(writer, type, &GetROMTypeEnumName);
            }
        }
    }
}

static bool LoadROM(rapidjson::Value *rom_json,
                    BeebConfig::SidewaysROM *rom,
                    const std::string &json_path,
                    Messages *msg) {
    return LoadROM(rom_json, rom, &rom->writeable, &rom->type, json_path, msg);
}

static void SaveROM(JSONWriter<StringStream> *writer, const BeebConfig::SidewaysROM &rom) {
    SaveROM(writer, rom, &rom.writeable, &rom.type);
}

static bool LoadROM(rapidjson::Value *rom_json,
                    BeebConfig::ROM *rom,
                    const std::string &json_path,
                    Messages *msg) {
    return LoadROM(rom_json, rom, nullptr, nullptr, json_path, msg);
}

static void SaveROM(JSONWriter<StringStream> *writer, const BeebConfig::ROM &rom) {
    SaveROM(writer, rom, nullptr, nullptr);
}

static bool LoadConfigs(rapidjson::Value *configs_json, const char *configs_path, Messages *msg) {
    for (rapidjson::SizeType config_idx = 0; config_idx < configs_json->Size(); ++config_idx) {
        rapidjson::Value *config_json = &(*configs_json)[config_idx];

        std::string json_path = strprintf("%s[%" PRIsizetype "]", configs_path, config_idx);

        if (!config_json->IsObject()) {
            msg->e.f("not an object: %s\n", json_path.c_str());
            continue;
        }

        BeebConfig config;

        if (!FindStringMember(&config.name, config_json, NAME, msg)) {
            continue;
        }

        rapidjson::Document::MemberIterator os_it = config_json->FindMember(OS);
        if (os_it != config_json->MemberEnd()) {
            if (!LoadROM(&os_it->value,
                         &config.os,
                         strprintf("%s.%s", json_path.c_str(), OS).c_str(),
                         msg)) {
                continue;
            }
        }

        if (!FindEnumMember(&config.type_id, config_json, TYPE, "BBC Micro type", &GetBBCMicroTypeIDEnumName, msg)) {
            continue;
        }

        std::string disc_interface_name;
        if (FindStringMember(&disc_interface_name, config_json, DISC_INTERFACE, nullptr)) {
            config.disc_interface = FindDiscInterfaceByConfigName(disc_interface_name.c_str());
            if (!config.disc_interface) {
                msg->w.f("unknown disc interface: %s\n", disc_interface_name.c_str());
            }
        }

        rapidjson::Value roms;
        if (!FindArrayMember(&roms, config_json, ROMS, msg)) {
            continue;
        }

        if (roms.Size() != 16) {
            msg->e.f("not an array with 16 entries: %s[%" PRIsizetype "].%s\n",
                     configs_path, config_idx, ROMS);
            continue;
        }

        for (rapidjson::SizeType rom_idx = 0; rom_idx < 16; ++rom_idx) {
            if (!LoadROM(&roms[rom_idx],
                         &config.roms[rom_idx],
                         strprintf("%s.%s[%" PRIsizetype "]", json_path.c_str(), ROMS, rom_idx),
                         msg)) {
                continue;
            }
        }

        FindBoolMember(&config.ext_mem, config_json, EXT_MEM, msg);
        FindBoolMember(&config.beeblink, config_json, BEEBLINK, msg);
        FindBoolMember(&config.adji, config_json, ADJI, msg);
        FindUInt8Member(&config.adji_dip_switches, config_json, ADJI_DIP_SWITCHES, msg);
        FindBoolMember(&config.mouse, config_json, MOUSE, msg);

        if (FindEnumMember(&config.parasite_type, config_json, PARASITE_TYPE, "parasite type", &GetBBCMicroParasiteTypeEnumName, msg)) {
            // ...
        } else {
            bool parasite;
            if (FindBoolMember(&parasite, config_json, PARASITE, msg)) {
                if (parasite) {
                    config.parasite_type = BBCMicroParasiteType_MasterTurbo;
                } else {
                    config.parasite_type = BBCMicroParasiteType_None;
                }
            }
        }

        rapidjson::Document::MemberIterator parasite_os_it = config_json->FindMember(PARASITE_OS);
        if (parasite_os_it != config_json->MemberEnd()) {
            if (!LoadROM(&parasite_os_it->value,
                         &config.parasite_os,
                         strprintf("%s.%s", json_path.c_str(), PARASITE_OS).c_str(),
                         msg)) {
                continue;
            }
        }

        FindEnumMember(&config.nvram_type, config_json, NVRAM_TYPE, "NVRAM type", &GetBeebConfigNVRAMTypeEnumName, msg);

        std::string nvram_hex;
        if (FindStringMember(&nvram_hex, config_json, NVRAM, msg)) {
            if (!GetDataFromHexString(&config.nvram, nvram_hex)) {
                config.nvram.clear();
            }
        }

        BeebWindows::AddConfig(std::move(config));
    }

    return true;
}

static void SaveConfigs(JSONWriter<StringStream> *writer) {
    {
        auto configs_json = ArrayWriter(writer, NEW_CONFIGS);

        for (size_t config_idx = 0; config_idx < BeebWindows::GetNumConfigs(); ++config_idx) {
            BeebConfig *config = BeebWindows::GetConfigByIndex(config_idx);

            auto config_json = ObjectWriter(writer);

            writer->Key(NAME);
            writer->String(config->name.c_str());

            writer->Key(OS);
            SaveROM(writer, config->os);

            writer->Key(TYPE);
            SaveEnum(writer, config->type_id, &GetBBCMicroTypeIDEnumName);

            writer->Key(DISC_INTERFACE);
            if (!config->disc_interface) {
                writer->Null();
            } else {
                writer->String(config->disc_interface->config_name.c_str());
            }

            {
                auto roms_json = ArrayWriter(writer, ROMS);

                for (size_t j = 0; j < 16; ++j) {
                    SaveROM(writer, config->roms[j]);
                }
            }

            writer->Key(EXT_MEM);
            writer->Bool(config->ext_mem);

            writer->Key(BEEBLINK);
            writer->Bool(config->beeblink);

            writer->Key(ADJI);
            writer->Bool(config->adji);

            if (config->adji) {
                writer->Key(ADJI_DIP_SWITCHES);
                writer->Uint(config->adji_dip_switches);
            }

            writer->Key(MOUSE);
            writer->Bool(config->mouse);

            writer->Key(PARASITE_TYPE);
            SaveEnum(writer, config->parasite_type, &GetBBCMicroParasiteTypeEnumName);

            writer->Key(PARASITE_OS);
            SaveROM(writer, config->parasite_os);

            writer->Key(NVRAM_TYPE);
            SaveEnum(writer, config->nvram_type, &GetBeebConfigNVRAMTypeEnumName);

            if (!config->nvram.empty()) {
                writer->Key(NVRAM);
                writer->String(GetHexStringFromData(config->nvram).c_str());
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void LoadNVRAM2(rapidjson::Value *nvram_json, std::vector<uint8_t> *nvram, Messages *msg, const char *key) {
    nvram->clear();

    std::string hex;
    if (FindStringMember(&hex, nvram_json, key, msg)) {
        if (!GetDataFromHexString(nvram, hex)) {
            nvram->clear();
        }
    }
}

static bool LoadNVRAM(rapidjson::Value *nvram_json, std::vector<uint8_t> *master_nvram, std::vector<uint8_t> *compact_nvram, Messages *msg) {
    LoadNVRAM2(nvram_json, master_nvram, msg, "Master");
    LoadNVRAM2(nvram_json, compact_nvram, msg, "MasterCompact");

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadJoysticks(rapidjson::Value *joysticks_json, Messages *msg) {
    JoysticksConfig config;

    rapidjson::Value device_names_json;
    if (FindArrayMember(&device_names_json, joysticks_json, DEVICE_NAMES, msg)) {

        for (uint8_t i = 0; i < device_names_json.Size() && i < NUM_BEEB_JOYSTICKS; ++i) {
            if (!device_names_json[i].IsString()) {
                msg->e.f("not a string: %s.%s[%u]\n", JOYSTICKS, DEVICE_NAMES, i);
                continue;
            }

            config.device_names[i] = device_names_json[i].GetString();
        }
    }

    FindBoolMember(&config.swap_joysticks_when_shared, joysticks_json, SWAP_JOYSTICKS_WHEN_SHARED, msg);

    SetJoysticksConfig(config);

    return true;
}

static void SaveJoysticks(JSONWriter<StringStream> *writer) {
    const JoysticksConfig config = GetJoysticksConfig();

    {
        auto joysticks_json = ObjectWriter(writer, JOYSTICKS);
        {
            auto device_names_json = ArrayWriter(writer, DEVICE_NAMES);

            for (uint8_t i = 0; i < NUM_BEEB_JOYSTICKS; ++i) {
                writer->String(config.device_names[i].c_str());
            }
        }

        writer->Key(SWAP_JOYSTICKS_WHEN_SHARED);
        writer->Bool(config.swap_joysticks_when_shared);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void LoadCopySettings(BeebWindowSettings::CopySettings *settings, rapidjson::Value *windows, const char *convert_mode_key, const char *handle_delete_key, const char *what, Messages *msg) {
    FindEnumMember(&settings->convert_mode, windows, convert_mode_key, what, &GetBBCUTF8ConvertModeEnumName, msg);
    FindBoolMember(&settings->handle_delete, windows, handle_delete_key, msg);
}

static bool LoadWindows(rapidjson::Value *windows, Messages *msg) {
    {
        std::string placement_str;
        if (FindStringMember(&placement_str, windows, PLACEMENT, nullptr)) {
            std::vector<uint8_t> placement_data;
            if (!GetDataFromHexString(&placement_data, placement_str)) {
                msg->e.f("invalid placement data\n");
            } else {
                BeebWindows::SetLastWindowPlacementData(std::move(placement_data));
            }
        }
    }

    FindBitIndexedFlagsMember(&BeebWindows::defaults.popups, windows, POPUPS, "Active popups", &GetBeebWindowPopupTypeEnumName, msg);
    FindFloatMember(&BeebWindows::defaults.bbc_volume, windows, BBC_VOLUME, msg);
    FindFloatMember(&BeebWindows::defaults.disc_volume, windows, DISC_VOLUME, msg);
    FindBoolMember(&BeebWindows::defaults.display_filter, windows, FILTER_BBC, nullptr);
    FindBoolMember(&BeebWindows::defaults.correct_aspect_ratio, windows, CORRECT_ASPECT_RATIO, nullptr);
    FindBoolMember(&BeebWindows::defaults.display_auto_scale, windows, AUTO_SCALE, nullptr);
    FindFloatMember(&BeebWindows::defaults.display_manual_scale, windows, MANUAL_SCALE, nullptr);
    FindBoolMember(&BeebWindows::defaults.power_on_tone, windows, POWER_ON_TONE, nullptr);
    FindBoolMember(&BeebWindows::defaults.display_interlace, windows, INTERLACE, nullptr);
    FindStringMember(&BeebWindows::default_config_name, windows, CONFIG, nullptr);
    FindEnumMember(&BeebWindows::defaults.leds_popup_mode, windows, LEDS_POPUP_MODE, "LEDs popup mode", &GetBeebWindowLEDsPopupModeEnumName, msg);
    FindUIntMember(&BeebWindows::defaults.gui_font_size, windows, GUI_FONT_SIZE, nullptr);
    FindBoolMember(&BeebWindows::defaults.screenshot_filter, windows, SCREENSHOT_FILTER, nullptr);
    FindBoolMember(&BeebWindows::defaults.screenshot_correct_aspect_ratio, windows, SCREENSHOT_CORRECT_ASPECT_RATIO, nullptr);
    FindBoolMember(&BeebWindows::defaults.screenshot_last_vsync, windows, SCREENSHOT_LAST_VSYNC, nullptr);
#if ENABLE_SDL_FULL_SCREEN
    FindBoolMember(&BeebWindows::defaults.full_screen, windows, FULL_SCREEN, nullptr);
#endif
    FindBoolMember(&BeebWindows::defaults.prefer_shortcuts, windows, PREFER_SHORTCUTS, nullptr);
    LoadCopySettings(&BeebWindows::defaults.text_copy_settings, windows, TEXT_UTF8_CONVERT_MODE, TEXT_HANDLE_DELETE, "Text copy mode", msg);
    LoadCopySettings(&BeebWindows::defaults.printer_copy_settings, windows, PRINTER_UTF8_CONVERT_MODE, PRINTER_HANDLE_DELETE, "Printe copy mode", msg);
    FindBoolMember(&BeebWindows::defaults.capture_mouse_on_click, windows, CAPTURE_MOUSE_ON_CLICK, nullptr);

    {
        std::string keymap_name;
        if (FindStringMember(&keymap_name, windows, KEYMAP, msg)) {
            if (const BeebKeymap *keymap = BeebWindows::FindBeebKeymapByName(keymap_name)) {
                BeebWindows::defaults.keymap = keymap;
            } else {
                msg->w.f("default keymap unknown: %s\n", keymap_name.c_str());

                // But it's OK - a sensible one will be selected.
                BeebWindows::defaults.keymap = BeebWindows::GetDefaultBeebKeymap();
            }
        }
    }

    return true;
}

static void SaveCopySettings(JSONWriter<StringStream> *writer, const BeebWindowSettings::CopySettings &settings, const char *convert_mode_key, const char *handle_delete_key) {
    writer->Key(convert_mode_key);
    SaveEnum(writer, settings.convert_mode, &GetBBCUTF8ConvertModeEnumName);

    writer->Key(handle_delete_key);
    writer->Bool(settings.handle_delete);
}

static void SaveWindows(JSONWriter<StringStream> *writer) {
    {
        auto windows_json = ObjectWriter(writer, WINDOWS);

        {
            const std::vector<uint8_t> &placement_data = BeebWindows::GetLastWindowPlacementData();
            if (!placement_data.empty()) {
                writer->Key(PLACEMENT);
                writer->String(GetHexStringFromData(placement_data).c_str());
            }
        }

        {
            auto ui_flags_json = ArrayWriter(writer, POPUPS);

            SaveBitIndexedFlags(writer, BeebWindows::defaults.popups, &GetBeebWindowPopupTypeEnumName);
        }

        if (BeebWindows::defaults.keymap) {
            writer->Key(KEYMAP);
            writer->String(BeebWindows::defaults.keymap->GetName().c_str());
        }

        writer->Key(BBC_VOLUME);
        writer->Double(BeebWindows::defaults.bbc_volume);

        writer->Key(DISC_VOLUME);
        writer->Double(BeebWindows::defaults.disc_volume);

        writer->Key(FILTER_BBC);
        writer->Bool(BeebWindows::defaults.display_filter);

        writer->Key(CORRECT_ASPECT_RATIO);
        writer->Bool(BeebWindows::defaults.correct_aspect_ratio);

        writer->Key(SCREENSHOT_FILTER);
        writer->Bool(BeebWindows::defaults.screenshot_filter);

        writer->Key(SCREENSHOT_CORRECT_ASPECT_RATIO);
        writer->Bool(BeebWindows::defaults.screenshot_correct_aspect_ratio);

        writer->Key(SCREENSHOT_LAST_VSYNC);
        writer->Bool(BeebWindows::defaults.screenshot_last_vsync);

#if ENABLE_SDL_FULL_SCREEN
        writer->Key(FULL_SCREEN);
        writer->Bool(BeebWindows::defaults.full_screen);
#endif

        writer->Key(PREFER_SHORTCUTS);
        writer->Bool(BeebWindows::defaults.prefer_shortcuts);

        writer->Key(AUTO_SCALE);
        writer->Bool(BeebWindows::defaults.display_auto_scale);

        writer->Key(MANUAL_SCALE);
        writer->Double(BeebWindows::defaults.display_manual_scale);

        writer->Key(POWER_ON_TONE);
        writer->Bool(BeebWindows::defaults.power_on_tone);

        writer->Key(INTERLACE);
        writer->Bool(BeebWindows::defaults.display_interlace);

        writer->Key(LEDS_POPUP_MODE);
        SaveEnum(writer, BeebWindows::defaults.leds_popup_mode, &GetBeebWindowLEDsPopupModeEnumName);

        writer->Key(GUI_FONT_SIZE);
        writer->Uint(BeebWindows::defaults.gui_font_size);

        SaveCopySettings(writer, BeebWindows::defaults.text_copy_settings, TEXT_UTF8_CONVERT_MODE, TEXT_HANDLE_DELETE);
        SaveCopySettings(writer, BeebWindows::defaults.printer_copy_settings, PRINTER_UTF8_CONVERT_MODE, PRINTER_HANDLE_DELETE);

        writer->Key(CAPTURE_MOUSE_ON_CLICK);
        writer->Bool(BeebWindows::defaults.capture_mouse_on_click);

        if (!BeebWindows::default_config_name.empty()) {
            writer->Key(CONFIG);
            writer->String(BeebWindows::default_config_name.c_str());
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadTrace(rapidjson::Value *trace_json, Messages *msg) {
    TraceUISettings settings;

    FindFlagsMember(&settings.flags, trace_json, FLAGS, "trace flag", &GetBBCMicroTraceFlagEnumName, msg);
    FindEnumMember(&settings.start, trace_json, START, "start condition", &GetTraceUIStartConditionEnumName, msg);
    FindEnumMember(&settings.stop, trace_json, STOP, "stop condition", &GetTraceUIStopConditionEnumName, msg);
    FindBoolMember(&settings.unlimited, trace_json, UNLIMITED, nullptr);
    FindFlagsMember(&settings.output_flags, trace_json, OUTPUT_FLAGS, "output flags", &GetTraceOutputFlagsEnumName, msg);
    FindUInt64Member(&settings.stop_num_2MHz_cycles, trace_json, STOP_NUM_CYCLES, nullptr);
    FindUInt16Member(&settings.start_instruction_address, trace_json, START_INSTRUCTION_ADDRESS, nullptr);
    FindUInt16Member(&settings.start_write_address, trace_json, START_WRITE_ADDRESS, nullptr);
    FindUInt16Member(&settings.stop_write_address, trace_json, STOP_WRITE_ADDRESS, nullptr);
    FindBoolMember(&settings.auto_save, trace_json, AUTO_SAVE, nullptr);
    FindStringMember(&settings.auto_save_path, trace_json, AUTO_SAVE_PATH, nullptr);
#if SYSTEM_WINDOWS
    FindBoolMember(&settings.unix_line_endings, trace_json, UNIX_LINE_ENDINGS, nullptr);
#endif

    SetDefaultTraceUISettings(settings);

    return true;
}

static void SaveTrace(JSONWriter<StringStream> *writer) {
    const TraceUISettings &settings = GetDefaultTraceUISettings();

    {
        auto trace_json = ObjectWriter(writer, TRACE);

        {
            auto default_flags_json = ArrayWriter(writer, FLAGS);

            SaveFlags(writer, settings.flags, &GetBBCMicroTraceFlagEnumName);
        }

        writer->Key(START);
        SaveEnum(writer, settings.start, &GetTraceUIStartConditionEnumName);

        writer->Key(STOP);
        SaveEnum(writer, settings.stop, &GetTraceUIStopConditionEnumName);

        writer->Key(UNLIMITED);
        writer->Bool(settings.unlimited);

        writer->Key(START_INSTRUCTION_ADDRESS);
        writer->Uint64(settings.start_instruction_address);

        writer->Key(START_WRITE_ADDRESS);
        writer->Uint64(settings.start_write_address);

        writer->Key(STOP_WRITE_ADDRESS);
        writer->Uint64(settings.stop_write_address);

        writer->Key(STOP_NUM_CYCLES);
        writer->Uint64(settings.stop_num_2MHz_cycles);

        {
            auto output_flags_json = ArrayWriter(writer, OUTPUT_FLAGS);

            SaveFlags(writer, settings.output_flags, &GetTraceOutputFlagsEnumName);
        }

        writer->Key(AUTO_SAVE);
        writer->Bool(settings.auto_save);

        writer->Key(AUTO_SAVE_PATH);
        writer->String(settings.auto_save_path.c_str());

#if SYSTEM_WINDOWS
        writer->Key(UNIX_LINE_ENDINGS);
        writer->Bool(settings.unix_line_endings);
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadBeebLink(rapidjson::Value *beeblink_json, Messages *msg) {
    std::vector<std::string> urls;

    rapidjson::Value urls_json;
    FindArrayMember(&urls_json, beeblink_json, URLS, msg);
    for (rapidjson::SizeType url_idx = 0; url_idx < urls_json.Size(); ++url_idx) {
        if (!urls_json[url_idx].IsString()) {
            msg->e.f("not a string: %s.%s[%u]\n", BEEBLINK, URLS, url_idx);
            continue;
        }

        urls.push_back(urls_json[url_idx].GetString());
    }

    BeebLinkHTTPHandler::SetServerURLs(urls);

    return true;
}

static void SaveBeebLink(JSONWriter<StringStream> *writer) {
    std::vector<std::string> urls = BeebLinkHTTPHandler::GetServerURLs();

    {
        auto beeblink_json = ObjectWriter(writer, BEEBLINK);

        {
            auto urls_json = ArrayWriter(writer, URLS);

            for (const std::string &url : urls) {
                writer->String(url.c_str());
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadGlobalConfig(Messages *msg) {
    std::string fname = GetConfigFileName();
    if (fname.empty()) {
        msg->e.f("failed to load config file\n");
        msg->i.f("(couldn't get file name)\n");
        return false;
    }

    // Special-case handling to accommodate old-style config files.
    std::vector<uint8_t> master_nvram, compact_nvram;

    std::vector<char> data;
    if (LoadTextFile(&data, fname, msg, LoadFlag_MightNotExist)) {
        std::unique_ptr<rapidjson::Document> doc = LoadDocument(&data, msg);
        if (!doc) {
            return false;
        }

        //LogDumpBytes(&LOG(LOADSAVE),data->data(),data->size());

        rapidjson::Value globals;
        if (FindObjectMember(&globals, doc.get(), GLOBALS, msg)) {
            LOGF(LOADSAVE, "Loading globals.\n");

            if (!LoadGlobals(&globals, msg)) {
                return false;
            }
        }

        rapidjson::Value recent_paths;
        if (FindObjectMember(&recent_paths, doc.get(), RECENT_PATHS, msg)) {
            LOGF(LOADSAVE, "Loading recent paths.\n");

            if (!LoadRecentPaths(&recent_paths, msg)) {
                return false;
            }
        }

        rapidjson::Value keymaps;
        if (FindArrayMember(&keymaps, doc.get(), OLD_KEYMAPS, msg)) {
            LOGF(LOADSAVE, "Loading keymaps.\n");

            AddDefaultBeebKeymaps();

            if (!LoadKeymaps(&keymaps, OLD_KEYMAPS, msg)) {
                return false;
            }
        } else if (FindArrayMember(&keymaps, doc.get(), NEW_KEYMAPS, msg)) {
            LOGF(LOADSAVE, "Loading keymaps.\n");

            // custom keymaps
            if (!LoadKeymaps(&keymaps, NEW_KEYMAPS, msg)) {
                return false;
            }
        }

        // need at least 1 keymap for loading the default keymap.
        EnsureDefaultBeebKeymapsAvailable();

        rapidjson::Value shortcuts;
        if (FindObjectMember(&shortcuts, doc.get(), SHORTCUTS, msg)) {
            LOGF(LOADSAVE, "Loading keyboard shortcuts.\n");

            if (!LoadShortcuts(&shortcuts, msg)) {
                return false;
            }
        }

        rapidjson::Value windows;
        if (FindObjectMember(&windows, doc.get(), WINDOWS, msg)) {
            if (!LoadWindows(&windows, msg)) {
                return false;
            }
        }

        rapidjson::Value configs;
        if (FindArrayMember(&configs, doc.get(), OLD_CONFIGS, msg)) {
            LOGF(LOADSAVE, "Loading configs.\n");

            AddDefaultBeebConfigs(true);

            if (!LoadConfigs(&configs, OLD_CONFIGS, msg)) {
                return false;
            }
        } else if (FindArrayMember(&configs, doc.get(), NEW_CONFIGS, msg)) {
            LOGF(LOADSAVE, "Loading configs.\n");

            if (!LoadConfigs(&configs, NEW_CONFIGS, msg)) {
                return false;
            }

            AddDefaultBeebConfigs(false);
        }

        rapidjson::Value trace;
        if (FindObjectMember(&trace, doc.get(), TRACE, msg)) {
            LOGF(LOADSAVE, "Loading trace.\n");

            if (!LoadTrace(&trace, msg)) {
                return false;
            }
        }

        rapidjson::Value beeblink;
        if (FindObjectMember(&beeblink, doc.get(), BEEBLINK, msg)) {
            LOGF(LOADSAVE, "Loading BeebLink.\n");

            if (!LoadBeebLink(&beeblink, msg)) {
                return false;
            }
        }

        // Only 2 types of NVRAM were ever supported by the old-style NVRAM
        // setup.
        rapidjson::Value nvram;
        if (FindObjectMember(&nvram, doc.get(), NVRAM, msg)) {
            LOGF(LOADSAVE, "Loading old NVRAM.\n");

            if (!LoadNVRAM(&nvram, &master_nvram, &compact_nvram, msg)) {
                return false;
            }
        }

        rapidjson::Value joysticks;
        if (FindObjectMember(&joysticks, doc.get(), JOYSTICKS, msg)) {
            LOGF(LOADSAVE, "Loading joysticks.\n");

            if (!LoadJoysticks(&joysticks, msg)) {
                return false;
            }
        }
    }

    // The rest of the code assumes there's at least 1 config and 1 keymap - so
    // if either list ended up empty, populate it with the default set.
    EnsureDefaultBeebKeymapsAvailable();

    if (BeebWindows::GetNumConfigs() == 0) {
        AddDefaultBeebConfigs(true);
    }

    // Handle old-style NVRAM by propagating loaded Master/Compact NVRAM
    // settings.
    if (master_nvram.empty()) {
        master_nvram = GetDefaultMaster128NVRAM();
    }

    if (compact_nvram.empty()) {
        compact_nvram = GetDefaultMasterCompactNVRAM();
    }

    for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
        BeebConfig *config = BeebWindows::GetConfigByIndex(i);

        if (config->nvram.empty()) {
            switch (config->type_id) {
            default:
                break;

            case BBCMicroTypeID_Master:
                config->nvram = master_nvram;
                break;

            case BBCMicroTypeID_MasterCompact:
                config->nvram = compact_nvram;
                break;
            }
        }
    }

    return true;
}

bool SaveGlobalConfig(Messages *messages) {
    std::string fname = GetConfigFileName();
    if (fname.empty()) {
        messages->e.f("failed to save config file: %s\n", fname.c_str());
        messages->i.f("(couldn't get file name)\n");
        return false;
    }

    if (!PathCreateFolder(PathGetFolder(fname))) {
        int e = errno;

        messages->e.f("failed to save config file: %s\n", fname.c_str());
        messages->i.f("(failed to create folder: %s)\n", strerror(e));
        return false;
    }

    std::string json;
    {
        StringStream stream(&json);
        JSONWriter<StringStream> writer(stream);

        auto root = ObjectWriter(&writer);

        SaveGlobals(&writer);

        SaveRecentPaths(&writer);

        SaveKeymaps(&writer);

        SaveShortcuts(&writer);

        SaveWindows(&writer);

        SaveTrace(&writer);

        SaveBeebLink(&writer);

        SaveConfigs(&writer);

        SaveJoysticks(&writer);
    }

    if (!SaveTextFile(json, fname, messages)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
