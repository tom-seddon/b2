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
        // Value of hsync output.
        uint32_t hsync : 1;

        // Value of vsync output.
        uint32_t vsync : 1;

        // If display, fetch from ADDRESS/RASTER and use as display data.
        uint32_t display : 1;

        // Value of cudisp output.
        uint32_t cudisp : 1;

        // 6845 address to fetch from, if DISPLAY set.
        uint32_t address : 14;

        // Current character row, if DISPLAY set.
        uint32_t raster : 5;
    };

    static uint8_t ReadAddress(void *c_, M6502Word a);
    static void WriteAddress(void *c_, M6502Word a, uint8_t value);
    static uint8_t ReadData(void *c_, M6502Word a);
    static void WriteData(void *c_, M6502Word a, uint8_t value);

    Output Update(uint8_t lightpen);

#if BBCMICRO_TRACE
    void SetTrace(Trace *t,
                  bool trace_scanlines,
                  bool trace_scanlines_separators);
#endif
  protected:
  private:
#include <shared/pushwarn_bitfields.h>
    struct R3Bits {
        uint8_t wh : 4;
        uint8_t wv : 4;
    };
#include <shared/popwarn.h>

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    union R3 {
        uint8_t value;
        struct R3Bits bits;
    };

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

#include <shared/pushwarn_bitfields.h>
    struct R8Bits {
        uint8_t s : 1;
        uint8_t v : 1;
        uint8_t _ : 2;
        uint8_t d : 2;
        uint8_t c : 2;
    };
#include <shared/popwarn.h>

    union R8 {
        uint8_t value;
        struct R8Bits bits;
    };

#include <shared/pushwarn_bitfields.h>
    struct R10Bits {
        uint8_t start : 5;
        uint8_t mode : 2; //(CRTCCursorMode)
    };
#include <shared/popwarn.h>

    union R10 {
        uint8_t value;
        struct R10Bits bits;
    };
    typedef union CRTCR10 CRTCR10;

    struct RegisterBits {
        uint8_t nht;              //R0 - horizontal total
        uint8_t nhd;              //R1 - horizontal displayed
        uint8_t nhsp;             //R2 - horizontal sync position
        R3 nsw;                   //R3 - sync width
        uint8_t nvt;              //R4 - vertical total
        uint8_t nadj;             //R5 - vertical total adjust
        uint8_t nvd;              //R6 - vertical displayed
        uint8_t nvsp;             //R7 - vertical sync position
        R8 r8;                    //R8 - interlace and skew
        uint8_t nr;               //R9 - maximum raster address
        R10 ncstart;              //R10 - cursor start raster
        uint8_t ncend;            //R11 - cursor end raster
        uint8_t addrh, addrl;     //R12,R13 - start address
        uint8_t cursorh, cursorl; //R14,R15 - cursor address
        uint8_t penh, penl;       //R16,R17 - light pen address
    };

    union Registers {
        uint8_t values[18];
        RegisterBits bits;
    };

    struct InternalState {
        uint8_t column = 0;        //character column
        uint8_t row = 0;           //character row
        uint8_t raster = 0;        //scanline in character
        int8_t vsync_counter = -1; //vsync counter
        int8_t hsync_counter = -1; //hsync counter
        int8_t vadj_counter = -1;
        //    //bool adj=false;//set if in the adjustment period
        bool hdisp = true;
        bool vdisp = true;
        M6502Word line_addr = {};
        M6502Word next_line_addr = {};
        M6502Word char_addr = {};
        uint32_t num_updates = 0;
        uint8_t skewed_display = 0;
        uint8_t skewed_cudisp = 0;
        bool check_vadj = false;
        bool in_vadj = false;
        bool end_of_vadj_latched = false;
        bool had_vsync_this_row = false;
        bool end_of_main_latched = false;
        bool do_even_frame_logic = false;
        bool first_scanline = false;
        bool in_dummy_raster = false;
        bool end_of_frame_latched = false;
        bool cursor = false;
        bool old_lightpen = false;
    };

    Registers m_registers = {};
    uint8_t m_address = 0;
    InternalState m_st;

    // incremented on each field - used for even/odd and cursor blink
    // timing, so wraparound is no problem
    uint8_t m_num_frames = 0;

    //    int m_interlace_delay_counter=-1;

#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
    bool m_trace_scanlines = false;
    bool m_trace_scanlines_separators = false;
#endif

    void EndOfFrame();
    void EndOfRow();
    void EndOfScanline();

#if BBCMICRO_DEBUGGER
    friend class CRTCDebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
