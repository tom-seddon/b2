#ifndef HEADER_CB62E80F404B4FFF88B0911D02D06041// -*- mode:c++ -*-
#define HEADER_CB62E80F404B4FFF88B0911D02D06041

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <vector>
#include <string>

class Messages;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Both sides have the same geometry - the BBC disc image formats
// aren't very clever.

struct DiscGeometry {
    bool double_sided=false;
    bool double_density=false;
    size_t num_tracks=0;
    size_t sectors_per_track=0;
    size_t bytes_per_sector=0;

    DiscGeometry();
    DiscGeometry(size_t num_tracks,
                 size_t sectors_per_track,
                 size_t bytes_per_sector,
                 bool double_sided=false,
                 bool double_density=false);

    size_t GetTotalNumBytes() const;

    // Returns true if the address is valid, and *index is then the
    // index to use - though it may be past the end of the data if
    // the disc image data is short.
    bool GetIndex(size_t *index,
                  uint8_t side,
                  uint8_t track,
                  uint8_t sector,
                  size_t offset) const;
};

bool operator==(const DiscGeometry &a,const DiscGeometry &b);
bool operator!=(const DiscGeometry &a,const DiscGeometry &b);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const std::vector<std::string> DISC_IMAGE_EXTENSIONS;

bool FindDiscGeometryFromFileDetails(DiscGeometry *geometry,
                                     const char *file_name,
                                     size_t file_size,
                                     Messages *msg);

bool FindDiscGeometryFromMIMEType(DiscGeometry *geometry,
                                  const char *mime_type,
                                  size_t file_size,
                                  Messages *msg);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *GetExtensionFromDiscGeometry(const DiscGeometry &geometry);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
