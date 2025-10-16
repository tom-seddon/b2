#include <shared/system.h>
#include <beeb/MMFS.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <stdio.h>
#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// MMFS - Memory Mapped Filing System
// Based on MMBeeb.cpp by Martin Mather
// Emulates MMC/SD card interface at 0xFE1C

LOG_DEFINE(MMFS, "MMFS", &log_printer_stderr_and_debugger, false);

MMFS::MMFS() {
    m_debug = false;
    Reset();
}

void MMFS::Reset() {
    m_shiftreg = 0xff;
    m_process = MMC_IDLE;
    m_status = MMC_NOGO;
    m_counter = 0;
    m_address = 0;
    m_block_length = 0x200;
    m_buffer_empty = true;
    m_buffer_address = 0;
}

void MMFS::SetDebug(bool debug) {
    m_debug = debug;
    if (m_debug) {
        LOGF(MMFS, "MMFS debug logging enabled\n");
    }
}

bool MMFS::GetDebug() const {
    return m_debug;
}

void MMFS::SetImagePath(const std::string &path) {
    m_image_path = path;
    m_address_limit = 0;

    if (m_debug) {
        LOGF(MMFS, "SetImagePath: %s\n", path.empty() ? "(empty)" : path.c_str());
    }

    // Try to open file and get size
    FILE *f = fopen(path.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        m_address_limit = (uint64_t)ftell(f);
        fclose(f);
        if (m_debug) {
            LOGF(MMFS, "  Image loaded: %llu bytes (%.1f MB)\n", 
                 (unsigned long long)m_address_limit,
                 m_address_limit / (1024.0 * 1024.0));
        }
    } else {
        if (m_debug) {
            LOGF(MMFS, "  Failed to open image file\n");
        }
    }
}

std::string MMFS::GetImagePath() const {
    return m_image_path;
}

void MMFS::ReadSector() {
    if (m_buffer_empty || m_address != m_buffer_address) {
        m_buffer_empty = true;

        FILE *f = fopen(m_image_path.c_str(), "rb");
        if (f) {
            // Use fseek in chunks for files > 2GB
            if (fseek(f, 0, SEEK_SET) == 0) {
                uint64_t offset = m_address;
                const uint64_t skip = 0x7FFFFFFE;

                while (offset > 0 && fseek(f, (long)(offset > skip ? skip : offset), SEEK_CUR) == 0) {
                    offset -= offset > skip ? skip : offset;
                }

                if (fread(m_buffer, sizeof(m_buffer), 1, f) == 1) {
                    m_buffer_address = m_address;
                    m_buffer_empty = false;
                }
            }
            fclose(f);
        }
    }
}

uint8_t MMFS::WriteSector() {
    m_buffer_empty = true;

    FILE *f = fopen(m_image_path.c_str(), "r+b");
    if (f) {
        // Use fseek in chunks for files > 2GB
        if (fseek(f, 0, SEEK_SET) == 0) {
            uint64_t offset = m_address;
            const uint64_t skip = 0x7FFFFFFE;

            while (offset > 0 && fseek(f, (long)(offset > skip ? skip : offset), SEEK_CUR) == 0) {
                offset -= offset > skip ? skip : offset;
            }

            if (fwrite(m_buffer, 0x200, 1, f) == 1) {
                m_buffer_address = m_address;
                m_buffer_empty = false;
                fclose(f);
                return 0x05;
            }
        }
        fclose(f);
    }

    return 0xff;
}

