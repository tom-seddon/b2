#include <shared/system.h>
#include <beeb/HardDiskImage.h>
#include <shared/path.h>
#include <shared/file_io.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <shared/load_store.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// See also http://www.cowsarenotpurple.co.uk/bbccomputer/native/adfs.html#imsandems

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// BBC hard disks always have 33 sectors.
static const uint8_t NUM_HARD_DISK_SECTORS = 33;

// 8-bit ADFS sector number is a 21 bit quantity, so max 2097152 sectors.
//
// If no .dsc file specified, use these parameters, for 2097480 sectors total.
static constexpr uint16_t DEFAULT_NUM_CYLINDERS = 6356;
static constexpr uint8_t DEFAULT_NUM_HEADS = 10;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetDSCPath(const std::string &dat_path) {
    return PathWithoutExtension(dat_path) + ".dsc";
}

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
    std::string dsc_path = GetDSCPath(dat_path);

    std::vector<uint8_t> dsc_data;
    if (!LoadFile(&dsc_data, dsc_path, &logs)) {
        dsc_data.resize(EXPECTED_DSC_SIZE, 0);

        // See
        // http://www.cowsarenotpurple.co.uk/bbccomputer/native/adfs.html#imsandems
        dsc_data[3] = 0x80;
        dsc_data[10] = 1;
        dsc_data[12] = 1;
        Store16BE(&dsc_data[13], DEFAULT_NUM_CYLINDERS);
        dsc_data[15] = DEFAULT_NUM_HEADS;
        dsc_data[17] = 0x80;
        dsc_data[19] = 0x80;
        dsc_data[21] = 0; //3ms

        logs.w.f("No .dsc file for disk image: %s\n", dat_path.c_str());
    } else {
        if (dsc_data.size() != EXPECTED_DSC_SIZE) {
            logs.e.f("Hard disk .dsc file is %zu bytes (%zu expected): %s\n", dsc_data.size(), EXPECTED_DSC_SIZE, dat_path.c_str());
            return nullptr;
        }
    }

    FILE *fp = fopenUTF8(dat_path.c_str(), "r+b");
    if (!fp) {
        logs.w.f("Couldn't open hard disk .dat file: %s\n", dat_path.c_str());

        // ...but that's ok! You can format it.
    }

    logs.i.f("%u H x %u C x %u S: %s\n", dsc_data[15], Load16BE(&dsc_data[13]), NUM_HARD_DISK_SECTORS, dat_path.c_str());

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
        if (n < 256 && feof(m_fp)) {
            // Reading past eof is ok - the disk image does not have to be full
            // size! Pretend the read was all 0 bytes.
            memset(dest + n, 0, 256 - n);
        } else {
            return false;
        }
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

    uint32_t n = (uint32_t)(g.num_heads * g.num_cylinders * NUM_HARD_DISK_SECTORS);
    if (lba < n) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::GetSCSIDeviceParameters(uint8_t *buffer) const {
    ASSERT(buffer);
    ASSERT(m_dsc_data.size() < 256);

    for (size_t i = 0; i < m_dsc_data.size(); ++i) {
        buffer[i] = m_dsc_data[i];
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::SetSCSIDeviceParameters(const uint8_t *buffer, size_t buffer_size_bytes) {
    if (buffer_size_bytes < EXPECTED_DSC_SIZE) {
        return false;
    }

    m_dsc_data.resize(EXPECTED_DSC_SIZE);
    for (size_t i = 0; i < EXPECTED_DSC_SIZE; ++i) {
        m_dsc_data[i] = buffer[i];
    }

    std::string dsc_path = GetDSCPath(this->dat_path);
    FILE *fp = fopenUTF8(dsc_path.c_str(), "wb");
    if (!fp) {
        return false;
    }

    size_t n = fwrite(m_dsc_data.data(), 1, m_dsc_data.size(), fp);

    fclose(fp), fp = nullptr;

    if (n != m_dsc_data.size()) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::Format() {
    if (m_fp) {
        fclose(m_fp), m_fp = nullptr;
    }

    // If the truncate fails, ignore it. The standard does say that there's no
    // guarantee that the format command actually does anything.
    m_fp = fopenUTF8(this->dat_path.c_str(), "wb");
    if (m_fp) {
        fclose(m_fp), m_fp = nullptr;
    }

    m_fp = fopenUTF8(this->dat_path.c_str(), "r+b");
    if (!m_fp) {
        // Welp... this fopen failure is definitely an error!
        return false;
    }

    // Just leave the newly created file at 0 bytes. It'll get extended as
    // required as sectors are written.
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HardDiskImage::SeekToBlock(uint32_t block) {
    if (!m_fp) {
        return false;
    }

    if (!this->IsValidLBA(block)) {
        return false;
    }

    if (!m_fp) {
        return false;
    }

    long offset = (long)block * 256;
    if (fseek(m_fp, offset, SEEK_SET) != 0) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
