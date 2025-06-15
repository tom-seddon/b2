#include <shared/system.h>
#include <beeb/HardDiskImage.h>
#include <shared/path.h>
#include <shared/file_io.h>
#include <shared/log.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// See also http://www.cowsarenotpurple.co.uk/bbccomputer/native/adfs.html#imsandems

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// BBC hard disks always have 33 sectors.
static const uint8_t NUM_HARD_DISK_SECTORS = 33;

static const uint32_t MAX_BLOCK = LONG_MAX / 256;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HardDiskImage::HardDiskImage(std::string dat_path_, std::vector<uint8_t> dsc_data_, FILE *fp)
    : dat_path(std::move(dat_path_))
    , m_dsc_data(std::move(dsc_data_))
    , m_fp(fp) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HardDiskImage::~HardDiskImage() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<HardDiskImage> HardDiskImage::CreateForFile(std::string dat_path, const LogSet &logs) {
    std::string dsc_path = PathWithoutExtension(dat_path) + ".dsc";

    std::vector<uint8_t> dsc_data;
    if (!LoadFile(&dsc_data, dsc_path, &logs)) {
        logs.e.f("Failed to load corresponding .dsc file for hard disk: %s\n", dat_path.c_str());
        return nullptr;
    }

    if (dsc_data.size() != EXPECTED_DSC_SIZE) {
        logs.e.f("Hard disk .dsc file is %zu bytes (%zu expected): %s\n", dsc_data.size(), EXPECTED_DSC_SIZE, dat_path.c_str());
        return nullptr;
    }

    FILE *fp = fopenUTF8(dat_path.c_str(), "r+b");
    if (!fp) {
        logs.e.f("Couldn't open hard disk .dat file: %s\n", dat_path.c_str());
        return nullptr;
    }

    auto image = std::make_shared<HardDiskImage>(std::move(dat_path), std::move(dsc_data), fp);

    return image;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::ReadSector(uint8_t *dest, uint32_t block) {
    if (!this->SeekToBlock(block)) {
        return false;
    }

    size_t n = fread(dest, 1, 256, m_fp);
    if (n != 256) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::WriteSector(const uint8_t *src, uint32_t block) {
    if (!this->SeekToBlock(block)) {
        return false;
    }

    size_t n = fwrite(src, 1, 256, m_fp);
    if (n != 256) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HardDiskGeometry HardDiskImage::GetGeometry() const {
    HardDiskGeometry g;

    g.num_heads = m_dsc_data[15];
    g.num_cylinders = m_dsc_data[13] << 8 | m_dsc_data[14];

    return g;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::IsValidLBA(uint32_t lba) const {
    HardDiskGeometry g = this->GetGeometry();

    uint32_t n = g.num_heads * g.num_cylinders * NUM_HARD_DISK_SECTORS;
    if (lba < n) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::SeekToBlock(uint32_t block) {
    // TODO: add cross platform 64-bit seek...
    if (block > MAX_BLOCK) {
        return false;
    }

    long offset = (long)block * 256;
    if (fseek(m_fp, offset, SEEK_SET) != 0) {
        return false;
    }

    return true;
}
