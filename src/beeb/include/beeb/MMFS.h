#ifndef HEADER_MMFS_8F5C3B9A4E2D1A7F // -*- mode:c++ -*-
#define HEADER_MMFS_8F5C3B9A4E2D1A7F

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/system.h>
#include <6502/6502.h>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// MMFS - Memory Mapped Filing System
// Based on MMBeeb.cpp by Martin Mather
// Emulates MMC/SD card interface at 0xFE1C

class MMFS {
  public:
    MMFS();

    void Reset();
    void SetImagePath(const std::string &path);
    std::string GetImagePath() const;
    void SetDebug(bool debug);
    bool GetDebug() const;

    // MMC State Machine
    enum MMCProcess {
        MMC_IDLE = 1,
        MMC_CMD0 = 2,
        MMC_CMD1 = 3,
        MMC_READDATA = 4,
        MMC_WRITEDATA = 5,
        MMC_CARDID = 6,
        MMC_SETBLKLEN = 7
    };

    enum MMCStatus {
        MMC_NOGO = 0,
        MMC_CMD0_OK = 1,
        MMC_CMD1_FAIL = 2,
        MMC_CMD1_OK = 3
    };

    static uint8_t ReadMMFS(void *data, M6502Word addr);
    static void WriteMMFS(void *data, M6502Word addr, uint8_t value);

  protected:
    uint8_t m_shiftreg;
    MMCProcess m_process;
    MMCStatus m_status;
    uint32_t m_counter;
    uint64_t m_address;
    uint64_t m_address_limit;
    uint32_t m_block_length;
    bool m_buffer_empty;
    uint64_t m_buffer_address;
    uint8_t m_buffer[0x200];
    std::string m_image_path;
    bool m_debug;

    static constexpr uint8_t CARD_ID[20] = {
        0xff, 0xfe, 0x01, 0x00, 0x00,
        0x33, 0x32, 0xff, 0xff, 0xff,
        0xff, 0xff, 0x19, 0x88, 0x4f,
        0x7a, 0x34, 0xff, 0x6a, 0xca};

    void ReadSector();
    uint8_t WriteSector();
    void WriteMMC(uint8_t value);

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
