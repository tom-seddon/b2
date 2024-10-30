#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/DiscGeometry.h>
#include <shared/path.h>
#include <string.h>
#include <inttypes.h>
#include <shared/log.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const DiscGeometry SSD_GEOMETRY(80, 10, 256);
const DiscGeometry DSD_GEOMETRY(80, 10, 256, true);
// static const DiscGeometry SDD_GEOMETRY(1,0,0,256);
// static const DiscGeometry DDD_GEOMETRY(2,0,0,256);
const DiscGeometry ADM_GEOMETRY(80, 16, 256, false, true);
const DiscGeometry ADL_GEOMETRY(80, 16, 256, true, true);
const DiscGeometry ADS_GEOMETRY(40, 16, 256, false, true);

static const size_t ADS_SIZE = ADS_GEOMETRY.GetTotalNumBytes();
static const size_t ADM_SIZE = ADM_GEOMETRY.GetTotalNumBytes();
static const size_t ADL_SIZE = ADL_GEOMETRY.GetTotalNumBytes();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const DiscGeometry SDD_GEOMETRIES[] = {
    DiscGeometry(80, 16, 256, false, true),
    DiscGeometry(40, 16, 256, false, true),
    DiscGeometry(80, 18, 256, false, true),
    DiscGeometry(40, 16, 256, false, true),
};
static const size_t NUM_SDD_GEOMETRIES = sizeof SDD_GEOMETRIES / sizeof SDD_GEOMETRIES[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const DiscGeometry DDD_GEOMETRIES[] = {
    DiscGeometry(80, 16, 256, true, true),
    DiscGeometry(40, 16, 256, true, true),
    DiscGeometry(80, 18, 256, true, true),
    DiscGeometry(40, 16, 256, true, true),
};
static const size_t NUM_DDD_GEOMETRIES = sizeof DDD_GEOMETRIES / sizeof DDD_GEOMETRIES[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This mechanism is now officially a bit of a disaster area, what
// with the separate calls for detecting disc type by extension or
// MIME type (that aren't even really all that convenient to use). It
// wants pulling out into its own module.

struct DiscImageType;

typedef bool (*FindDiscGeometryFn)(DiscGeometry *geometry, const char *name, uint64_t size, const LogSet *logs, const DiscImageType *disc_image_type);

struct DiscImageType {
    const char *ext;
    const char *mime_type;
    FindDiscGeometryFn find_geometry_fn;
    const DiscGeometry *possible_geometries;
    size_t num_possible_geometries;
};

// If adding more extensions, update the Info.plist for macOS too.
#define SSD_EXT ".ssd"
#define DSD_EXT ".dsd"
#define SDD_EXT ".sdd"
#define DDD_EXT ".ddd"
#define ADM_EXT ".adm"
#define ADL_EXT ".adl"
#define ADS_EXT ".ads"
#define ADF_EXT ".adf"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void PrintInvalidSizeMessage(const char *name, const LogSet &logs) {
    logs.e.f("invalid size");
    if (name) {
        logs.e.f(" for file: %s", name);
    }
    logs.e.f("\n");
}

static bool IsMultipleOfSectorSize(const char *name, size_t size, const LogSet *logs) {
    if (size % 256 != 0) {
        if (logs) {
            PrintInvalidSizeMessage(name, *logs);

            logs->i.f("(length %zu not a multiple of sector size 256)\n",
                      size);
        }
        return false;
    }

    return true;
}

static bool FindSingleDensityDiscGeometry(DiscGeometry *geometry, const char *name, uint64_t size, const LogSet *logs, const DiscImageType *disc_image_type) {
    ASSERT(disc_image_type->num_possible_geometries == 1);
    ASSERT(!disc_image_type->possible_geometries[0].double_density);
    *geometry = DiscGeometry(80, 10, 256, disc_image_type->possible_geometries[0].double_sided);

    if (!IsMultipleOfSectorSize(name, size, logs)) {
        return false;
    }

    if (size > geometry->GetTotalNumBytes()) {
        if (logs) {
            PrintInvalidSizeMessage(name, *logs);

            logs->i.f("(size %" PRIu64 " bytes is larger than maximum %zu bytes for %d*%zu*%zu sectors)\n",
                      size,
                      geometry->GetTotalNumBytes(),
                      geometry->double_sided ? 2 : 1,
                      geometry->num_tracks,
                      geometry->sectors_per_track);
        }
        return false;
    }

    return true;
}

static bool FindDiscGeometryFromFileSize(DiscGeometry *geometry, const char *name, uint64_t size, const LogSet *logs, const DiscImageType *disc_image_type) {
    for (size_t i = 0; i < disc_image_type->num_possible_geometries; ++i) {
        const DiscGeometry *g = &disc_image_type->possible_geometries[i];

        if (size == g->GetTotalNumBytes()) {
            *geometry = *g;
            return true;
        }
    }

    if (logs) {
        PrintInvalidSizeMessage(name, *logs);

        logs->i.f("(size is %" PRIu64 "; valid sizes are: ", size);
        for (size_t i = 0; i < disc_image_type->num_possible_geometries; ++i) {
            if (i > 0) {
                logs->i.f("; ");
            }

            logs->i.f("%zu", disc_image_type->possible_geometries[i].GetTotalNumBytes());
        }
        logs->i.f(")\n");
    }

    return false;
}

static bool FindADFSDiscGeometry(DiscGeometry *geometry, const char *name, uint64_t size, const LogSet *logs, const DiscImageType *disc_image_type) {
    (void)disc_image_type;

    if (!IsMultipleOfSectorSize(name, size, logs)) {
        return false;
    }

    if (size > ADL_SIZE) {
        if (logs) {
            PrintInvalidSizeMessage(name, *logs);

            logs->i.f("(size is %" PRIu64 "; max valid size is %zu", size, ADL_SIZE);
        }

        return false;
    } else if (size > ADM_SIZE && size <= ADL_SIZE) {
        *geometry = ADL_GEOMETRY;
    } else if (size > ADS_SIZE && size <= ADM_SIZE) {
        *geometry = ADM_GEOMETRY;
    } else {
        *geometry = ADS_GEOMETRY;
    }

    return true;
}

static const DiscImageType DISC_IMAGE_TYPES[] = {
    {SSD_EXT, "application/x.acorn.disc-image.ssd", &FindSingleDensityDiscGeometry, &SSD_GEOMETRY, 1},
    {DSD_EXT, "application/x.acorn.disc-image.dsd", &FindSingleDensityDiscGeometry, &DSD_GEOMETRY, 1},
    {SDD_EXT, "application/x.acorn.disc-image.sdd", &FindDiscGeometryFromFileSize, SDD_GEOMETRIES, NUM_SDD_GEOMETRIES},
    {DDD_EXT, "application/x.acorn.disc-image.ddd", &FindDiscGeometryFromFileSize, DDD_GEOMETRIES, NUM_DDD_GEOMETRIES},
    {ADM_EXT, "application/x.acorn.disc-image.adm", &FindDiscGeometryFromFileSize, &ADM_GEOMETRY, 1},
    {ADL_EXT, "application/x.acorn.disc-image.adl", &FindDiscGeometryFromFileSize, &ADL_GEOMETRY, 1},
    {ADS_EXT, "application/x.acorn.disc-image.ads", &FindDiscGeometryFromFileSize, &ADS_GEOMETRY, 1},
    {ADF_EXT, "application/x.acorn.disc-image.adf", &FindADFSDiscGeometry, nullptr, 0},
    {},
};

const std::vector<std::string> DISC_IMAGE_EXTENSIONS = {
    SSD_EXT,
    DSD_EXT,
    SDD_EXT,
    DDD_EXT,
    ADM_EXT,
    ADL_EXT,
    ADS_EXT,
    ADF_EXT,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscGeometry::DiscGeometry() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscGeometry::DiscGeometry(size_t num_tracks_,
                           size_t sectors_per_track_,
                           size_t bytes_per_sector_,
                           bool double_sided_,
                           bool double_density_)
    : double_sided(double_sided_)
    , double_density(double_density_)
    , num_tracks(num_tracks_)
    , sectors_per_track(sectors_per_track_)
    , bytes_per_sector(bytes_per_sector_) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t DiscGeometry::GetTotalNumBytes() const {
    size_t n = this->num_tracks * this->sectors_per_track * this->bytes_per_sector;

    if (this->double_sided) {
        n *= 2;
    }

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DiscGeometry::GetIndex(size_t *index,
                            uint8_t side,
                            uint8_t track,
                            uint8_t sector,
                            size_t offset) const {
    if (side >= (this->double_sided ? 2 : 1)) {
        return false;
    }

    if (track >= this->num_tracks) {
        return false;
    }

    if (sector >= this->sectors_per_track) {
        return false;
    }

    if (offset >= this->bytes_per_sector) {
        return false;
    }

    *index = 0;
    *index += track; // in tracks
    if (this->double_sided) {
        *index *= 2;
        *index += side; // adjusted for track interleaving
    }
    *index *= this->sectors_per_track; // in sectors
    *index += sector;
    *index *= this->bytes_per_sector; // in bytes
    *index += offset;                 // +offset

    ASSERT(*index < this->GetTotalNumBytes());

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool operator==(const DiscGeometry &a, const DiscGeometry &b) {
    if (a.double_sided != b.double_sided) {
        return false;
    }

    if (a.double_density != b.double_density) {
        return false;
    }

    if (a.num_tracks != b.num_tracks) {
        return false;
    }

    if (a.sectors_per_track != b.sectors_per_track) {
        return false;
    }

    if (a.bytes_per_sector != b.bytes_per_sector) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool operator!=(const DiscGeometry &a, const DiscGeometry &b) {
    return !(a == b);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool FindDiscGeometryFromFileDetails(DiscGeometry *geometry,
                                     const char *file_name,
                                     uint64_t file_size,
                                     const LogSet *logs) {
    std::string ext = PathGetExtension(file_name);

    for (const DiscImageType *type = DISC_IMAGE_TYPES; type->ext; ++type) {
        if (PathCompare(ext, type->ext) == 0) {
            return (*type->find_geometry_fn)(geometry, file_name, file_size, logs, type);
        }
    }

    if (logs) {
        logs->e.f("unknown extension: %s\n", ext.c_str());
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool FindDiscGeometryFromMIMEType(DiscGeometry *geometry,
                                  const char *mime_type,
                                  uint64_t file_size,
                                  const LogSet &logs) {
    for (const DiscImageType *type = DISC_IMAGE_TYPES; type->ext; ++type) {
        if (strcmp(mime_type, type->mime_type) == 0) {
            return (*type->find_geometry_fn)(geometry, nullptr, file_size, &logs, type);
        }
    }

    logs.e.f("unknown MIME type: %s\n", mime_type);

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsGeometryInList(const DiscGeometry &geometry, const DiscGeometry *geometries, size_t num_geometries) {
    for (size_t i = 0; i < num_geometries; ++i) {
        if (geometries[i] == geometry) {
            return true;
        }
    }

    return false;
}

const char *GetExtensionFromDiscGeometry(const DiscGeometry &geometry) {
    if (geometry.double_density) {
        if (geometry == ADL_GEOMETRY) {
            return ADL_EXT;
        } else if (geometry == ADM_GEOMETRY) {
            return ADM_EXT;
        } else if (geometry.double_sided) {
            if (IsGeometryInList(geometry, DDD_GEOMETRIES, NUM_DDD_GEOMETRIES)) {
                return DDD_EXT;
            }
        } else {
            if (IsGeometryInList(geometry, SDD_GEOMETRIES, NUM_SDD_GEOMETRIES)) {
                return SDD_EXT;
            }
        }
    } else {
        if (geometry.double_sided) {
            return DSD_EXT;
        } else {
            return SSD_EXT;
        }
    }

    return nullptr;
}
