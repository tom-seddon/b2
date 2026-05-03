#include <shared/system.h>
#include "misc.h"
#include <shared/path.h>
#include <shared/log.h>
#include <SDL.h>
//#include <parson.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <beeb/BBCMicro.h>
#include <beeb/Trace.h>
#include <shared/debug.h>
#include <unordered_map>
#include <shared/strings.h>
#include <limits>
#include <shared/file_io.h>
#include <miniz.h>
#include <miniz_tinfl.h>

#include <shared/enum_def.h>
#include "misc.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DumpRendererInfo(Log *log, const SDL_RendererInfo *info) {
    LogIndenter indent(log);

    log->f("Name: %s\n", info->name);
#define F(X) info->flags &SDL_RENDERER_##X ? " " #X : ""
    log->f("Flags:%s%s%s%s\n", F(SOFTWARE), F(ACCELERATED), F(PRESENTVSYNC), F(TARGETTEXTURE));
#undef F
    log->f("Max Texture Size: %dx%d\n", info->max_texture_width, info->max_texture_width);
    log->f("Texture Formats:");
    for (size_t i = 0; i < info->num_texture_formats; ++i) {
        log->f(" %s", SDL_GetPixelFormatName(info->texture_formats[i]));
    }
    log->f("\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetRenderScaleQualityHint(bool filter) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter ? "linear" : "nearest");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetFlagsString(uint32_t value, const char *(*get_name_fn)(int)) {
    std::string str;

    for (uint32_t mask = 1; mask != 0; mask <<= 1) {
        if (value & mask) {
            const char *name = (*get_name_fn)((int)mask);

            if (!str.empty()) {
                str += "|";
            }

            if (name[0] == '?') {
                str += strprintf("0x%" PRIx32, mask);
            } else {
                str += name;
            }
        }
    }

    if (str.empty()) {
        str = "0";
    }

    return str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddCommaSeparator(std::string *str) {
    if (!str->empty()) {
        *str += ", ";
    }
}

std::string GetCloneImpedimentsDescription(uint32_t impediments) {
    if (impediments == 0) {
        return "none";
    } else {
        std::string r;
        for (int i = 0; i < NUM_DRIVES; ++i) {
            if (impediments & (uint32_t)BBCMicroCloneImpediment_Drive0 << i) {
                AddCommaSeparator(&r);
                r += strprintf("drive %d", i);
            }
        }

        for (int i = 0; i < NUM_HARD_DISKS; ++i) {
            if (impediments & (uint32_t)BBCMicroCloneImpediment_HardDisk0 << i) {
                AddCommaSeparator(&r);
                r += strprintf("hard disk %d", i);
            }
        }

        if (impediments & BBCMicroCloneImpediment_BeebLink) {
            AddCommaSeparator(&r);
            r += "BeebLink";
        }

        if (impediments & BBCMicroCloneImpediment_Serial) {
            AddCommaSeparator(&r);
            r += "Serial";
        }

        if (impediments & BBCMicroCloneImpediment_MMFS) {
            AddCommaSeparator(&r);
            r += "MMFS";
        }

        return r;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//std::string GetMicrosecondsString(uint64_t num_microseconds) {
//    char str[500];
//
//    uint64_t n = num_microseconds;
//
//    unsigned us = n % 1000;
//    n /= 1000;
//
//    unsigned ms = n % 1000;
//    n /= 1000;
//
//    unsigned secs = n % 60;
//    n /= 60;
//
//    uint64_t minutes = n;
//
//    snprintf(str, sizeof str, "%" PRIu64 " min %02u sec %03u ms %03u usec", minutes, secs, ms, us);
//
//    return str;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetCycleCountString(CycleCount cycle_count) {
    static_assert(CYCLES_PER_SECOND == 4000000, "GetCycleCountString needs fixing");

    char str[500];

    uint64_t n = cycle_count.n;

    unsigned cycles = n % 4;
    n /= 4;

    unsigned us = n % 1000;
    n /= 1000;

    unsigned ms = n % 1000;
    n /= 1000;

    unsigned secs = n % 60;
    n /= 60;

    uint64_t minutes = n;

    snprintf(str, sizeof str, "%" PRIu64 " min %02u sec %03u ms %03u.%02d " MICROSECONDS_UTF8, minutes, secs, ms, us, cycles * 25);

    return str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GetThousandsString(char *str, uint64_t value) {
    char tmp[MAX_UINT64_THOUSANDS_SIZE];
    char *p = tmp + sizeof tmp - 1;
    int n = 3;

    *p = 0;

    do {
        --p;
        if (n == 0) {
            *p = ',';
            --p;
            n = 3;
        }
        *p = '0' + value % 10;
        value /= 10;
        --n;
    } while (value != 0);

    memcpy(str, p, (size_t)(tmp + sizeof tmp - p));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Window *w) const {
    SDL_DestroyWindow(w);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Renderer *r) const {
    SDL_DestroyRenderer(r);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Texture *t) const {
    SDL_DestroyTexture(t);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Surface *s) const {
    SDL_FreeSurface(s);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_PixelFormat *p) const {
    SDL_FreeFormat(p);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Joystick *p) const {
    SDL_JoystickClose(p);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_GameController *p) const {
    SDL_GameControllerClose(p);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SDL_SurfaceLocker::SDL_SurfaceLocker(SDL_Surface *surface) {
    if (SDL_LockSurface(surface) == 0) {
        m_surface = surface;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SDL_SurfaceLocker::~SDL_SurfaceLocker() {
    this->Unlock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SDL_SurfaceLocker::IsLocked() const {
    return !!m_surface;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_SurfaceLocker::Unlock() {
    if (m_surface) {
        SDL_UnlockSurface(m_surface);
        m_surface = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetUniqueName(std::string suggested_name,
                          std::function<const void *(const std::string &)> find,
                          const void *ignore) {
    const void *p = find(suggested_name);
    if (!p || p == ignore) {
        return suggested_name;
    }

    uint64_t suffix = 2;

    // decode and remove any existing suffix.
    if (!suggested_name.empty()) {
        if (suggested_name.back() == ')') {
            std::string::size_type op = suggested_name.find_last_of("(");
            if (op != std::string::npos) {
                std::string suffix_str = suggested_name.substr(op + 1, (suggested_name.size() - 1) - (op + 1));
                if (suffix_str.find_first_not_of("0123456789") == std::string::npos) {
                    suffix = strtoull(suffix_str.c_str(), nullptr, 0);

                    suggested_name = suggested_name.substr(0, op);

                    while (!suggested_name.empty() && isspace(suggested_name.back())) {
                        suggested_name.pop_back();
                    }
                }
            }
        }
    }

    for (;;) {
        std::string new_name = suggested_name + " (" + std::to_string(suffix) + ")";
        const void *existing_item = find(new_name);
        if (!existing_item || existing_item == ignore) {
            return new_name;
        }

        ++suffix;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct tm GetUTCTimeNow() {
    time_t now;
    time(&now);

    struct tm utc;
    gmtime_r(&now, &utc);

    return utc;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct tm GetLocalTimeNow() {
    time_t now;
    time(&now);

    struct tm local;
    localtime_r(&now, &local);

    return local;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetTimeString(const struct tm &t) {
    char time_str[500];
    strftime(time_str, sizeof time_str, "%c", &t);

    return time_str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

AudioDeviceLock::AudioDeviceLock(uint32_t device)
    : m_device(device) {
    if (m_device != 0) {
        SDL_LockAudioDevice(m_device);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

AudioDeviceLock::~AudioDeviceLock() {
    if (m_device != 0) {
        SDL_UnlockAudioDevice(m_device);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::string> GetSplitString(const std::string &str, const std::string &separator_chars) {
    std::vector<std::string> parts;

    std::string::size_type a = 0;
    while (a < str.size()) {
        std::string::size_type b = str.find_first_of(separator_chars, a);
        if (b == std::string::npos) {
            parts.push_back(str.substr(a));
            break;
        }

        parts.push_back(str.substr(a, b - a));
        a = b + 1;
    }

    return parts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static bool GetUIntValueFromString(T *value, const char *str, int radix, const char **ep_out) {
    // Always skip leading spaces.
    const char *c = str;
    while (*c != 0 && isspace(*c)) {
        ++c;
    }

    // If empty, fail.
    if (*c == 0) {
        return false;
    }

    if (radix == 0) {
        // Handle non-standard base prefixes.

        if (*c == '$' || *c == '&') {
            c++;
            radix = 16;
        } else if (c[0] == '0' && c[1] == 'b') {
            c += 2;
            radix = 2;
        }
    }

    char *ep;
    unsigned long long tmp = strtoull(c, &ep, radix);
    if (ep_out) {
        *ep_out = ep;
    } else {
        if (*ep != 0 && !isspace(*ep)) {
            return false;
        }
    }

    if (tmp > std::numeric_limits<T>::max()) {
        return false;
    }

    *value = (T)tmp;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetBoolFromString(bool *value, const std::string &str) {
    return GetBoolFromString(value, str.c_str());
}

bool GetBoolFromString(bool *value, const char *str) {
    while (*str != 0 && isspace(*str)) {
        ++str;
    }

    if (strcmp(str, "1") == 0 || strcmp(str, "true") == 0) {
        *value = true;
        return true;
    } else if (strcmp(str, "0") == 0 || strcmp(str, "false") == 0) {
        *value = false;
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt8FromString(uint8_t *value, const std::string &str, int radix, const char **ep) {
    return GetUInt8FromString(value, str.c_str(), radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt8FromString(uint8_t *value, const char *str, int radix, const char **ep) {
    return GetUIntValueFromString(value, str, radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt16FromString(uint16_t *value, const std::string &str, int radix, const char **ep) {
    return GetUInt16FromString(value, str.c_str(), radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt16FromString(uint16_t *value, const char *str, int radix, const char **ep) {
    return GetUIntValueFromString(value, str, radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt32FromString(uint32_t *value, const std::string &str, int radix, const char **ep) {
    return GetUInt32FromString(value, str.c_str(), radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt32FromString(uint32_t *value, const char *str, int radix, const char **ep) {
    return GetUIntValueFromString(value, str, radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt64FromString(uint64_t *value, const std::string &str, int radix, const char **ep) {
    return GetUInt64FromString(value, str.c_str(), radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt64FromString(uint64_t *value, const char *str, int radix, const char **ep) {
    return GetUIntValueFromString(value, str, radix, ep);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

static const uint32_t UTF8_ACCEPT = 0;
static const uint32_t UTF8_REJECT = 1;

static const uint8_t utf8d[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00..1f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 20..3f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40..5f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 60..7f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 80..9f
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // a0..bf
    8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // c0..df
    0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3,                 // e0..ef
    0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,                 // f0..ff
    0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1,                 // s0..s0
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, // s1..s2
    1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, // s3..s4
    1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, // s5..s6
    1, 3, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // s7..s8
};

uint32_t inline decode(uint32_t *state, uint32_t *codep, uint32_t byte) {
    uint32_t type = utf8d[byte];

    *codep = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xffu >> type) & (byte);

    *state = utf8d[256 + *state * 16 + type];
    return *state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::unordered_map<uint32_t, uint8_t> g_bbc_char_by_codepoint;
static std::string g_utf8_char_by_bbc_char[BBCUTF8ConvertMode_Count][128];
static bool g_utf8_convert_tables_initialised = false;

static uint32_t GetCodePointForBBCChar(uint8_t bbc_char, BBCUTF8ConvertMode mode) {
    ASSERT(bbc_char >= 32 && bbc_char < 127);

    switch (mode) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCUTF8ConvertMode_PassThrough:
        return bbc_char;
        //return std::string(1, (char)bbc_char);

    case BBCUTF8ConvertMode_OnlyGBP:
        switch (bbc_char) {
        case '`':
            return 0xa3; //return "\xc2\xa3"; // U+00A3 POUND SIGN

        default:
            return bbc_char; //return std::string(1, (char)bbc_char);
        }

    case BBCUTF8ConvertMode_SAA5050:
        switch (bbc_char) {
        case '`':
            return 0xa3; //"\xc2\xa3"; // U+00A3 POUND SIGN

        case '\\':
            return 0xbd; //"\xc2\xbd"; //U+00BD VULGAR FRACTION ONE HALF

        case '_':
            return 0x2015; //"\xe2\x80\x95"; //U+2015 HORIZONTAL BAR

        case '[':
            return 0x2190; //"\xe2\x86\x90"; // U+2190 LEFTWARDS ARROW

        case ']':
            return 0x2192; //"\xe2\x86\x92"; // U+2192 RIGHTWARDS ARROW

        case '{':
            return 0xbc; //"\xc2\xbc"; //U+00BC VULGAR FRACTION ONE QUARTER

        case '}':
            return 0xbe; //"\xc2\xbe"; // U+00BE VULGAR FRACTION THREE QUARTERS

        case '|':
            return 0x2016; //"\xe2\x80\x96"; //U+2016 DOUBLE VERTICAL LINE

        case '^':
            return 0x2191; //"\xe2\x86\x91"; //U+2191 UPWARDS ARROW

        case '~':
            return 0xf7; //"\xc3\xb7"; // U+00F7 DIVISION SIGN

        default:
            return bbc_char; //std::string(1, (char)bbc_char);
        }
    }
}

std::string GetUTF8StringForCodePoint(uint32_t u) {
    if (u < 0x80) {
        return std::string(1, (char)u);
    } else if (u < 0x800) {
        char buf[2] = {
            (char)(0xc0 | (u >> 6)),
            (char)(0x80 | (u & 0x3f)),
        };
        return std::string(buf, buf + 2);
    } else if (u < 0x10000) {
        char buf[3] = {
            (char)(0xe0 | (u >> 12)),
            (char)(0x80 | (u >> 6 & 0x3f)),
            (char)(0x80 | (u & 0x3f)),
        };
        return std::string(buf, buf + 3);
    } else {
        ASSERT(u < 0x110000);
        char buf[4] = {
            (char)(0xf0 | (u >> 18)),
            (char)(0x80 | (u >> 12 & 0x3f)),
            (char)(0x80 | (u >> 6 & 0x3f)),
            (char)(0x80 | (u & 0x3f)),
        };
        return std::string(buf, buf + 4);
    }
}

static void InitUTF8ConvertTables() {
    if (!g_utf8_convert_tables_initialised) {
        for (int mode = 0; mode < BBCUTF8ConvertMode_Count; ++mode) {
            for (uint8_t c = 32; c < 127; ++c) {
                uint32_t u = GetCodePointForBBCChar(c, (BBCUTF8ConvertMode)mode);

                g_utf8_char_by_bbc_char[mode][c] = GetUTF8StringForCodePoint(u);

                auto &&it = g_bbc_char_by_codepoint.find(u);
                if (it == g_bbc_char_by_codepoint.end()) {
                    g_bbc_char_by_codepoint[u] = c;
                } else {
                    ASSERT(g_bbc_char_by_codepoint[u] == c);
                }
            }
        }

        ASSERT(g_bbc_char_by_codepoint.count(10) == 0);
        g_bbc_char_by_codepoint[10] = 10;

        ASSERT(g_bbc_char_by_codepoint.count(13) == 0);
        g_bbc_char_by_codepoint[13] = 13;

        g_utf8_convert_tables_initialised = true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetBBCASCIIFromUTF8(std::vector<uint8_t> *ascii, const uint8_t *data, size_t data_size_bytes, int32_t *bad_codepoint_ptr, size_t *bad_char_start_ptr, int *bad_char_len_ptr) {
    uint32_t state = UTF8_ACCEPT, codepoint;

    InitUTF8ConvertTables();

    ascii->clear();
    size_t char_start = 0;

    int32_t bad_codepoint = -1;
    int bad_char_len = 0;

    for (size_t i = 0; i < data_size_bytes; ++i) {
        decode(&state, &codepoint, data[i]);
        if (state == UTF8_ACCEPT) {
            auto &&it = g_bbc_char_by_codepoint.find(codepoint);
            if (it == g_bbc_char_by_codepoint.end()) {
                bad_codepoint = (int32_t)codepoint; //cast is safe - Unicode codepoints are <32 bits
                bad_char_len = (int)(i - char_start);

                goto bad;
            }

            ascii->push_back(it->second);
            char_start = i + 1;
        } else if (state == UTF8_REJECT) {
            goto bad;
        }
    }

    return true;

bad:;
    if (bad_codepoint_ptr) {
        *bad_codepoint_ptr = bad_codepoint;
    }

    if (bad_char_start_ptr) {
        *bad_char_start_ptr = char_start;
    }

    if (bad_char_len_ptr) {
        *bad_char_len_ptr = bad_char_len;
    }

    return false;
}

bool GetBBCASCIIFromUTF8(std::vector<uint8_t> *ascii, const std::string &data, int32_t *bad_codepoint_ptr, size_t *bad_char_start_ptr, int *bad_char_len_ptr) {
    return GetBBCASCIIFromUTF8(ascii, (const uint8_t *)data.data(), data.size(), bad_codepoint_ptr, bad_char_start_ptr, bad_char_len_ptr);
}

bool GetBBCASCIIFromUTF8(std::vector<uint8_t> *ascii, const std::vector<uint8_t> &data, int32_t *bad_codepoint_ptr, size_t *bad_char_start_ptr, int *bad_char_len_ptr) {
    return GetBBCASCIIFromUTF8(ascii, data.data(), data.size(), bad_codepoint_ptr, bad_char_start_ptr, bad_char_len_ptr);
}

void SetClipboardFromBBCASCII(const std::vector<uint8_t> &data, BBCUTF8ConvertMode mode, bool handle_delete, const LogSet *logs) {
    std::string utf8 = GetUTF8FromBBCASCII(data, mode, handle_delete);

    int rc = SDL_SetClipboardText(utf8.c_str());
    if (rc != 0) {
        if (logs) {
            logs->e.f("Failed to copy to clipboard: %s\n", SDL_GetError());
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t GetBBCASCIIFromISO8859_1(std::vector<uint8_t> *bbc_ascii, const std::vector<uint8_t> &data) {
    bbc_ascii->clear();

    for (uint8_t x : data) {
        if (x >= 32 && x <= 126) {
            // ok...
        } else if (x == 0xa3) {
            // GBP
            x = '`';
        } else {
            return x;
        }

        bbc_ascii->push_back(x);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint8_t NUM_VDU_CONTROL_CODE_PARAMETERS[32] = {
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    2,
    5,
    0,
    0,
    1,
    9,
    8,
    5,
    0,
    0,
    4,
    4,
    0,
    2,
};

std::string GetUTF8FromBBCASCII(const std::vector<uint8_t> &data, BBCUTF8ConvertMode mode, bool handle_delete) {
    ASSERT(mode >= 0 && mode < BBCUTF8ConvertMode_Count);
    InitUTF8ConvertTables();

    // Normalize line endings and strip out control codes.
    //
    // TODO: do it in a less dumb fashion.
    std::string utf8;
    utf8.reserve(data.size());

    std::vector<uint8_t> output_sizes;
    output_sizes.reserve(data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        size_t old_utf8_size = utf8.size();

        if (data[i] == 10 || data[i] == 13) {
            // Translate line endings.
            if (i + 1 < data.size() &&
                (data[i + 1] == 10 || data[i + 1] == 13) &&
                data[i] != data[i + 1]) {
                // Consume 2-byte line ending.
                ++i;
            }
#if SYSTEM_WINDOWS
            utf8 += "\r\n";
#else
            utf8 += "\n";
#endif
        } else if (data[i] < 32) {
            // Skip VDU codes.
            i += NUM_VDU_CONTROL_CODE_PARAMETERS[data[i]];
        } else if (data[i] >= 32 && data[i] < 127) {
            utf8 += g_utf8_char_by_bbc_char[mode][data[i]];
        } else if (data[i] == 127) {
            if (handle_delete) {
                // Remove the last char (if any).
                uint8_t n = output_sizes.back();
                output_sizes.pop_back();

                if (n > utf8.size()) {
                    n = (uint8_t)utf8.size();
                }

                utf8.erase(utf8.end() - n);

                continue;
            }
        } else {
            // 128+. TODO.
        }

        size_t delta = utf8.size() - old_utf8_size;
        ASSERT(delta < UINT8_MAX);
        output_sizes.push_back((uint8_t)delta);
    }

    return utf8;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ForEachLine(const std::string &str, std::function<bool(const std::string_view &line)> fun) {
    std::string::size_type a = 0, b = a, n = str.size();
    const char *data = str.data();

    while (b != n) {
        char c = str[b];
        if (c == '\r' || c == '\n') {
            if (!fun(std::string_view(data + a, b - a))) {
                return false;
            }

            ++b;
            if (b < n) {
                char c2 = str[b];
                if ((c2 == '\r' || c2 == '\n') && c2 != c) {
                    ++b;
                }
            }

            a = b;
        } else {
            ++b;
        }
    }

    if (b != a) {
        if (!fun(std::string_view(data + a, b - a))) {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FixBBCASCIINewlines(std::vector<uint8_t> *str) {
    // Knobble newlines.
    if (str->size() > 1) {
        size_t i = 0;

        while (i < str->size() - 1) {
            if ((*str)[i] == 10 && (*str)[i + 1] == 13) {
                str->erase(str->begin() + (ptrdiff_t)i);
            } else if ((*str)[i] == 13 && (*str)[i + 1] == 10) {
                ++i;
                str->erase(str->begin() + (ptrdiff_t)i);
            } else if ((*str)[i] == 10) {
                (*str)[i++] = 13;
            } else {
                ++i;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsGzipData(const std::vector<uint8_t> &data, size_t index) {
    if (index + 10 <= data.size()) {
        if (data[index + 0] == 0x1f && data[index + 1] == 0x8b && data[index + 2] == 8) {
            return true;
        }
    }

    return false;
}

static bool Decompress(std::vector<uint8_t> *data, const std::string &path, const LogSet *logs) {
    // 3 GB should be enough for anyone
    static const size_t MAX_UNCOMPRESSED_SIZE = 3u * 1024u * 1024u * 1024u;
    static const size_t DEST_DATA_SIZE_DELTA = 1048576;

    std::vector<uint8_t> dest_data;
    size_t dest_index = 0;

    size_t src_index = 0;
    while (src_index < data->size()) {
        if (!IsGzipData(*data, src_index)) {
            if (src_index == 0) {
                // The entire input data is presumably uncompressed, so this is fine.
                return true;
            } else {
                if (logs) {
                    logs->e.f("%s: contains trailing uncompressed data\n", path.c_str());
                }
                return false;
            }
        }

        // Read FLG from the header.
        uint8_t flg = (*data)[src_index + 3];

        // Skip the header.
        src_index += 10;

        if (flg & 1 << 2) {
            // FEXTRA.
            if (src_index + 2 > data->size()) {
                if (logs) {
                    logs->e.f("%s: FEXTRA header overran\n", path.c_str());
                }
                return false;
            }

            uint16_t xlen = (*data)[src_index + 0] | (*data)[src_index + 1] << 8;
            src_index += 2;
            if (src_index + xlen > data->size()) {
                if (logs) {
                    logs->e.f("%s: FEXTRA data overran\n", path.c_str());
                }
                return false;
            }

            src_index += xlen;
        }

        if (flg & 1 << 3) {
            // FNAME.
            while (src_index < data->size() && (*data)[src_index] != 0) {
                ++src_index;
            }

            if (src_index == data->size()) {
                if (logs) {
                    logs->e.f("%s: FNAME data overran\n", path.c_str());
                }
                return false;
            }

            ++src_index; //and skip the 0 too.
        }

        if (flg & 1 << 4) {
            // FCOMMENT.
            while (src_index < data->size() && (*data)[src_index] != 0) {
                ++src_index;
            }

            if (src_index == data->size()) {
                if (logs) {
                    logs->e.f("%s: FCOMMENT data overran\n", path.c_str());
                }
                return false;
            }

            ++src_index; //and skip the 0 too.
        }

        if (flg & 1 << 1) {
            src_index += 2;

            if (src_index >= data->size()) {
                if (logs) {
                    logs->e.f("%s: FHCRC data overran\n", path.c_str());
                }
                return false;
            }
        }

        if (src_index == data->size()) {
            if (logs) {
                logs->e.f("%s: compressed data missing\n", path.c_str());
            }
            return false;
        }

        tinfl_decompressor *decompressor = tinfl_decompressor_alloc();
        if (!decompressor) {
            if (logs) {
                logs->e.f("%s: failed to allocate decompressor\n", path.c_str());
            }
            return false;
        }

        for (;;) {
            const mz_uint8 *src = data->data() + src_index;
            size_t src_size = data->size() - src_index;

            mz_uint8 *dest = dest_data.data() + dest_index;
            size_t dest_size = dest_data.size() - dest_index;

            tinfl_status status = tinfl_decompress(decompressor,
                                                   src,
                                                   &src_size,
                                                   dest_data.data(),
                                                   dest,
                                                   &dest_size,
                                                   TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
            src_index += src_size;
            dest_index += dest_size;

            switch (status) {
            case TINFL_STATUS_DONE:
                dest_data.resize(dest_index);
                goto done;

            case TINFL_STATUS_HAS_MORE_OUTPUT:
                if (dest_data.size() > MAX_UNCOMPRESSED_SIZE) {
                    if (logs) {
                        logs->e.f("%s: uncompressed data too large\n", path.c_str());
                        return false;
                    }
                }

                dest_data.resize(dest_data.size() + DEST_DATA_SIZE_DELTA);
                break;

            default:
                if (logs) {
                    logs->e.f("%s: decompressor failed: %d\n", path.c_str(), status);
                }
                return false;
            }
        }
    done:

        tinfl_decompressor_free(decompressor), decompressor = nullptr;

        // Skip CRC32 and ISIZE.
        src_index += 8;
    }

    data->swap(dest_data);

    return true;
}

bool DecompressGzip(std::vector<uint8_t> *data) {
    return Decompress(data, "", nullptr);
}

bool LoadPossiblyGzippedFile(std::vector<uint8_t> *data, const std::string &path, const LogSet *logs, uint32_t flags) {
    if (!LoadFile(data, path, logs, flags)) {
        return false;
    }

    if (!Decompress(data, path, logs)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static_assert(sizeof(BASE64_ALPHABET) - 1 == 64);

static constexpr int8_t GetBase64BitsFromChar(uint8_t ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return 0 + (ch - 'A');
    } else if (ch >= 'a' && ch <= 'z') {
        return 26 + (ch - 'a');
    } else if (ch >= '0' && ch <= '9') {
        return 52 + (ch - '0');
    } else if (ch == '+') {
        return 62;
    } else if (ch == '/') {
        return 63;
    } else {
        return -1;
    }
}

static constexpr int8_t BASE64_BITS_FROM_CHAR[] = {GetBase64BitsFromChar(0), GetBase64BitsFromChar(1), GetBase64BitsFromChar(2), GetBase64BitsFromChar(3), GetBase64BitsFromChar(4), GetBase64BitsFromChar(5), GetBase64BitsFromChar(6), GetBase64BitsFromChar(7), GetBase64BitsFromChar(8), GetBase64BitsFromChar(9), GetBase64BitsFromChar(10), GetBase64BitsFromChar(11), GetBase64BitsFromChar(12), GetBase64BitsFromChar(13), GetBase64BitsFromChar(14), GetBase64BitsFromChar(15), GetBase64BitsFromChar(16), GetBase64BitsFromChar(17), GetBase64BitsFromChar(18), GetBase64BitsFromChar(19), GetBase64BitsFromChar(20), GetBase64BitsFromChar(21), GetBase64BitsFromChar(22), GetBase64BitsFromChar(23), GetBase64BitsFromChar(24), GetBase64BitsFromChar(25), GetBase64BitsFromChar(26), GetBase64BitsFromChar(27), GetBase64BitsFromChar(28), GetBase64BitsFromChar(29), GetBase64BitsFromChar(30), GetBase64BitsFromChar(31), GetBase64BitsFromChar(32), GetBase64BitsFromChar(33), GetBase64BitsFromChar(34), GetBase64BitsFromChar(35), GetBase64BitsFromChar(36), GetBase64BitsFromChar(37), GetBase64BitsFromChar(38), GetBase64BitsFromChar(39), GetBase64BitsFromChar(40), GetBase64BitsFromChar(41), GetBase64BitsFromChar(42), GetBase64BitsFromChar(43), GetBase64BitsFromChar(44), GetBase64BitsFromChar(45), GetBase64BitsFromChar(46), GetBase64BitsFromChar(47), GetBase64BitsFromChar(48), GetBase64BitsFromChar(49), GetBase64BitsFromChar(50), GetBase64BitsFromChar(51), GetBase64BitsFromChar(52), GetBase64BitsFromChar(53), GetBase64BitsFromChar(54), GetBase64BitsFromChar(55), GetBase64BitsFromChar(56), GetBase64BitsFromChar(57), GetBase64BitsFromChar(58), GetBase64BitsFromChar(59), GetBase64BitsFromChar(60), GetBase64BitsFromChar(61), GetBase64BitsFromChar(62), GetBase64BitsFromChar(63), GetBase64BitsFromChar(64), GetBase64BitsFromChar(65), GetBase64BitsFromChar(66), GetBase64BitsFromChar(67), GetBase64BitsFromChar(68), GetBase64BitsFromChar(69), GetBase64BitsFromChar(70), GetBase64BitsFromChar(71), GetBase64BitsFromChar(72), GetBase64BitsFromChar(73), GetBase64BitsFromChar(74), GetBase64BitsFromChar(75), GetBase64BitsFromChar(76), GetBase64BitsFromChar(77), GetBase64BitsFromChar(78), GetBase64BitsFromChar(79), GetBase64BitsFromChar(80), GetBase64BitsFromChar(81), GetBase64BitsFromChar(82), GetBase64BitsFromChar(83), GetBase64BitsFromChar(84), GetBase64BitsFromChar(85), GetBase64BitsFromChar(86), GetBase64BitsFromChar(87), GetBase64BitsFromChar(88), GetBase64BitsFromChar(89), GetBase64BitsFromChar(90), GetBase64BitsFromChar(91), GetBase64BitsFromChar(92), GetBase64BitsFromChar(93), GetBase64BitsFromChar(94), GetBase64BitsFromChar(95), GetBase64BitsFromChar(96), GetBase64BitsFromChar(97), GetBase64BitsFromChar(98), GetBase64BitsFromChar(99), GetBase64BitsFromChar(100), GetBase64BitsFromChar(101), GetBase64BitsFromChar(102), GetBase64BitsFromChar(103), GetBase64BitsFromChar(104), GetBase64BitsFromChar(105), GetBase64BitsFromChar(106), GetBase64BitsFromChar(107), GetBase64BitsFromChar(108), GetBase64BitsFromChar(109), GetBase64BitsFromChar(110), GetBase64BitsFromChar(111), GetBase64BitsFromChar(112), GetBase64BitsFromChar(113), GetBase64BitsFromChar(114), GetBase64BitsFromChar(115), GetBase64BitsFromChar(116), GetBase64BitsFromChar(117), GetBase64BitsFromChar(118), GetBase64BitsFromChar(119), GetBase64BitsFromChar(120), GetBase64BitsFromChar(121), GetBase64BitsFromChar(122), GetBase64BitsFromChar(123), GetBase64BitsFromChar(124), GetBase64BitsFromChar(125), GetBase64BitsFromChar(126), GetBase64BitsFromChar(127), GetBase64BitsFromChar(128), GetBase64BitsFromChar(129), GetBase64BitsFromChar(130), GetBase64BitsFromChar(131), GetBase64BitsFromChar(132), GetBase64BitsFromChar(133), GetBase64BitsFromChar(134), GetBase64BitsFromChar(135), GetBase64BitsFromChar(136), GetBase64BitsFromChar(137), GetBase64BitsFromChar(138), GetBase64BitsFromChar(139), GetBase64BitsFromChar(140), GetBase64BitsFromChar(141), GetBase64BitsFromChar(142), GetBase64BitsFromChar(143), GetBase64BitsFromChar(144), GetBase64BitsFromChar(145), GetBase64BitsFromChar(146), GetBase64BitsFromChar(147), GetBase64BitsFromChar(148), GetBase64BitsFromChar(149), GetBase64BitsFromChar(150), GetBase64BitsFromChar(151), GetBase64BitsFromChar(152), GetBase64BitsFromChar(153), GetBase64BitsFromChar(154), GetBase64BitsFromChar(155), GetBase64BitsFromChar(156), GetBase64BitsFromChar(157), GetBase64BitsFromChar(158), GetBase64BitsFromChar(159), GetBase64BitsFromChar(160), GetBase64BitsFromChar(161), GetBase64BitsFromChar(162), GetBase64BitsFromChar(163), GetBase64BitsFromChar(164), GetBase64BitsFromChar(165), GetBase64BitsFromChar(166), GetBase64BitsFromChar(167), GetBase64BitsFromChar(168), GetBase64BitsFromChar(169), GetBase64BitsFromChar(170), GetBase64BitsFromChar(171), GetBase64BitsFromChar(172), GetBase64BitsFromChar(173), GetBase64BitsFromChar(174), GetBase64BitsFromChar(175), GetBase64BitsFromChar(176), GetBase64BitsFromChar(177), GetBase64BitsFromChar(178), GetBase64BitsFromChar(179), GetBase64BitsFromChar(180), GetBase64BitsFromChar(181), GetBase64BitsFromChar(182), GetBase64BitsFromChar(183), GetBase64BitsFromChar(184), GetBase64BitsFromChar(185), GetBase64BitsFromChar(186), GetBase64BitsFromChar(187), GetBase64BitsFromChar(188), GetBase64BitsFromChar(189), GetBase64BitsFromChar(190), GetBase64BitsFromChar(191), GetBase64BitsFromChar(192), GetBase64BitsFromChar(193), GetBase64BitsFromChar(194), GetBase64BitsFromChar(195), GetBase64BitsFromChar(196), GetBase64BitsFromChar(197), GetBase64BitsFromChar(198), GetBase64BitsFromChar(199), GetBase64BitsFromChar(200), GetBase64BitsFromChar(201), GetBase64BitsFromChar(202), GetBase64BitsFromChar(203), GetBase64BitsFromChar(204), GetBase64BitsFromChar(205), GetBase64BitsFromChar(206), GetBase64BitsFromChar(207), GetBase64BitsFromChar(208), GetBase64BitsFromChar(209), GetBase64BitsFromChar(210), GetBase64BitsFromChar(211), GetBase64BitsFromChar(212), GetBase64BitsFromChar(213), GetBase64BitsFromChar(214), GetBase64BitsFromChar(215), GetBase64BitsFromChar(216), GetBase64BitsFromChar(217), GetBase64BitsFromChar(218), GetBase64BitsFromChar(219), GetBase64BitsFromChar(220), GetBase64BitsFromChar(221), GetBase64BitsFromChar(222), GetBase64BitsFromChar(223), GetBase64BitsFromChar(224), GetBase64BitsFromChar(225), GetBase64BitsFromChar(226), GetBase64BitsFromChar(227), GetBase64BitsFromChar(228), GetBase64BitsFromChar(229), GetBase64BitsFromChar(230), GetBase64BitsFromChar(231), GetBase64BitsFromChar(232), GetBase64BitsFromChar(233), GetBase64BitsFromChar(234), GetBase64BitsFromChar(235), GetBase64BitsFromChar(236), GetBase64BitsFromChar(237), GetBase64BitsFromChar(238), GetBase64BitsFromChar(239), GetBase64BitsFromChar(240), GetBase64BitsFromChar(241), GetBase64BitsFromChar(242), GetBase64BitsFromChar(243), GetBase64BitsFromChar(244), GetBase64BitsFromChar(245), GetBase64BitsFromChar(246), GetBase64BitsFromChar(247), GetBase64BitsFromChar(248), GetBase64BitsFromChar(249), GetBase64BitsFromChar(250), GetBase64BitsFromChar(251), GetBase64BitsFromChar(252), GetBase64BitsFromChar(253), GetBase64BitsFromChar(254), GetBase64BitsFromChar(255)};
static_assert(sizeof(BASE64_BITS_FROM_CHAR) == 256);

//static constexpr char BASE64_FS_SAFE_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
//static_assert(sizeof(BASE64_FS_SAFE_ALPHABET) - 1 == 64);

static constexpr char BASE64_PAD_CHAR = '=';

// Original data:
//
//  23  22  21  20  19  18  17  16   15  14  13  12  11  10  9   8    7   6   5   4   3   2   1   0
// +---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---+
// | A | B | C | D | E | F | G | H || I | J | K | L | M | N | O | P || Q | R | S | T | U | V | W | X |
// +---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---+
//
// Encoded data:
//
//  23  22  21  20  19  18   17  16  15  14  13  12   11  10  9   8   7   6    5   4   3   2   1   0
// +---+---+---+---+---+---++---+---+---+---+---+---++---+---+---+---+---+---++---+---+---+---+---+---+
// | A | B | C | D | E | F || G | H | I | J | K | L || M | N | O | P | Q | R || S | T | U | V | W | X |
// +---+---+---+---+---+---++---+---+---+---+---+---++---+---+---+---+---+---++---+---+---+---+---+---+

static std::string Base64Encode2(const std::vector<uint8_t> &data, const char *alphabet) {
    std::string result;

    size_t i = 0;

    if (data.size() >= 3) {
        for (; i < data.size() - 2; i += 3) {

            uint8_t d0 = data[i + 0]; // abcdefgh
            uint8_t d1 = data[i + 1]; // ijklmnop
            uint8_t d2 = data[i + 2]; // qrstuvwx

            result.push_back(alphabet[d0 >> 2]);                       //abcdef
            result.push_back(alphabet[((d0 & 0x3) << 4) | (d1 >> 4)]); //gh|ijkl
            result.push_back(alphabet[((d1 & 0xf) << 2) | (d2 >> 6)]); //mnop|qr
            result.push_back(alphabet[d2 & 0x3f]);                     //stuvwx
        }
    }

    if (i == data.size() - 2) {
        uint8_t d0 = data[i + 0]; //abcdefgh
        uint8_t d1 = data[i + 1]; //ijklmnop

        result.push_back(alphabet[d0 >> 2]);                       //abcdef
        result.push_back(alphabet[((d0 & 0x3) << 4) | (d1 >> 4)]); //gh|ijkl
        result.push_back(alphabet[(d1 & 0xf) << 2]);               //mnop|00

        result.push_back(BASE64_PAD_CHAR);
    } else if (i == data.size() - 1) {
        uint8_t d0 = data[i + 0];

        result.push_back(alphabet[d0 >> 2]);
        result.push_back(alphabet[(d0 & 0x3) << 4]);

        result.push_back(BASE64_PAD_CHAR);
        result.push_back(BASE64_PAD_CHAR);
    } else {
        ASSERT(i == data.size());
    }

    return result;
}

std::string Base64Encode(const std::vector<uint8_t> &data) {
    return Base64Encode2(data, BASE64_ALPHABET);
}

bool Base64Decode(std::vector<uint8_t> *data, const std::string &str, const LogSet *logs) {
    if (str.size() % 4 != 0) {
        if (logs) {
            logs->e.f("invalid length for base64 data: %zu\n", str.size());
        }

        return false;
    }

    size_t i = 0;

    if (str.size() > 4) {
        for (; i < str.size() - 4; i += 4) {
            int8_t c0 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 0]]; //abcdef
            if (c0 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 0]);
                }
                return false;
            }

            int8_t c1 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 1]]; //ghijkl
            if (c1 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 1]);
                }
                return false;
            }

            int8_t c2 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 2]]; //mnopqr
            if (c2 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 2]);
                }
                return false;
            }

            int8_t c3 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 3]]; //stuvwx
            if (c3 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 3]);
                }
                return false;
            }

            data->push_back((uint8_t)c0 << 2 | (uint8_t)c1 >> 4);   //abcdef|gh
            data->push_back((uint8_t)(c1 << 4) | (uint8_t)c2 >> 2); //ijkl|mnop
            data->push_back((uint8_t)(c2 << 6) | (uint8_t)c3);      //qr|stuvwx
        }
    }

    if (str[i + 3] == BASE64_PAD_CHAR) {
        // 1 or 2 pad chars
        if (str[i + 2] == BASE64_PAD_CHAR) {
            // 2 pad chars - 1 byte
            int8_t c0 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 0]]; //abcdef
            if (c0 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 0]);
                }
                return false;
            }

            int8_t c1 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 1]]; //ghijkl
            if (c1 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 1]);
                }
                return false;
            }

            if ((c1 & 0xf) != 0) {
                if (logs) {
                    logs->e.f("invalid padded base64 char: %c\n", str[i + 1]);
                }
                return false;
            }

            data->push_back((uint8_t)c0 << 2 | (uint8_t)c1 >> 4); //abcdef|gh
        } else {
            // 1 pad char - 2 bytes
            int8_t c0 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 0]]; //abcdef
            if (c0 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 0]);
                }
                return false;
            }

            int8_t c1 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 1]]; //ghijkl
            if (c1 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 1]);
                }
                return false;
            }

            int8_t c2 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 2]]; //mnopqr
            if (c2 < 0) {
                if (logs) {
                    logs->e.f("invalid base64 char: %d\n", str[i + 2]);
                }
                return false;
            }

            if ((c2 & 3) != 0) {
                if (logs) {
                    logs->e.f("invalid padded base64 char: %c\n", str[i + 1]);
                }
                return false;
            }

            data->push_back((uint8_t)c0 << 2 | (uint8_t)c1 >> 4);   //abcdef|gh
            data->push_back((uint8_t)(c1 << 4) | (uint8_t)c2 >> 2); //ijkl|mnop
        }
    } else {
        // No pad chars - 3 bytes
        int8_t c0 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 0]]; //abcdef
        if (c0 < 0) {
            if (logs) {
                logs->e.f("invalid base64 char: %d\n", str[i + 0]);
            }
            return false;
        }

        int8_t c1 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 1]]; //ghijkl
        if (c1 < 0) {
            if (logs) {
                logs->e.f("invalid base64 char: %d\n", str[i + 1]);
            }
            return false;
        }

        int8_t c2 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 2]]; //mnopqr
        if (c2 < 0) {
            if (logs) {
                logs->e.f("invalid base64 char: %d\n", str[i + 2]);
            }
            return false;
        }

        int8_t c3 = BASE64_BITS_FROM_CHAR[(uint8_t)str[i + 3]]; //stuvwx
        if (c3 < 0) {
            if (logs) {
                logs->e.f("invalid base64 char: %d\n", str[i + 3]);
            }
            return false;
        }

        data->push_back((uint8_t)c0 << 2 | (uint8_t)c1 >> 4);   //abcdef|gh
        data->push_back((uint8_t)(c1 << 4) | (uint8_t)c2 >> 2); //ijkl|mnop
        data->push_back((uint8_t)(c2 << 6) | (uint8_t)c3);      //qr|stuvwx
    }

    return true;
}
