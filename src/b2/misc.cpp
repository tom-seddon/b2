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
#include "Messages.h"
#include <shared/debug.h>
#include <unordered_map>

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

std::string strprintf(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    std::string result = strprintfv(fmt, v);
    va_end(v);

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintfv(const char *fmt, va_list v) {
    char *str;
    if (vasprintf(&str, fmt, v) == -1) {
        // Better suggestions welcome... please.
        return std::string("vasprintf failed - ") + strerror(errno) + " (" + std::to_string(errno) + ")";
    } else {
        std::string result(str);

        free(str);
        str = NULL;

        return result;
    }
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

std::string GetCloneImpedimentsDescription(uint32_t impediments) {
    if (impediments == 0) {
        return "none";
    } else {
        std::string r;
        for (int i = 0; i < NUM_DRIVES; ++i) {
            if (impediments & (uint32_t)BBCMicroCloneImpediment_Drive0 << i) {
                if (!r.empty()) {
                    r += ", ";
                }

                r += strprintf("drive %d", i);
            }
        }

        if (impediments & BBCMicroCloneImpediment_BeebLink) {
            if (!r.empty()) {
                r += ", ";
            }

            r += "BeebLink";
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

SDL_PixelFormat *ClonePixelFormat(const SDL_PixelFormat *pixel_format) {
    return SDL_AllocFormat(pixel_format->format);
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

void ForEachLine(const std::string &str, std::function<void(const std::string::const_iterator &a, const std::string::const_iterator &b)> fun) {
    std::string::const_iterator a = str.begin(), b = a;
    while (b != str.end()) {
        char c = *b;
        if (c == '\r' || c == '\n') {
            fun(a, b);

            ++b;
            if (b != str.end()) {
                if ((*b == '\r' || *b == '\n') && *b != c) {
                    ++b;
                }
            }

            a = b;
        } else {
            ++b;
        }
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
        // Handle &/$.

        if (*c == '$' || *c == '&') {
            c++;
            radix = 16;
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

static std::unordered_map<uint32_t, char> g_bbc_char_by_codepoint;
static std::string g_utf8_char_by_bbc_char[BBCUTF8ConvertMode_Count][128];
static bool g_utf8_convert_tables_initialised = false;

static uint32_t GetCodePointForBBCChar(uint8_t bbc_char, BBCUTF8ConvertMode mode) {
    ASSERT(bbc_char >= 32 && bbc_char < 127);

    switch (mode) {
    default:
        ASSERT(false);
        // fall through
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

static std::string GetUTF8StringForCodePoint(uint32_t u) {
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
                    g_bbc_char_by_codepoint[u] = (char)c;
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

bool GetBBCASCIIFromUTF8(std::string *ascii,
                         const std::vector<uint8_t> &data,
                         uint32_t *bad_codepoint_ptr,
                         const uint8_t **bad_char_start_ptr,
                         int *bad_char_len_ptr) {
    uint32_t state = UTF8_ACCEPT, codepoint;

    InitUTF8ConvertTables();

    ascii->clear();
    size_t char_start = 0;

    uint32_t bad_codepoint = 0;
    const uint8_t *bad_char_start = nullptr;
    int bad_char_len = 0;

    for (size_t i = 0; i < data.size(); ++i) {
        decode(&state, &codepoint, data[i]);
        if (state == UTF8_ACCEPT) {
            auto &&it = g_bbc_char_by_codepoint.find(codepoint);
            if (it == g_bbc_char_by_codepoint.end()) {
                bad_codepoint = codepoint;
                bad_char_start = &data[char_start];
                bad_char_len = (int)(i - char_start);

                goto bad;
            }

            ascii->push_back(it->second);
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
        *bad_char_start_ptr = bad_char_start;
    }

    if (bad_char_len_ptr) {
        *bad_char_len_ptr = bad_char_len;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t GetBBCASCIIFromISO8859_1(std::string *ascii, const std::vector<uint8_t> &data) {
    ascii->clear();

    for (uint8_t x : data) {
        if (x >= 32 && x <= 126) {
            // ok...
        } else if (x == 0xa3) {
            // GBP
            x = '`';
        } else {
            return x;
        }

        ascii->push_back((char)x);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t VDU_CODE_LENGTHS[32] = {
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
            i += 1u + VDU_CODE_LENGTHS[data[i]];
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

void FixBBCASCIINewlines(std::string *str) {
    // Knobble newlines.
    if (str->size() > 1) {
        std::string::size_type i = 0;

        while (i < str->size() - 1) {
            if ((*str)[i] == 10 && (*str)[i + 1] == 13) {
                str->erase(i, 1);
            } else if ((*str)[i] == 13 && (*str)[i + 1] == 10) {
                ++i;
                str->erase(i, 1);
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
