#ifndef __INC_CORE_H
#define __INC_CORE_H

static const float VOLUMES_TABLE[] = {
    0.00000f,
    0.03981f,
    0.05012f,
    0.06310f,
    0.07943f,
    0.10000f,
    0.12589f,
    0.15849f,
    0.19953f,
    0.25119f,
    0.31623f,
    0.39811f,
    0.50119f,
    0.63096f,
    0.79433f,
    1.00000f,
};


#define B2_SAMPLE_RATE 250000
#define B2_SAMPLE_RATE_FLOAT 250000.0
#define B2_SNAPSHOT_SIZE 262144

#define B2_LIBRETRO_SCREEN_WIDTH 768
#define B2_LIBRETRO_SCREEN_HEIGHT 576

#define B2_MAX_USERS 2
#define B2_MESSAGE_DISPLAY_FRAMES 6*50

#if __GNUC__
#define printflike __attribute__((format (printf, 1, 2)))
#else
#define printflike
#endif
#include <stdbool.h>

extern void log_fatal(const char *fmt, ...) printflike;
extern void log_error(const char *fmt, ...) printflike;
extern void log_warn(const char *fmt, ...) printflike;
extern void log_info(const char *fmt, ...) printflike;
//extern void log_info_OUTPUT(const char *fmt, ...) printflike;

// If the debugging compilation option is enabled a real function will
// be available to log debug messages.  If the debugging compilation
// optionis disabled we use a static inline empty function to make the
// debug calls disappear but in a way that does not generate warnings
// about unused variables etc.

extern void log_debug(const char *fmt, ...) printflike;
extern void log_dump(const char *prefix, uint8_t *data, size_t size);
extern void log_bitfield(const char *fmt, unsigned value, const char **names);

extern int autoboot;
extern bool sound_ddnoise, sound_tape;

#endif