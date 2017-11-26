#ifndef HEADER_1701AC57CFE84A00BF9069818645373D
#define HEADER_1701AC57CFE84A00BF9069818645373D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

union VideoDataUnit;

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

    // One char is 2 units wide.
    void EmitVideoDataUnit(VideoDataUnit *unit);

    void HSync();

    /* odd_frame is specifically 1 (odd frame) or 0 (even frame). */
    void VSync(uint8_t odd_frame);

    bool IsDebug() const;
    void SetDebug(bool debug);
protected:
private:
    // Teletext
    uint8_t m_raster=0;
    uint8_t m_frame=0;

    // Data to use for the current cell - 16 pixel bitmap, and
    // foreground/background colours.
    uint8_t m_data_colours[2];
    uint16_t m_data0=0;
    uint16_t m_data1=0;

    // Current character set data.
    uint8_t m_charset=0;
    uint8_t m_graphics_charset=0;

    // Current teletext colours.
    uint8_t m_fg=0;
    uint8_t m_bg=0;

    uint16_t m_last_graphics_data0=0;
    uint16_t m_last_graphics_data1=0;

    // Double height management.
    uint8_t m_raster_shift=0;
    uint8_t m_raster_offset=0;

    bool m_any_double_height=false;
    bool m_conceal=false;
    bool m_hold=false;
    bool m_flash=false;
    bool m_frame_flash=false;
    bool m_debug=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
