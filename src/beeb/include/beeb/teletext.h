#ifndef HEADER_1701AC57CFE84A00BF9069818645373D
#define HEADER_1701AC57CFE84A00BF9069818645373D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

union VideoDataHalfUnit;

#include "conf.h"

#include <shared/enum_decl.h>
#include "teletext.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SAA5050 {
public:
    SAA5050();

    void Byte(uint8_t byte);

    // Produce 1 video data half unit. (One char is 2 data units wide.)
    void EmitVideoDataHalfUnit(VideoDataHalfUnit *unit);

    void HSync();

    /* odd_frame is specifically 1 (odd frame) or 0 (even frame). */
    void VSync(uint8_t odd_frame);

    bool IsDebug() const;
    void SetDebug(bool debug);

#if !BBCMICRO_FINER_TELETEXT
    bool IsAA() const;
    void SetAA(bool aa);
#endif
protected:
private:
    // Teletext
    uint8_t m_raster=0;
    uint8_t m_frame=0;

    // Data to use for the current cell - 16 pixel bitmap, and
    // foreground/background colours.
    uint8_t m_data_colours[2];
#if BBCMICRO_FINER_TELETEXT
    uint16_t m_data0=0;
    uint16_t m_data1=0;
#else
    uint16_t m_data=0;
#endif

    // Current character set data.
    uint8_t m_charset=0;
    uint8_t m_graphics_charset=0;

    // Current teletext colours.
    uint8_t m_fg=0;
    uint8_t m_bg=0;

#if BBCMICRO_FINER_TELETEXT
    uint16_t m_last_graphics_data0=0;
    uint16_t m_last_graphics_data1=0;
#else
    uint16_t m_last_graphics_data=0;
#endif

    // Double height management.
    uint8_t m_raster_shift=0;
    uint8_t m_raster_offset=0;

    bool m_any_double_height=false;
    bool m_conceal=false;
    bool m_hold=false;
    bool m_flash=false;
    bool m_frame_flash=false;
    bool m_debug=false;
#if !BBCMICRO_FINER_TELETEXT
    bool m_aa=false;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
