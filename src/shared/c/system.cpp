#include <shared/system.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern void SetCurrentThreadNameInternal(const char *name);

static SetCurrentThreadNameFn g_set_current_thread_name_fn;
static void *g_set_current_thread_name_context;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetSetCurrentThreadNameCallback(SetCurrentThreadNameFn fn, void *context) {
    g_set_current_thread_name_fn = fn;
    g_set_current_thread_name_context = context;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetCurrentThreadName(const char *name) {
    SetCurrentThreadNameInternal(name);

    if (g_set_current_thread_name_fn) {
        (*g_set_current_thread_name_fn)(name, g_set_current_thread_name_context);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetCurrentThreadNamef(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    SetCurrentThreadNamev(fmt, v);
    va_end(v);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetCurrentThreadNamev(const char *fmt, va_list v) {
    char *name;
    if (vasprintf(&name, fmt, v) == -1) {
        // Not much you can do, if this happens...
        return;
    }

    SetCurrentThreadName(name);

    free(name);
    name = NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !SYSTEM_HAVE_STRLCPY

// http://www.opensource.apple.com/source/mail_cmds/mail_cmds-24/mail/strlcpy.c
size_t strlcpy(char *dest, const char *src, size_t size) {
    char *d = dest;
    const char *s = src;
    size_t n = size;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (size != 0)
            *d = '\0'; /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return (size_t)(s - src - 1); /* count does not include NUL */
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This goes here because... it has to go somewhere.

#define BIT(X, N) (((X)&1 << N) ? '1' : '0')
#define BITS(X) {BIT(X, 7), BIT(X, 6), BIT(X, 5), BIT(X, 4), BIT(X, 3), BIT(X, 2), BIT(X, 1), BIT(X, 0), 0},
#define AABBCCDD(A, Y, Z, W) BITS(((A) << 6 | (Y) << 4 | (Z) << 2 | (W)))
#define AABBCC__(A, B, C) \
    AABBCCDD(A, B, C, 0)  \
    AABBCCDD(A, B, C, 1)  \
    AABBCCDD(A, B, C, 2)  \
    AABBCCDD(A, B, C, 3)
#define AABB____(A, B) \
    AABBCC__(A, B, 0)  \
    AABBCC__(A, B, 1)  \
    AABBCC__(A, B, 2)  \
    AABBCC__(A, B, 3)
#define AA______(A) \
    AABB____(A, 0)  \
    AABB____(A, 1)  \
    AABB____(A, 2)  \
    AABB____(A, 3)

const char BINARY_BYTE_STRINGS[256][9] = {AA______(0) AA______(1) AA______(2) AA______(3)};

#undef AA______
#undef AABB____
#undef AABBCC__
#undef AABBCCDD
#undef BITS
#undef BIT

const char ASCII_BYTE_STRINGS[256][5] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //0
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //16
    "' '", "'!'", "'\"'", "'#'", "'$'", "'%'", "'&'", "'\\''", "'('", "')'", "'*'", "'+'", "','", "'-'", "'.'", "'/'", //32
    "'0'", "'1'", "'2'", "'3'", "'4'", "'5'", "'6'", "'7'", "'8'", "'9'", "':'", "';'", "'<'", "'='", "'>'", "'?'",    //48
    "'@'", "'A'", "'B'", "'C'", "'D'", "'E'", "'F'", "'G'", "'H'", "'I'", "'J'", "'K'", "'L'", "'M'", "'N'", "'O'",    //64
    "'P'", "'Q'", "'R'", "'S'", "'T'", "'U'", "'V'", "'W'", "'X'", "'Y'", "'Z'", "'['", "'\\'", "']'", "'^'", "'_'",   //80,
    "'`'", "'a'", "'b'", "'c'", "'d'", "'e'", "'f'", "'g'", "'h'", "'i'", "'j'", "'k'", "'l'", "'m'", "'n'", "'o'",    //96,
    "'p'", "'q'", "'r'", "'s'", "'t'", "'u'", "'v'", "'w'", "'x'", "'y'", "'z'", "'{'", "'|'", "'}'", "'~'", "",       //112
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //128
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //144
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //160
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //176
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //192
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //208
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //224
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                                                    //240
};

const char HEX_CHARS_UC[] = "0123456789ABCDEF";
const char HEX_CHARS_LC[] = "0123456789abcdef";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
