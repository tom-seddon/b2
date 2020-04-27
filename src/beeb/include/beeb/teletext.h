#ifndef HEADER_1701AC57CFE84A00BF9069818645373D
#define HEADER_1701AC57CFE84A00BF9069818645373D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

union VideoDataUnitPixels;

#include "conf.h"

#include "video.h"

#include <shared/enum_decl.h>
#include "teletext.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SAA5050 {
public:
    SAA5050();

    void Byte(uint8_t byte,uint8_t dispen);

    // One char is 2 units wide.
    void EmitPixels(VideoDataUnitPixels *pixels);

    void StartOfLine();
    void EndOfLine();

    void VSync();

#if BBCMICRO_DEBUGGER
    bool IsDebug() const;
    void SetDebug(bool debug);
#endif
protected:
private:
    struct Output {
        uint8_t fg,bg,data0,data1;
    };

    // Teletext
    uint8_t m_raster=0;
    uint8_t m_frame=0;

    // Output buffers.
    Output m_output[8]={};
    uint8_t m_write_index=4;
    uint8_t m_read_index=0;

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
    bool m_text_visible=false;
    bool m_frame_flash_visible=false;
#if BBCMICRO_DEBUGGER
    bool m_debug=false;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
