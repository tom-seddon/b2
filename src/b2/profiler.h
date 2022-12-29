#ifndef HEADER_D15923544A40435AA4A723DBDB41207C // -*- mode:c++ -*-
#define HEADER_D15923544A40435AA4A723DBDB41207C

// Prerequisites are <shared/system_specific.h>

#if BUILD_TYPE_RelWithDebInfo
#define PROFILER_ENABLED (1)
#else
// Don't want it in Final, and hardly seems worth it in Debug...
#define PROFILER_ENABLED (0)
#endif

#include <shared/enum_decl.h>
#include "profiler.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

union ProfilerColour {
    struct {
#if CPU_LITTLE_ENDIAN
        uint8_t b, g, r, a;
#else
#error Untested...
#endif
    } rgba;
    uint32_t value;
};

#define PROFILER_COLOUR(R, G, B) \
    {                            \
        { (B), (G), (R), 0xff }  \
    }

// https://en.wikipedia.org/wiki/X11_color_names#Color_name_chart
static constexpr ProfilerColour PROFILER_COLOUR_ALICE_BLUE = PROFILER_COLOUR(0xF0, 0xF8, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_ANTIQUE_WHITE = PROFILER_COLOUR(0xFA, 0xEB, 0xD7);
static constexpr ProfilerColour PROFILER_COLOUR_AQUA = PROFILER_COLOUR(0x00, 0xFF, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_AQUAMARINE = PROFILER_COLOUR(0x7F, 0xFF, 0xD4);
static constexpr ProfilerColour PROFILER_COLOUR_AZURE = PROFILER_COLOUR(0xF0, 0xFF, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_BEIGE = PROFILER_COLOUR(0xF5, 0xF5, 0xDC);
static constexpr ProfilerColour PROFILER_COLOUR_BISQUE = PROFILER_COLOUR(0xFF, 0xE4, 0xC4);
static constexpr ProfilerColour PROFILER_COLOUR_BLACK = PROFILER_COLOUR(0x00, 0x00, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_BLANCHED_ALMOND = PROFILER_COLOUR(0xFF, 0xEB, 0xCD);
static constexpr ProfilerColour PROFILER_COLOUR_BLUE = PROFILER_COLOUR(0x00, 0x00, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_BLUE_VIOLET = PROFILER_COLOUR(0x8A, 0x2B, 0xE2);
static constexpr ProfilerColour PROFILER_COLOUR_BROWN = PROFILER_COLOUR(0xA5, 0x2A, 0x2A);
static constexpr ProfilerColour PROFILER_COLOUR_BURLYWOOD = PROFILER_COLOUR(0xDE, 0xB8, 0x87);
static constexpr ProfilerColour PROFILER_COLOUR_CADET_BLUE = PROFILER_COLOUR(0x5F, 0x9E, 0xA0);
static constexpr ProfilerColour PROFILER_COLOUR_CHARTREUSE = PROFILER_COLOUR(0x7F, 0xFF, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_CHOCOLATE = PROFILER_COLOUR(0xD2, 0x69, 0x1E);
static constexpr ProfilerColour PROFILER_COLOUR_CORAL = PROFILER_COLOUR(0xFF, 0x7F, 0x50);
static constexpr ProfilerColour PROFILER_COLOUR_CORNFLOWER_BLUE = PROFILER_COLOUR(0x64, 0x95, 0xED);
static constexpr ProfilerColour PROFILER_COLOUR_CORNSILK = PROFILER_COLOUR(0xFF, 0xF8, 0xDC);
static constexpr ProfilerColour PROFILER_COLOUR_CRIMSON = PROFILER_COLOUR(0xDC, 0x14, 0x3C);
static constexpr ProfilerColour PROFILER_COLOUR_CYAN = PROFILER_COLOUR(0x00, 0xFF, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_BLUE = PROFILER_COLOUR(0x00, 0x00, 0x8B);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_CYAN = PROFILER_COLOUR(0x00, 0x8B, 0x8B);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_GOLDENROD = PROFILER_COLOUR(0xB8, 0x86, 0x0B);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_GRAY = PROFILER_COLOUR(0xA9, 0xA9, 0xA9);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_GREEN = PROFILER_COLOUR(0x00, 0x64, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_KHAKI = PROFILER_COLOUR(0xBD, 0xB7, 0x6B);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_MAGENTA = PROFILER_COLOUR(0x8B, 0x00, 0x8B);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_OLIVE_GREEN = PROFILER_COLOUR(0x55, 0x6B, 0x2F);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_ORANGE = PROFILER_COLOUR(0xFF, 0x8C, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_ORCHID = PROFILER_COLOUR(0x99, 0x32, 0xCC);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_RED = PROFILER_COLOUR(0x8B, 0x00, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_SALMON = PROFILER_COLOUR(0xE9, 0x96, 0x7A);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_SEA_GREEN = PROFILER_COLOUR(0x8F, 0xBC, 0x8F);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_SLATE_BLUE = PROFILER_COLOUR(0x48, 0x3D, 0x8B);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_SLATE_GRAY = PROFILER_COLOUR(0x2F, 0x4F, 0x4F);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_TURQUOISE = PROFILER_COLOUR(0x00, 0xCE, 0xD1);
static constexpr ProfilerColour PROFILER_COLOUR_DARK_VIOLET = PROFILER_COLOUR(0x94, 0x00, 0xD3);
static constexpr ProfilerColour PROFILER_COLOUR_DEEP_PINK = PROFILER_COLOUR(0xFF, 0x14, 0x93);
static constexpr ProfilerColour PROFILER_COLOUR_DEEP_SKY_BLUE = PROFILER_COLOUR(0x00, 0xBF, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_DIM_GRAY = PROFILER_COLOUR(0x69, 0x69, 0x69);
static constexpr ProfilerColour PROFILER_COLOUR_DODGER_BLUE = PROFILER_COLOUR(0x1E, 0x90, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_FIREBRICK = PROFILER_COLOUR(0xB2, 0x22, 0x22);
static constexpr ProfilerColour PROFILER_COLOUR_FLORAL_WHITE = PROFILER_COLOUR(0xFF, 0xFA, 0xF0);
static constexpr ProfilerColour PROFILER_COLOUR_FOREST_GREEN = PROFILER_COLOUR(0x22, 0x8B, 0x22);
static constexpr ProfilerColour PROFILER_COLOUR_FUCHSIA = PROFILER_COLOUR(0xFF, 0x00, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_GAINSBORO = PROFILER_COLOUR(0xDC, 0xDC, 0xDC);
static constexpr ProfilerColour PROFILER_COLOUR_GHOST_WHITE = PROFILER_COLOUR(0xF8, 0xF8, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_GOLD = PROFILER_COLOUR(0xFF, 0xD7, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_GOLDENROD = PROFILER_COLOUR(0xDA, 0xA5, 0x20);
static constexpr ProfilerColour PROFILER_COLOUR_GRAY = PROFILER_COLOUR(0xBE, 0xBE, 0xBE);
static constexpr ProfilerColour PROFILER_COLOUR_WEB_GRAY = PROFILER_COLOUR(0x80, 0x80, 0x80);
static constexpr ProfilerColour PROFILER_COLOUR_GREEN = PROFILER_COLOUR(0x00, 0xFF, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_WEB_GREEN = PROFILER_COLOUR(0x00, 0x80, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_GREEN_YELLOW = PROFILER_COLOUR(0xAD, 0xFF, 0x2F);
static constexpr ProfilerColour PROFILER_COLOUR_HONEYDEW = PROFILER_COLOUR(0xF0, 0xFF, 0xF0);
static constexpr ProfilerColour PROFILER_COLOUR_HOT_PINK = PROFILER_COLOUR(0xFF, 0x69, 0xB4);
static constexpr ProfilerColour PROFILER_COLOUR_INDIAN_RED = PROFILER_COLOUR(0xCD, 0x5C, 0x5C);
static constexpr ProfilerColour PROFILER_COLOUR_INDIGO = PROFILER_COLOUR(0x4B, 0x00, 0x82);
static constexpr ProfilerColour PROFILER_COLOUR_IVORY = PROFILER_COLOUR(0xFF, 0xFF, 0xF0);
static constexpr ProfilerColour PROFILER_COLOUR_KHAKI = PROFILER_COLOUR(0xF0, 0xE6, 0x8C);
static constexpr ProfilerColour PROFILER_COLOUR_LAVENDER = PROFILER_COLOUR(0xE6, 0xE6, 0xFA);
static constexpr ProfilerColour PROFILER_COLOUR_LAVENDER_BLUSH = PROFILER_COLOUR(0xFF, 0xF0, 0xF5);
static constexpr ProfilerColour PROFILER_COLOUR_LAWN_GREEN = PROFILER_COLOUR(0x7C, 0xFC, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_LEMON_CHIFFON = PROFILER_COLOUR(0xFF, 0xFA, 0xCD);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_BLUE = PROFILER_COLOUR(0xAD, 0xD8, 0xE6);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_CORAL = PROFILER_COLOUR(0xF0, 0x80, 0x80);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_CYAN = PROFILER_COLOUR(0xE0, 0xFF, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_GOLDENROD = PROFILER_COLOUR(0xFA, 0xFA, 0xD2);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_GRAY = PROFILER_COLOUR(0xD3, 0xD3, 0xD3);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_GREEN = PROFILER_COLOUR(0x90, 0xEE, 0x90);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_PINK = PROFILER_COLOUR(0xFF, 0xB6, 0xC1);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_SALMON = PROFILER_COLOUR(0xFF, 0xA0, 0x7A);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_SEA_GREEN = PROFILER_COLOUR(0x20, 0xB2, 0xAA);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_SKY_BLUE = PROFILER_COLOUR(0x87, 0xCE, 0xFA);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_SLATE_GRAY = PROFILER_COLOUR(0x77, 0x88, 0x99);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_STEEL_BLUE = PROFILER_COLOUR(0xB0, 0xC4, 0xDE);
static constexpr ProfilerColour PROFILER_COLOUR_LIGHT_YELLOW = PROFILER_COLOUR(0xFF, 0xFF, 0xE0);
static constexpr ProfilerColour PROFILER_COLOUR_LIME = PROFILER_COLOUR(0x00, 0xFF, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_LIME_GREEN = PROFILER_COLOUR(0x32, 0xCD, 0x32);
static constexpr ProfilerColour PROFILER_COLOUR_LINEN = PROFILER_COLOUR(0xFA, 0xF0, 0xE6);
static constexpr ProfilerColour PROFILER_COLOUR_MAGENTA = PROFILER_COLOUR(0xFF, 0x00, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_MAROON = PROFILER_COLOUR(0xB0, 0x30, 0x60);
static constexpr ProfilerColour PROFILER_COLOUR_WEB_MAROON = PROFILER_COLOUR(0x80, 0x00, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_AQUAMARINE = PROFILER_COLOUR(0x66, 0xCD, 0xAA);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_BLUE = PROFILER_COLOUR(0x00, 0x00, 0xCD);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_ORCHID = PROFILER_COLOUR(0xBA, 0x55, 0xD3);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_PURPLE = PROFILER_COLOUR(0x93, 0x70, 0xDB);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_SEA_GREEN = PROFILER_COLOUR(0x3C, 0xB3, 0x71);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_SLATE_BLUE = PROFILER_COLOUR(0x7B, 0x68, 0xEE);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_SPRING_GREEN = PROFILER_COLOUR(0x00, 0xFA, 0x9A);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_TURQUOISE = PROFILER_COLOUR(0x48, 0xD1, 0xCC);
static constexpr ProfilerColour PROFILER_COLOUR_MEDIUM_VIOLET_RED = PROFILER_COLOUR(0xC7, 0x15, 0x85);
static constexpr ProfilerColour PROFILER_COLOUR_MIDNIGHT_BLUE = PROFILER_COLOUR(0x19, 0x19, 0x70);
static constexpr ProfilerColour PROFILER_COLOUR_MINT_CREAM = PROFILER_COLOUR(0xF5, 0xFF, 0xFA);
static constexpr ProfilerColour PROFILER_COLOUR_MISTY_ROSE = PROFILER_COLOUR(0xFF, 0xE4, 0xE1);
static constexpr ProfilerColour PROFILER_COLOUR_MOCCASIN = PROFILER_COLOUR(0xFF, 0xE4, 0xB5);
static constexpr ProfilerColour PROFILER_COLOUR_NAVAJO_WHITE = PROFILER_COLOUR(0xFF, 0xDE, 0xAD);
static constexpr ProfilerColour PROFILER_COLOUR_NAVY_BLUE = PROFILER_COLOUR(0x00, 0x00, 0x80);
static constexpr ProfilerColour PROFILER_COLOUR_OLD_LACE = PROFILER_COLOUR(0xFD, 0xF5, 0xE6);
static constexpr ProfilerColour PROFILER_COLOUR_OLIVE = PROFILER_COLOUR(0x80, 0x80, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_OLIVE_DRAB = PROFILER_COLOUR(0x6B, 0x8E, 0x23);
static constexpr ProfilerColour PROFILER_COLOUR_ORANGE = PROFILER_COLOUR(0xFF, 0xA5, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_ORANGE_RED = PROFILER_COLOUR(0xFF, 0x45, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_ORCHID = PROFILER_COLOUR(0xDA, 0x70, 0xD6);
static constexpr ProfilerColour PROFILER_COLOUR_PALE_GOLDENROD = PROFILER_COLOUR(0xEE, 0xE8, 0xAA);
static constexpr ProfilerColour PROFILER_COLOUR_PALE_GREEN = PROFILER_COLOUR(0x98, 0xFB, 0x98);
static constexpr ProfilerColour PROFILER_COLOUR_PALE_TURQUOISE = PROFILER_COLOUR(0xAF, 0xEE, 0xEE);
static constexpr ProfilerColour PROFILER_COLOUR_PALE_VIOLET_RED = PROFILER_COLOUR(0xDB, 0x70, 0x93);
static constexpr ProfilerColour PROFILER_COLOUR_PAPAYA_WHIP = PROFILER_COLOUR(0xFF, 0xEF, 0xD5);
static constexpr ProfilerColour PROFILER_COLOUR_PEACH_PUFF = PROFILER_COLOUR(0xFF, 0xDA, 0xB9);
static constexpr ProfilerColour PROFILER_COLOUR_PERU = PROFILER_COLOUR(0xCD, 0x85, 0x3F);
static constexpr ProfilerColour PROFILER_COLOUR_PINK = PROFILER_COLOUR(0xFF, 0xC0, 0xCB);
static constexpr ProfilerColour PROFILER_COLOUR_PLUM = PROFILER_COLOUR(0xDD, 0xA0, 0xDD);
static constexpr ProfilerColour PROFILER_COLOUR_POWDER_BLUE = PROFILER_COLOUR(0xB0, 0xE0, 0xE6);
static constexpr ProfilerColour PROFILER_COLOUR_PURPLE = PROFILER_COLOUR(0xA0, 0x20, 0xF0);
static constexpr ProfilerColour PROFILER_COLOUR_WEB_PURPLE = PROFILER_COLOUR(0x80, 0x00, 0x80);
static constexpr ProfilerColour PROFILER_COLOUR_REBECCA_PURPLE = PROFILER_COLOUR(0x66, 0x33, 0x99);
static constexpr ProfilerColour PROFILER_COLOUR_RED = PROFILER_COLOUR(0xFF, 0x00, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_ROSY_BROWN = PROFILER_COLOUR(0xBC, 0x8F, 0x8F);
static constexpr ProfilerColour PROFILER_COLOUR_ROYAL_BLUE = PROFILER_COLOUR(0x41, 0x69, 0xE1);
static constexpr ProfilerColour PROFILER_COLOUR_SADDLE_BROWN = PROFILER_COLOUR(0x8B, 0x45, 0x13);
static constexpr ProfilerColour PROFILER_COLOUR_SALMON = PROFILER_COLOUR(0xFA, 0x80, 0x72);
static constexpr ProfilerColour PROFILER_COLOUR_SANDY_BROWN = PROFILER_COLOUR(0xF4, 0xA4, 0x60);
static constexpr ProfilerColour PROFILER_COLOUR_SEA_GREEN = PROFILER_COLOUR(0x2E, 0x8B, 0x57);
static constexpr ProfilerColour PROFILER_COLOUR_SEASHELL = PROFILER_COLOUR(0xFF, 0xF5, 0xEE);
static constexpr ProfilerColour PROFILER_COLOUR_SIENNA = PROFILER_COLOUR(0xA0, 0x52, 0x2D);
static constexpr ProfilerColour PROFILER_COLOUR_SILVER = PROFILER_COLOUR(0xC0, 0xC0, 0xC0);
static constexpr ProfilerColour PROFILER_COLOUR_SKY_BLUE = PROFILER_COLOUR(0x87, 0xCE, 0xEB);
static constexpr ProfilerColour PROFILER_COLOUR_SLATE_BLUE = PROFILER_COLOUR(0x6A, 0x5A, 0xCD);
static constexpr ProfilerColour PROFILER_COLOUR_SLATE_GRAY = PROFILER_COLOUR(0x70, 0x80, 0x90);
static constexpr ProfilerColour PROFILER_COLOUR_SNOW = PROFILER_COLOUR(0xFF, 0xFA, 0xFA);
static constexpr ProfilerColour PROFILER_COLOUR_SPRING_GREEN = PROFILER_COLOUR(0x00, 0xFF, 0x7F);
static constexpr ProfilerColour PROFILER_COLOUR_STEEL_BLUE = PROFILER_COLOUR(0x46, 0x82, 0xB4);
static constexpr ProfilerColour PROFILER_COLOUR_TAN = PROFILER_COLOUR(0xD2, 0xB4, 0x8C);
static constexpr ProfilerColour PROFILER_COLOUR_TEAL = PROFILER_COLOUR(0x00, 0x80, 0x80);
static constexpr ProfilerColour PROFILER_COLOUR_THISTLE = PROFILER_COLOUR(0xD8, 0xBF, 0xD8);
static constexpr ProfilerColour PROFILER_COLOUR_TOMATO = PROFILER_COLOUR(0xFF, 0x63, 0x47);
static constexpr ProfilerColour PROFILER_COLOUR_TURQUOISE = PROFILER_COLOUR(0x40, 0xE0, 0xD0);
static constexpr ProfilerColour PROFILER_COLOUR_VIOLET = PROFILER_COLOUR(0xEE, 0x82, 0xEE);
static constexpr ProfilerColour PROFILER_COLOUR_WHEAT = PROFILER_COLOUR(0xF5, 0xDE, 0xB3);
static constexpr ProfilerColour PROFILER_COLOUR_WHITE = PROFILER_COLOUR(0xFF, 0xFF, 0xFF);
static constexpr ProfilerColour PROFILER_COLOUR_WHITE_SMOKE = PROFILER_COLOUR(0xF5, 0xF5, 0xF5);
static constexpr ProfilerColour PROFILER_COLOUR_YELLOW = PROFILER_COLOUR(0xFF, 0xFF, 0x00);
static constexpr ProfilerColour PROFILER_COLOUR_YELLOW_GREEN = PROFILER_COLOUR(0x9A, 0xCD, 0x32);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if PROFILER_ENABLED && defined USE_PIX

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// PIX
//
// https://devblogs.microsoft.com/pix/winpixeventruntime/
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <pix3.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ScopeProfiler {
  public:
    template <typename... ARGS>
    inline ScopeProfiler(
        ProfilerColour colour, const char *fmt, ARGS... args) {
        PIXBeginEvent(colour.value, fmt, args...);
    }

    template <typename... ARGS>
    inline ScopeProfiler(uint8_t category, const char *fmt, ARGS... args) {
        PIXBeginEvent(category, fmt, args...);
    }

    inline ~ScopeProfiler() {
        PIXEndEvent();
    }

    ScopeProfiler(const ScopeProfiler &) = delete;
    ScopeProfiler &operator=(const ScopeProfiler &) = delete;
    ScopeProfiler(ScopeProfiler &&) = delete;
    ScopeProfiler &operator=(ScopeProfiler &&) = delete;

  protected:
  private:
};

template <typename... ARGS>
static inline void ProfileMarker(
    uint8_t r, uint8_t g, uint8_t b, const char *fmt, ARGS... args) {
    PIXSetMarker(0xff000000 | r << 16 | g << 8 | b, fmt, args...);
}

template <typename... ARGS>
static inline void
ProfileMarker(uint8_t category, const char *fmt, ARGS... args) {
    PIXSetMarker(category, fmt, args...);
}

#define PROFILE_SCOPE_NAME(PREFIX, SUFFIX) \
    PROFILE_SCOPE_NAME_2(PREFIX, SUFFIX)
#define PROFILE_SCOPE_NAME_2(PREFIX, SUFFIX) PREFIX##_##SUFFIX

// (uint8_t r,uint8_t g,uint8_t b,const char *fmt,...)
// (uint8_t category,const char *fmt,...)
#define PROFILE_SCOPE(...) \
    ScopeProfiler PROFILE_SCOPE_NAME(profiler, __LINE__)(__VA_ARGS__)

// (uint8_t r,uint8_t g,uint8_t b,const char *fmt,...)
// (uint8_t category,const char *fmt,...)
#define PROFILE_MARKER(...) ProfileMarker(__VA_ARGS__)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#else

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// No profiler
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define PROFILE_SCOPE(...) ((void)0)
#define PROFILE_MARKER(...) ((void)0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
