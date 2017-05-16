#ifndef HEADER_D20E0E20832349CAA17E68B81F546B14
#define HEADER_D20E0E20832349CAA17E68B81F546B14

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include <6502/6502.h>

class Trace;

#include <shared/enum_decl.h>
#include "crtc.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CRTC {
public:
    struct Output {
        // Value of hsync output. If !display, display is off, for
        // whatever resaon.
        uint32_t hsync:1;

        // Value of vsync output.
        uint32_t vsync:1;

        // If !hsync && !vsync && display, fetch from ADDRESS/ROW and use
        // as display data.
        uint32_t display:1;

        // Value of cudisp output.
        uint32_t cudisp:1;

        // Whether this is an odd or even frame.
        uint32_t odd_frame:1;

        // 6845 address to fetch from, if FETCH set.
        uint32_t address:14;

        // Current character row, if FETCH set.
        uint32_t raster:5;
    };

    static uint8_t ReadAddress(void *c_,M6502Word a);
    static void WriteAddress(void *c_,M6502Word a,uint8_t value);
    static uint8_t ReadData(void *c_,M6502Word a);
    static void WriteData(void *c_,M6502Word a,uint8_t value);

    Output Update(uint8_t fast_6845);

#if BBCMICRO_TRACE
    void SetTrace(Trace *t,bool trace_scanlines);
#endif
protected:
private:
#include <shared/pushwarn_bitfields.h>
    struct R3Bits {
        uint8_t wh:4;
        uint8_t wv:4;
    };
#include <shared/popwarn.h>

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    union R3 {
        struct R3Bits bits;
        uint8_t value;
    };

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

#include <shared/pushwarn_bitfields.h>
    struct R8Bits {
        uint8_t s:1;
        uint8_t v:1;
        uint8_t _:2;
        uint8_t d:2;
        uint8_t c:2;
    };
#include <shared/popwarn.h>

    union R8 {
        struct R8Bits bits;
        uint8_t value;
    };

#include <shared/pushwarn_bitfields.h>
    struct R10Bits {
        uint8_t start:5;
        uint8_t mode:2;//(CRTCCursorMode)
    };
#include <shared/popwarn.h>

    union R10 {
        struct R10Bits bits;
        uint8_t value;
    };
    typedef union CRTCR10 CRTCR10;

    struct RegisterBits {
        uint8_t nht;//R0 - horizontal total
        uint8_t nhd;//R1 - horizontal displayed
        uint8_t nhsp;//R2 - horizontal sync position
        R3 nsw;//R3 - sync width
        uint8_t nvt;//R4 - vertical total
        uint8_t nadj;//R5 - vertical total adjust
        uint8_t nvd;//R6 - vertical displayed
        uint8_t nvsp;//R7 - vertical sync position
        R8 r8;//R8 - interlace and skew
        uint8_t nr;//R9 - maximum raster address
        R10 ncstart;//R10 - cursor start raster
        uint8_t ncend;//R11 - cursor end raster
        uint8_t addrh,addrl;//R12,R13 - start address
        uint8_t cursorh,cursorl;//R14,R15 - cursor address
        uint8_t r16;
        uint8_t r17;
    };

    union Registers {
        RegisterBits bits;
        uint8_t values[18];
    };

    Registers m_registers={};
    uint8_t m_address=0;

    // incremented on each field - used for even/odd and cursor blink
    // timing, so wraparound is no problem
    uint8_t m_num_frames=0;
    uint8_t m_interlace_delay=0;
    uint8_t m_column=0;//character column
    uint8_t m_row=0;//character row
    uint8_t m_raster=0;//scanline in character
    uint8_t m_vsync_left=0;//vsync counter
    uint8_t m_hsync_left=0;//hsync counter
    bool m_adj=false;//set if in the adjustment period
    M6502Word m_line_addr={};
    M6502Word m_char_addr={};
    uint8_t m_delay=0;// display enable delay for current scanline

#if BBCMICRO_TRACE
    Trace *m_trace=nullptr;
    uint32_t m_trace_scanline=0;
    bool m_trace_scanlines=false;
#endif

    void NextRaster();
    void StartOfFrame();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
