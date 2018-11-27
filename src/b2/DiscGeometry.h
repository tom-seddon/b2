#ifndef HEADER_CB62E80F404B4FFF88B0911D02D06041// -*- mode:c++ -*-
#define HEADER_CB62E80F404B4FFF88B0911D02D06041

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
};

bool operator==(const DiscGeometry &a,const DiscGeometry &b);
bool operator!=(const DiscGeometry &a,const DiscGeometry &b);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
