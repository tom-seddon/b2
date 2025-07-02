#include <shared/system.h>
#include "discs.h"
#include "load_save.h"
#include <beeb/DiscGeometry.h>
#include <random>
#include <shared/load_store.h>
#include <shared/debug.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string Disc::GetAssetPath() const {
    return ::GetAssetPath("discs", this->path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string HardDisk::GetDATAssetPath() const {
    return ::GetAssetPath("discs", this->path_stem + ".dat");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string HardDisk::GetDSCAssetPath() const {
    return ::GetAssetPath("discs", this->path_stem + ".dsc");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const Disc BLANK_DFS_DISCS[] = {
    {"DFS 80T SSD", "80.ssd", &SSD_GEOMETRY},
    {"DFS 80T DSD", "80.dsd", &DSD_GEOMETRY},

    // Support for 40T disks just isn't very good... the files won't round
    // trip properly.

    //    {"DFS 40T SSD","40.ssd",&SSD_GEOMETRY},
    //    {"DFS 40T DSD","40.dsd",&DSD_GEOMETRY},
};

extern const size_t NUM_BLANK_DFS_DISCS = sizeof BLANK_DFS_DISCS / sizeof BLANK_DFS_DISCS[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const Disc BLANK_ADFS_DISCS[] = {
    {"ADFS L", "adl.adl", &ADL_GEOMETRY},
    {"ADFS M", "adm.adm", &ADM_GEOMETRY},
    {"ADFS S", "ads.ads", &ADS_GEOMETRY},
};

const size_t NUM_BLANK_ADFS_DISCS = sizeof BLANK_ADFS_DISCS / sizeof BLANK_ADFS_DISCS[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const HardDisk BLANK_HARD_DISKS[] = {
    {"10 MByte ADFS", "10MB"},
};

extern const size_t NUM_BLANK_HARD_DISKS = sizeof BLANK_HARD_DISKS / sizeof BLANK_HARD_DISKS[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t GetADFSChecksum(const uint8_t *sector) {
    uint32_t sum = 255;

    for (size_t i = 0; i < 255; ++i) {
        if (sum > 255) {
            sum = (sum + 1) & 0xff;
        }

        sum += sector[i];
    }

    return (uint8_t)sum;
}

void RandomizeADFSDiskIdentifier(std::vector<uint8_t> *data) {
    if (data->size() < 0x200) {
        // Too small to be ADFS.
        return;
    }

    uint8_t checksum0 = GetADFSChecksum(&(*data)[0]);
    if (checksum0 != (*data)[255]) {
        // Bad sector 0 checksum.
        return;
    }

    uint8_t checksum1 = GetADFSChecksum(&(*data)[256]);
    if (checksum1 != (*data)[511]) {
        // Bad sector 1 checksum.
        return;
    }

    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 65536);
    int value = dist(rd);

    Store16LE(&(*data)[0x1fb], (uint16_t)value);
    (*data)[511] = GetADFSChecksum(&(*data)[256]);

    ASSERT(GetADFSChecksum(&(*data)[0]) == (*data)[255]);
    ASSERT(GetADFSChecksum(&(*data)[256]) == (*data)[511]);
}