void MMFS::WriteMMC(uint8_t value) {
    m_shiftreg = 0xff;
    m_counter++;

    // Read command address
    if (m_process != MMC_IDLE && m_counter < 6) {
        m_address = (m_address << 8) | value;
        return;
    }

    switch (m_process) {
    case MMC_IDLE: // Wait for valid command
        m_counter = 1;
        m_address = 0;
        if (value == 0x40 && m_address_limit > 0) {
            m_status = MMC_NOGO;
            m_process = MMC_CMD0;
        } else if (m_status >= MMC_CMD0_OK) {
            if (value == 0x41)
                m_process = MMC_CMD1;
            else if (m_status == MMC_CMD1_OK) {
                switch (value) {
                case 0x4a:
                    m_process = MMC_CARDID;
                    break;
                case 0x50:
                    m_process = MMC_SETBLKLEN;
                    break;
                case 0x51:
                    m_process = MMC_READDATA;
                    break;
                case 0x58:
                    m_process = MMC_WRITEDATA;
                    break;
                }
            }
        }
        break;

    case MMC_READDATA: // Read data block
        if (m_counter > 9) {
            if (m_counter < (m_block_length + 10)) {
                m_shiftreg = m_buffer[m_counter - 10];
                        } else if (m_counter == (m_block_length + 12)) {
                if (m_debug) {
                    LOGF(MMFS, "Read complete: address=0x%08llX, %u bytes\n",
                         (unsigned long long)m_address, m_block_length);
                }
                m_process = MMC_IDLE;
            }
        } else if (m_counter == 9)
            m_shiftreg = 0xfe;
        else if (m_counter == 8) {
            if (m_address < m_address_limit) {
                ReadSector();
                if (m_buffer_empty) {
                    if (m_debug) {
                        LOGF(MMFS, "ReadSector FAILED at address 0x%08llX\n",
                             (unsigned long long)m_address);
                    }
                    m_shiftreg = 0xff; // read error
                    m_process = MMC_IDLE;
                } else {
                    if (m_debug) {
                        LOGF(MMFS, "ReadSector OK at address 0x%08llX (first bytes: %02X %02X %02X %02X)\n",
                             (unsigned long long)m_address,
                             m_buffer[0], m_buffer[1], m_buffer[2], m_buffer[3]);
                    }
                    m_shiftreg = 0x00;
                }
            } else {
                m_shiftreg = 0x40;
                m_process = MMC_IDLE;
            }
        }
        break;

    case MMC_WRITEDATA: // Write data block
        if (m_counter > 11) {
            if (m_counter < (0x200 + 12))
                m_buffer[m_counter - 12] = value;
            else if (m_counter == (0x200 + 14)) {
                m_shiftreg = WriteSector();
                m_process = MMC_IDLE;
            }
        } else if (m_counter == 8) {
            if ((m_address & 0x1ff) != 0) { // Must be sector-aligned
                m_shiftreg = 0x40;
                m_process = MMC_IDLE;
            } else
                m_shiftreg = 0x00;
        }
        break;

    case MMC_CMD0: // CMD 0 (Reset card)
        if (m_counter == 8) {
            m_shiftreg = 0x01;
            m_status = MMC_CMD0_OK;
            m_process = MMC_IDLE;
            if (m_debug) {
                LOGF(MMFS, "CMD0 complete -> 0x01 (status=CMD0_OK)\n");
            }
        }
        break;

    case MMC_CMD1: // CMD 1 (Initialize card) - must be called twice
        if (m_counter == 8) {
            if (m_status == MMC_CMD0_OK) {
                // First call after CMD0
                m_shiftreg = 0x01;
                m_status = MMC_CMD1_FAIL;
                if (m_debug) {
                    LOGF(MMFS, "CMD1 first call -> 0x01 (status=CMD1_FAIL)\n");
                }
            } else {
                // Second call
                m_shiftreg = 0x00;
                m_status = MMC_CMD1_OK;
                if (m_debug) {
                    LOGF(MMFS, "CMD1 second call -> 0x00 (status=CMD1_OK)\n");
                }
            }
            m_process = MMC_IDLE;
        }
        break;

    case MMC_CARDID: // Return card identification
        if (m_counter == 8) {
            m_shiftreg = 0x00;
        } else if (m_counter > 8) {
            if (m_counter == 28) {
                m_process = MMC_IDLE;
            } else {
                m_shiftreg = CARD_ID[m_counter - 9];
            }
        }
        break;

    case MMC_SETBLKLEN: // Set (Read) Block Length
        if (m_counter == 8) {
            if (m_address != 0 && m_address <= 0x200) {
                m_shiftreg = 0x00;
                m_block_length = (uint32_t)m_address;
            } else
                m_shiftreg = 0x40;
            m_process = MMC_IDLE;
        }
        break;
    }
}

uint8_t MMFS::ReadMMFS(void *data, M6502Word addr) {
    (void)addr;
    auto *mmfs = (MMFS *)data;
    return mmfs->m_shiftreg;
}

void MMFS::WriteMMFS(void *data, M6502Word addr, uint8_t value) {
    (void)addr;
    auto *mmfs = (MMFS *)data;
    mmfs->WriteMMC(value);
}

// Initialize static constexpr member
constexpr uint8_t MMFS::CARD_ID[20];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
