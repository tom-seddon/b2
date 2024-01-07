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
static bool GetValueFromString(T *value, const char *str, int radix) {
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
    if (*ep != 0 && !isspace(*ep)) {
        return false;
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

bool GetUInt8FromString(uint8_t *value, const std::string &str, int radix) {
    return GetUInt8FromString(value, str.c_str(), radix);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt8FromString(uint8_t *value, const char *str, int radix) {
    return GetValueFromString(value, str, radix);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt16FromString(uint16_t *value, const std::string &str, int radix) {
    return GetUInt16FromString(value, str.c_str(), radix);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt16FromString(uint16_t *value, const char *str, int radix) {
    return GetValueFromString(value, str, radix);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt32FromString(uint32_t *value, const std::string &str, int radix) {
    return GetUInt32FromString(value, str.c_str(), radix);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt32FromString(uint32_t *value, const char *str, int radix) {
    return GetValueFromString(value, str, radix);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt64FromString(uint64_t *value, const std::string &str, int radix) {
    return GetUInt64FromString(value, str.c_str(), radix);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetUInt64FromString(uint64_t *value, const char *str, int radix) {
    return GetValueFromString(value, str, radix);
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

static int GetBBCChar(uint32_t codepoint) {
    if (codepoint == 13 || codepoint == 10) {
        return (int)codepoint;
    } else if (codepoint >= 32 && codepoint <= 126) {
        return (int)codepoint;
    } else if (codepoint == 0xa3) {
        return 95; //GBP symbol
    } else {
        return -1;
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

    ascii->clear();
    size_t char_start = 0;

    uint32_t bad_codepoint = 0;
    const uint8_t *bad_char_start = nullptr;
    int bad_char_len = 0;

    for (size_t i = 0; i < data.size(); ++i) {
        decode(&state, &codepoint, data[i]);
        if (state == UTF8_ACCEPT) {
            int c = GetBBCChar(codepoint);
            if (c < 0) {
                bad_codepoint = codepoint;
                bad_char_start = &data[char_start];
                bad_char_len = (int)(i - char_start);

                goto bad;
            }

            ascii->push_back((char)c);
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

uint32_t GetBBCASCIIFromISO88511(std::string *ascii, const std::vector<uint8_t> &data) {
    ascii->clear();

    for (uint8_t x : data) {
        int c = GetBBCChar(x);
        if (c < 0) {
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

std::string GetUTF8FromBBCASCII(const std::vector<uint8_t> &data) {
    // Normalize line endings and strip out control codes.
    //
    // TODO: do it in a less dumb fashion.
    std::string utf8;
    utf8.reserve(data.size());

    for (size_t i = 0; i < data.size(); ++i) {
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
        } else if (data[i] == 95) {
            // Translate pound sign into UTF8.
            utf8 += POUND_SIGN_UTF8;
        } else if (data[i] < 127) {
            // Pass other printable 7-bit ASCII chars straight through.
            utf8.push_back((char)data[i]);
        } else {
            // Discard DEL and 128+. TODO.
        }
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
