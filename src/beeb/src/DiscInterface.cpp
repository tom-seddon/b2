#include <shared/system.h>
#include <beeb/DiscInterface.h>
#include <string.h>
#include <beeb/BBCMicro.h>
#include <beeb/6502.h>
#include <stdlib.h>
#include <shared/debug.h>
#include <memory>
#include <shared/mutex.h>
#include <vector>
#include <beeb/roms.h>

#include <shared/enum_def.h>
#include <beeb/DiscInterface.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// More notes:

// Solidisk
// --------
//
// http://stardot.org.uk/forums/viewtopic.php?f=4&t=11633
//
// FDC = fe80
// Control = fe86: -----DSd (D=density, S=side, d=drive)
// No INTRQ
//

// Opus INTRQ
// ----------
//
// http://stardot.org.uk/forums/viewtopic.php?f=4&t=11633#p149049 -
// not connected after all???

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscInterfaceExtraHardwareState ::~DiscInterfaceExtraHardwareState() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscInterface::DiscInterface(std::string config_name_, std::string display_name_, StandardROM fs_rom_, uint16_t fdc_addr_, uint16_t control_addr_, uint32_t flags_)
    : config_name(std::move(config_name_))
    , display_name(std::move(display_name_))
    , fs_rom(fs_rom_)
    , fdc_addr(fdc_addr_)
    , control_addr(control_addr_)
    , flags(flags_) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscInterface::~DiscInterface() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscInterfaceExtraHardwareState> DiscInterface::CreateExtraHardwareState() const {
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscInterfaceExtraHardwareState> DiscInterface::CloneExtraHardwareState(const std::shared_ptr<DiscInterfaceExtraHardwareState> &src) const {
    (void)src;
    ASSERT(!src);

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DiscInterface::InstallExtraHardware(BBCMicro *m, const std::shared_ptr<DiscInterfaceExtraHardwareState> &state) const {
    (void)m, (void)state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char ACORN_1770_CONFIG_NAME[] = "Acorn 1770";

class DiscInterfaceAcorn1770 : public DiscInterface {
  public:
    DiscInterfaceAcorn1770()
        : DiscInterface(ACORN_1770_CONFIG_NAME, "Acorn 1770", StandardROM_Acorn1770DFS, 0xfe84, 0xfe80, 0) {
    }

    DiscInterfaceControl GetControlFromByte(uint8_t value) const override {
        DiscInterfaceControl control;
        control.dden = (value & 8) == 0;
        control.reset = (value & 32) == 0;
        control.side = (value & 4) != 0;

        // DFS 2.26 does this:
        //
        // <pre>
        // 9115 lda $fe80                 A=02 X=00 Y=0E S=F6 P=nvUBdIzC (D=02)
        // 9118 and #$03                  A=02 X=00 Y=0E S=F6 P=nvUBdIzC (D=03)
        // 911A beq $9113                 A=02 X=00 Y=0E S=F6 P=nvUBdIzC (D=00)
        // 911C rts                       A=02 X=00 Y=0E S=F8 P=nvUBdIzC (D=91)
        // </pre>
        //
        // This suggests that one of the drives will always be active. But
        // what if you write %xxxxxx00 to the drive control register? Old
        // model-b would select drive 0 in that case.

        if (value & 1) {
            control.drive = 0;
        } else if (value & 2) {
            control.drive = 1;
        } else {
            control.drive = -1;
        }

        return control;
    }

    uint8_t GetByteFromControl(DiscInterfaceControl control) const override {
        uint8_t value = 0;

        if (control.dden) {
            value |= 8;
        }

        if (control.side) {
            value |= 4;
        }

        // Acorn DFS does this on startup to detect the FDC:
        //
        // <pre>
        // 537493  9115: lda $FE80                A=00 X=00 Y=0E S=F6 P=nvdIZC (D=00)
        // 537497  9118: and #$03                 A=00 X=00 Y=0E S=F6 P=nvdIZC (D=03)
        // 537499  911A: beq $9113                A=00 X=00 Y=0E S=F6 P=nvdIZC (D=01)
        // </pre>
        //
        // So one of bits 0 or 1 always ought to be set.

        if (control.drive == 0) {
            value |= 1;
        } else {
            value |= 2;
        }

        return value;
    }

  protected:
  private:
};

static const DiscInterfaceAcorn1770 DISC_INTERFACE_ACORN_1770_VALUE;
const DiscInterface *const DISC_INTERFACE_ACORN_1770 = &DISC_INTERFACE_ACORN_1770_VALUE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char WATFORD_1770_DDB2_CONFIG_NAME[] = "Watford 1770 (DDB2)";

class DiscInterfaceWatford1770DDB2 : public DiscInterface {
  public:
    DiscInterfaceWatford1770DDB2()
        : DiscInterface(WATFORD_1770_DDB2_CONFIG_NAME, "Watford 1770 (DDB2)", StandardROM_WatfordDDFS_DDB2, 0xfe84, 0xfe80, 0) {
    }

    DiscInterfaceControl GetControlFromByte(uint8_t value) const override {
        DiscInterfaceControl control;
        control.dden = (value & 1) == 0;
        control.side = (value & 2) != 0;
        control.reset = (value & 8) == 0;

        if (value & 4) {
            control.drive = 1;
        } else {
            control.drive = 0;
        }

        return control;
    }

    uint8_t GetByteFromControl(DiscInterfaceControl control) const override {
        uint8_t value = 0;

        if (!control.dden) {
            value |= 1;
        }

        if (control.side) {
            value |= 2;
        }

        if (control.drive != 0) {
            value |= 4;
        }

        return value;
    }

  protected:
  private:
};

static const DiscInterfaceWatford1770DDB2 DISC_INTERFACE_WATFORD_DDB2;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static const char WATFORD_1770_DDB3_CONFIG_NAME[] = "Watford 1770 (DDB3)";

class DiscInterfaceWatford1770DDB3 : public DiscInterface {
  public:
    DiscInterfaceWatford1770DDB3()
        : DiscInterface(WATFORD_1770_DDB3_CONFIG_NAME, "Watford 1770 (DDB3)", StandardROM_WatfordDDFS_DDB3, 0xfe84, 0xfe80, 0) {
    }

    DiscInterfaceControl GetControlFromByte(uint8_t value) const override {
        DiscInterfaceControl control;
        control.dden = (value & 8) == 0;
        control.reset = (value & 32) == 0;
        control.side = (value & 4) != 0;

        if (value & 1) {
            control.drive = 0;
        } else if (value & 2) {
            control.drive = 1;
        } else {
            control.drive = -1;
        }

        return control;
    }

    uint8_t GetByteFromControl(DiscInterfaceControl control) const override {
        uint8_t value = 0;

        if (control.dden) {
            value |= 8;
        }

        if (control.side) {
            value |= 4;
        }

        if (control.drive == 0) {
            value |= 1;
        } else {
            value |= 2;
        }

        return value;
    }

  protected:
  private:
};

static const DiscInterfaceWatford1770DDB3 DISC_INTERFACE_WATFORD_DDB3;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char OPUS_1770_CONFIG_NAME[] = "Opus 1770";

class DiscInterfaceOpus1770 : public DiscInterface {
  public:
    DiscInterfaceOpus1770()
        : DiscInterface(OPUS_1770_CONFIG_NAME, "Opus 1770", StandardROM_OpusDDOS, 0xfe80, 0xfe84, 0) {
    }

    DiscInterfaceControl GetControlFromByte(uint8_t value) const override {
        DiscInterfaceControl control;
        control.dden = (value & 64) != 0;
        control.side = (value & 2) != 0;
        control.drive = value & 1;

        return control;
    }

    uint8_t GetByteFromControl(DiscInterfaceControl control) const override {
        uint8_t value = 0;

        if (control.dden) {
            value |= 64;
        }

        if (control.side) {
            value |= 2;
        }

        if (control.drive != 0) {
            value |= 1;
        }

        return value;
    }

  protected:
  private:
};

static const DiscInterfaceOpus1770 DISC_INTERFACE_OPUS;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This slightly (over-?)complicated arrangement is supposed to reduce
// memory consumption when saving states. Rather than copy the whole
// RAM disc contents, it means only the chunks that have changed need
// be copied.
//
// (The ChallengerRAMChunk objects are potentially shared between
// BBCMicro objects, and so need to be thread safe. No such
// restrictions for ChallengerExtraData, which is part of the BBCMicro
// object.)

// CHALLENGER_CHUNK_SIZE is a tradeoff between fixed/per-chunk
// overhead (smaller chunk size = larger chunk array, more mutexes)
// and state overhead (larger chunk size = more cost per state).
//
// I chose 8K because it sounded vaguely sensible.
//
// It probably ought to be at least 2,560 bytes, because that's how
// much space the Challenger DFS uses for its own storage at the start
// of the RAM. This probably changes often and so there'll likely
// always be one chunk that has to be saved.

#define CHALLENGER_CHUNK_SIZE (8192)

struct ChallengerRAMChunk {
    Mutex mutex;

    size_t num_refs = 1;
    uint8_t data[CHALLENGER_CHUNK_SIZE] = {};
};

class DiscInterfaceChallengerState : public DiscInterfaceExtraHardwareState {
  public:
    explicit DiscInterfaceChallengerState(size_t num_chunks) {
        //ASSERT(ram_size % CHALLENGER_CHUNK_SIZE == 0);

        m_chunks.resize(num_chunks); //ram_size / CHALLENGER_CHUNK_SIZE);
        for (size_t i = 0; i < m_chunks.size(); ++i) {
            m_chunks[i] = new ChallengerRAMChunk;
        }
    }

    // Not actually intended for public use, but std::make_shared needs it to be
    // public, and it's not in a header, so it's not exactly a big problem.
    DiscInterfaceChallengerState(const DiscInterfaceChallengerState &src) = default;

    ~DiscInterfaceChallengerState() {
        for (size_t i = 0; i < m_chunks.size(); ++i) {
            this->DecLockedChunkRef(i, UniqueLock<Mutex>(m_chunks[i]->mutex));
        }
    }

  protected:
  private:
    M6502Word m_page = {};
    std::vector<ChallengerRAMChunk *> m_chunks;

    bool GetChallengerRAMPtr(UniqueLock<Mutex> *lock_ptr, size_t *index_ptr, size_t *offset_ptr, uint8_t addr_lsb) {
        size_t addr = (uint32_t)m_page.w << 8 | addr_lsb;

        size_t chunk_index = addr / CHALLENGER_CHUNK_SIZE;

        if (chunk_index >= m_chunks.size()) {
            return false;
        }

        ChallengerRAMChunk *chunk = m_chunks[chunk_index];

        *lock_ptr = UniqueLock<Mutex>(chunk->mutex);
        *index_ptr = chunk_index;
        *offset_ptr = addr % CHALLENGER_CHUNK_SIZE;

        return true;
    }

    void DecLockedChunkRef(size_t index, UniqueLock<Mutex> lock) {
        ASSERT(index >= 0 && index < m_chunks.size());

        ChallengerRAMChunk *chunk = m_chunks[index];
        m_chunks[index] = 0;

        {
            ASSERT(chunk->num_refs > 0);
            --chunk->num_refs;
            if (chunk->num_refs > 0) {
                return;
            }
        }

        lock.unlock();

        delete chunk;
        chunk = nullptr;
    }

    static uint8_t ReadPagingMSB(void *data, M6502Word addr) {
        (void)addr;
        auto c = (DiscInterfaceChallengerState *)data;

        return c->m_page.b.h;
    }

    static void WritePagingMSB(void *data, M6502Word addr, uint8_t value) {
        (void)addr;
        auto c = (DiscInterfaceChallengerState *)data;

        c->m_page.b.h = value;
    }

    static uint8_t ReadPagingLSB(void *data, M6502Word addr) {
        (void)addr;
        auto c = (DiscInterfaceChallengerState *)data;

        return c->m_page.b.l;
    }

    static void WritePagingLSB(void *data, M6502Word addr, uint8_t value) {
        (void)addr;
        auto c = (DiscInterfaceChallengerState *)data;

        c->m_page.b.l = value;
    }

    static uint8_t ReadRAM(void *data, M6502Word addr) {
        auto c = (DiscInterfaceChallengerState *)data;

        size_t index, offset;
        UniqueLock<Mutex> lock;
        if (c->GetChallengerRAMPtr(&lock, &index, &offset, addr.b.l)) {
            return c->m_chunks[index]->data[offset];
        } else {
            if (addr.b.l & 1) {
                return 255;
            } else {
                return 0;
            }
        }
    }

    static void WriteRAM(void *data, M6502Word addr, uint8_t value) {
        auto c = (DiscInterfaceChallengerState *)data;

        size_t index, offset;
        UniqueLock<Mutex> lock;
        if (!c->GetChallengerRAMPtr(&lock, &index, &offset, addr.b.l)) {
            // Ignore out of range addresses.
            return;
        }

        ChallengerRAMChunk *chunk = c->m_chunks[index];
        ASSERT(chunk->num_refs > 0);

        if (chunk->data[offset] == value) {
            // Ignore writes that don't change memory.
            return;
        }

        if (chunk->num_refs > 1) {
            // Need to duplicate this chunk before modifying it.
            auto new_chunk = new ChallengerRAMChunk;

            memcpy(new_chunk->data, chunk->data, CHALLENGER_CHUNK_SIZE);

            c->DecLockedChunkRef(index, std::move(lock));

            ASSERT(!c->m_chunks[index]);
            c->m_chunks[index] = new_chunk;
            chunk = new_chunk;
        }

        chunk->data[offset] = value;
    }

    friend class DiscInterfaceChallenger;
};

class DiscInterfaceChallenger : public DiscInterface {
    //static const uint16_t FDC_ADDR = 0xfcf8;
    //static const uint16_t CONTROL_ADDR = 0xfcfc;
    //static const uint32_t FLAGS = DiscInterfaceFlag_NoINTRQ;
    static const bool STRETCH = true;
    static const bool INTRQ = false;

  public:
    DiscInterfaceChallenger(std::string config_name, std::string display_name, size_t ram_size)
        : DiscInterface(std::move(config_name), std::move(display_name), StandardROM_OpusChallenger, 0xfcf8, 0xfcfc, DiscInterfaceFlag_NoINTRQ | DiscInterfaceFlag_Uses1MHzBus) {
        ASSERT(ram_size % CHALLENGER_CHUNK_SIZE == 0);
        m_num_state_chunks = ram_size / CHALLENGER_CHUNK_SIZE;
    }

    DiscInterfaceControl GetControlFromByte(uint8_t value) const override {
        DiscInterfaceControl control;
        control.dden = (value & 32) == 0;
        control.side = (value & 1) != 0;

        if (value & 2) {
            control.drive = 0;
        } else if (value & 4) {
            control.drive = 1;
        } else {
            control.drive = -1;
        }

        return control;
    }

    uint8_t GetByteFromControl(DiscInterfaceControl control) const override {
        uint8_t value = 0;

        if (!control.dden) {
            value |= 32;
        }

        if (control.side) {
            value |= 1;
        }

        if (control.drive == 0) {
            value |= 2;
        } else if (control.drive == 1) {
            value |= 4;
        }

        return value;
    }

    std::shared_ptr<DiscInterfaceExtraHardwareState> CreateExtraHardwareState() const override {
        return std::make_shared<DiscInterfaceChallengerState>(m_num_state_chunks);
    }

    std::shared_ptr<DiscInterfaceExtraHardwareState> CloneExtraHardwareState(const std::shared_ptr<DiscInterfaceExtraHardwareState> &src) const override {
        ASSERT(dynamic_cast<const DiscInterfaceChallengerState *>(src.get()));
        auto clone = std::make_shared<DiscInterfaceChallengerState>(*(const DiscInterfaceChallengerState *)src.get());

        for (ChallengerRAMChunk *chunk : clone->m_chunks) {
            LockGuard<Mutex> lock(chunk->mutex);

            ++chunk->num_refs;
        }

        return clone;
    }

    void InstallExtraHardware(BBCMicro *m, const std::shared_ptr<DiscInterfaceExtraHardwareState> &state) const override {
        auto *challenger_state = dynamic_cast<DiscInterfaceChallengerState *>(state.get());
        ASSERT(challenger_state);

        m->SetXFJIO(0xfcfe, &DiscInterfaceChallengerState::ReadPagingMSB, challenger_state, DiscInterfaceChallengerState::WritePagingMSB, challenger_state);
        m->SetXFJIO(0xfcff, &DiscInterfaceChallengerState::ReadPagingLSB, challenger_state, DiscInterfaceChallengerState::WritePagingLSB, challenger_state);

        for (uint16_t addr = 0xfd00; addr < 0xfe00; ++addr) {
            m->SetXFJIO(addr, &DiscInterfaceChallengerState::ReadRAM, challenger_state, &DiscInterfaceChallengerState::WriteRAM, challenger_state);
        }
    }

  protected:
  private:
    size_t m_num_state_chunks = 0;
};

static const char CHALLENGER_256K_CONFIG_NAME[] = "Opus CHALLENGER 256K";
static const DiscInterfaceChallenger DISC_INTERFACE_CHALLENGER_256K(CHALLENGER_256K_CONFIG_NAME, "Opus CHALLENGER 256K", 256 * 1024);

static const char CHALLENGER_512K_CONFIG_NAME[] = "Opus CHALLENGER 512K";
static const DiscInterfaceChallenger DISC_INTERFACE_CHALLENGER_512K(CHALLENGER_512K_CONFIG_NAME, "Opus CHALLENGER 512K", 512 * 1024);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char MASTER_128_CONFIG_NAME[] = "Master 128";

class DiscInterfaceMaster128 : public DiscInterface {
  public:
    DiscInterfaceMaster128()
        : DiscInterface(MASTER_128_CONFIG_NAME, "Master 128", StandardROM_None, 0xfe28, 0xfe24, 0) {
    }

    DiscInterfaceControl GetControlFromByte(uint8_t value) const override {
        DiscInterfaceControl control;
        control.dden = (value & 32) == 0;
        control.reset = (value & 4) == 0;
        control.side = (value & 16) != 0;

        if (value & 1) {
            control.drive = 0;
        } else if (value & 2) {
            control.drive = 1;
        } else {
            control.drive = -1;
        }

        // BeebEm says bit 3 is drive 2...

        return control;
    }

    uint8_t GetByteFromControl(DiscInterfaceControl control) const override {
        uint8_t value = 0;

        if (!control.dden) {
            value |= 32;
        }

        if (control.side) {
            value |= 16;
        }

        if (control.drive == 0) {
            value |= 1;
        } else if (control.drive == 1) {
            value |= 2;
        }

        return value;
    }

  protected:
  private:
};

const DiscInterfaceMaster128 DISC_INTERFACE_MASTER128_VALUE;
const DiscInterface *const DISC_INTERFACE_MASTER128 = &DISC_INTERFACE_MASTER128_VALUE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const DiscInterface *const MODEL_B_DISC_INTERFACES[] = {
    &DISC_INTERFACE_ACORN_1770_VALUE,
    &DISC_INTERFACE_WATFORD_DDB2,
    &DISC_INTERFACE_WATFORD_DDB3,
    &DISC_INTERFACE_OPUS,
    &DISC_INTERFACE_CHALLENGER_256K,
    &DISC_INTERFACE_CHALLENGER_512K,
    NULL,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const DiscInterface *FindDiscInterfaceByConfigName(const char *config_name) {
    for (const DiscInterface *const *di_ptr = MODEL_B_DISC_INTERFACES; *di_ptr != NULL; ++di_ptr) {
        const DiscInterface *di = *di_ptr;

        if (di->config_name == config_name) {
            return di;
        }
    }

    if (DISC_INTERFACE_MASTER128->config_name == config_name) {
        return DISC_INTERFACE_MASTER128;
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
