#ifndef HEADER_5658C00A506F464FA4F6A3A3803849A4 // -*- mode:c++ -*-
#define HEADER_5658C00A506F464FA4F6A3A3803849A4

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#include <shared/enum_decl.h>
#include "TVOutput.inl"
#include <shared/enum_end.h>

#include <memory>
#include <vector>
#include <beeb/OutputData.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct VideoDataUnit;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TVOutput is the analogue of a combination of the video encoding and the TV -
// it looks after converting a stream of video data chunks into a graphical
// display. Output format is DXGI_FORMAT_B8G8R8A8_UNORM, aka
// SDL_PIXELFORMAT_ARGB8888. The texture is always
// TV_TEXTURE_WIDTH*TV_TEXTURE_HEIGHT, and its stride is TV_OUTPUT_WIDTH*4.

// TODO: should be a CreateSDLTexture function, that creates an appropriate
// SDL_Texture. Just supply the texture access flags.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TVOutput {
  public:
    bool show_usec_markers = false;
    bool show_half_usec_markers = false;
    bool show_6845_row_markers = false;
    bool show_6845_dispen_markers = false;
    bool show_beam_position = false;

    TVOutput();
    ~TVOutput();

    // returns number of us consumed.
    void Update(const VideoDataUnit *units, size_t num_units);

#if BBCMICRO_DEBUGGER
    void FillWithTestPattern();
#endif

    // *data_version (optional) is set to texture data version, incremented on
    // each vblank. (Between vblanks, the buffer contains a partially scanned-out frame.)
    const uint32_t *GetTexturePixels(uint64_t *texture_data_version) const;

    void CopyTexturePixels(void *dest_pixels, size_t dest_pitch) const;

#if VIDEO_TRACK_METADATA
    const VideoDataUnit *GetTextureUnits() const;
#endif

    // returns false, *X and *Y untouched, if beam is outside the
    // visible area.
    bool GetBeamPosition(size_t *x, size_t *y) const;

    bool IsInVerticalBlank() const;

    // TODO - nothing actually uses this! There should probably be a slider
    // somewhere, or something...
    double GetGamma() const;
    void SetGamma(double gamma);

    bool GetInterlace() const;
    void SetInterlace(bool interlace);

  protected:
  private:
    TVOutputState m_state = TVOutputState_VerticalRetrace;
    uint32_t *m_pixels_line = nullptr;
#if VIDEO_TRACK_METADATA
    VideoDataUnit *m_units_line = nullptr;
#endif
    size_t m_x = 0;
    size_t m_y = 0;
    int m_state_timer = 0;
    size_t m_num_fields = 0;
    bool m_interlace = false; //it's horrid. It's there, but you don't want it

    // TV - output texture and its properties
    std::vector<uint32_t> m_texture_pixels;
#if VIDEO_TRACK_METADATA
    std::vector<VideoDataUnit> m_texture_units;
#endif
    uint64_t m_texture_data_version = 1;

#if TRACK_VIDEO_LATENCY
    uint64_t render_latency_ticks;
    size_t num_renders;
#endif

    double m_gamma = 2.2;

    // m_blend[i][j] is gamma-corrected 8-bit blend of 1/3 i<<4|i and
    // 2/3 j<<4|j.
    uint8_t m_blend[16][16] = {};

    uint32_t m_usec_marker_xor = 0;
    uint32_t m_half_usec_marker_xor = 0;
    uint32_t m_6845_raster0_marker_xor = 0;
    uint32_t m_6845_dispen_marker_xor = 0;
    uint32_t m_beam_marker_xor = 0;

    uint32_t GetTexelValue(uint8_t r, uint8_t g, uint8_t b) const;
    void InitPalette();
#if VIDEO_TRACK_METADATA
    void AddMetadataMarkers(void *dest_pixels, size_t dest_pitch_bytes, bool add, uint8_t metadata_flag, uint32_t xor_value) const;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
