#include <shared/system.h>
#include <beeb/video.h>
#include <shared/debug.h>

#include <shared/enum_def.h>
#include <beeb/video.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool IsTeletextData(const VideoDataUnit &unit) {
    switch ((VideoDataType)unit.pixels.pixels[0].bits.x) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case VideoDataType_Bitmap12MHz:
    case VideoDataType_Bitmap16MHz:
        return false;

    case VideoDataType_Teletext:
    case VideoDataType_TeletextUnscaled:
        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
