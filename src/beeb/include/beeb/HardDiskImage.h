#ifndef HEADER_2505EB908E004383A7164232E3A47593 // -*- mode:c++ -*-
#define HEADER_2505EB908E004383A7164232E3A47593

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

struct LogSet;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Somewhat similar to DiscImage, but for hard disks specifically.
//
// Hard disk images have restricted geometry options, different plans for future
// extension (there'll be no .fsd equivalent for example), and (on account of
// the sizes involved) cloning is not currently an option.
//
// shared_ptr<HardDiskImage> is a thing only because it simplifies the BBC Micro
// state management.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// SCSI images are stored with 256 byte sectors, LBA order.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const size_t EXPECTED_DSC_SIZE = 22;

struct HardDiskGeometry {
    uint8_t num_heads = 0;
    uint16_t num_cylinders = 0;
};

class HardDiskImage : public std::enable_shared_from_this<HardDiskImage> {
  public:
    const std::string dat_path;

    HardDiskImage(std::string dat_path, std::vector<uint8_t> dsc_data, FILE *fp);
    ~HardDiskImage();

    HardDiskImage(const HardDiskImage &) = delete;
    HardDiskImage(HardDiskImage &&) = delete;
    HardDiskImage &operator=(const HardDiskImage &) = delete;
    HardDiskImage &operator=(HardDiskImage &&) = delete;

    static std::shared_ptr<HardDiskImage> CreateForFile(std::string dat_path, const LogSet &logs);

    bool ReadSector(uint8_t *dest, uint32_t block);
    bool WriteSector(const uint8_t *src, uint32_t block);

    HardDiskGeometry GetGeometry() const;
    bool IsValidLBA(uint32_t lba) const;

    // If successful, fills up to 256 bytes of SCSI data, as per the Mode Sense
    // result.
    bool GetSCSIDeviceParameters(uint8_t *buffer) const;

    bool SetSCSIDeviceParameters(const uint8_t *buffer, size_t buffer_size_bytes);

    // Truncates the file - performed by re-opening the file, which could fail.
    bool Format();

  protected:
  private:
    std::vector<uint8_t> m_dsc_data;
    FILE *m_fp = nullptr;

    bool SeekToBlock(uint32_t block);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
