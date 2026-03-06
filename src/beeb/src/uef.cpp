#include <shared/system.h>
#include <shared/log.h>
#include <shared/load_store.h>
#include <shared/debug.h>
#include <beeb/uef.h>
#include <inttypes.h>

// https://mdfs.net/Docs/Comp/BBC/FileFormat/UEFSpecs.htm
//
// http://electrem.emuunlim.com/UEFSpecs.htm (10/1/2006)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char UEF_IDENTIFIER[] = "UEF File!";
static_assert(sizeof UEF_IDENTIFIER == 10);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool UEFReader::Load(std::vector<uint8_t> data, std::string name, const LogSet *logs) {
    if (data.size() < 12) {
        if (logs) {
            logs->e.f("not a UEF file: too small\n");
        }
        return false;
    }

    if (memcmp(data.data(), UEF_IDENTIFIER, sizeof UEF_IDENTIFIER) != 0) {
        if (logs) {
            logs->e.f("not a UEF file: UEF identifier missing\n");
        }
        return false;
    }

    std::vector<Chunk> chunks;
    size_t i = 12;
    while (i + 6 < data.size()) {
        uint16_t id = Load16LE(&data[i + 0]);
        uint32_t size = Load32LE(&data[i + 2]);

        if (i + 6 + size > data.size()) {
            if (logs) {
                logs->e.f("bad UEF file: chunk $%04" PRIx16 " (+%zu) runs off end of file\n", id, i);
            }
            return false;
        }

        chunks.push_back({id, size, i + 6});

        i += 6;
        i += size;
    }

    m_name = std::move(name);
    m_data = std::move(data);
    m_chunks = std::move(chunks);
    m_minor_version = m_data[10];
    m_major_version = m_data[11];

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &UEFReader::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t UEFReader::GetNumChunks() const {
    return m_chunks.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

UEFChunk UEFReader::GetChunkByIndex(size_t index) const {
    ASSERT(index < m_chunks.size());
    const Chunk *chunk = &m_chunks[index];

    return {chunk->id, chunk->size, m_data.data() + chunk->data_index};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

UEFWriter::UEFWriter(uint8_t major_version, uint8_t minor_version, const LogSet *logs)
    : m_logs(logs) {
    m_data.insert(m_data.end(), UEF_IDENTIFIER, UEF_IDENTIFIER + sizeof UEF_IDENTIFIER);
    m_data.push_back(minor_version);
    m_data.push_back(major_version);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> UEFWriter::Save() {
    return m_data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void UEFWriter::AddChunk(uint16_t id, const std::vector<uint8_t> &data) {
    if (data.size() > UINT32_MAX) {
        if (m_logs) {
            m_logs->e.f("chunk 0x%04X too large: %zu (0x%zx) bytes\n", id, data.size(), data.size());
        }
        return;
    }

    size_t index = m_data.size();
    m_data.resize(m_data.size() + 6);
    Store16LE(&m_data[index + 0], id);
    Store32LE(&m_data[index + 2], (uint32_t)data.size());
    m_data.insert(m_data.end(), data.begin(), data.end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *GetUEFChunkDescription(uint16_t id) {
    switch (id) {
    default:
        return nullptr;

        // names from http://electrem.emuunlim.com/UEFSpecs.htm
    case 0x0000:
        return "origin information chunk";
    case 0x0001:
        return "game instructions / manual or URL";
    case 0x0003:
        return "inlay scan";
    case 0x0005:
        return "target machine chunk";
    case 0x0006:
        return "bit multiplexing information";
    case 0x0007:
        return "extra palette";
    case 0x0008:
        return "ROM hint";
    case 0x0009:
        return "short title";
    case 0x000a:
        return "visible area";
    case 0x0100:
        return "implicit start/stop bit tape data block";
    case 0x0101:
        return "multiplexed data block";
    case 0x0102:
        return "explicit tape data block";
    case 0x0104:
        return "defined tape format data block";
    case 0x0110:
        return "carrier tone";
    case 0x0111:
        return "carrier tone with dummy byte";
    case 0x0112:
        return "integer gap";
    case 0x0116:
        return "floating point gap";
    case 0x0113:
        return "change of base frequency";
    case 0x0114:
        return "security cycles";
    case 0x0115:
        return "phase change";
    case 0x0117:
        return "data encoding format change";
    case 0x0120:
        return "position marker";
    case 0x0130:
        return "tape set info";
    case 0x0131:
        return "start of tape side";
    case 0x0200:
        return "disc info";
    case 0x0201:
        return "single implicit disc side";
    case 0x0202:
        return "multiplexed disc side";
    case 0x0300:
        return "standard machine rom";
    case 0x0301:
        return "multiplexed machine rom";
    case 0x0400:
        return "6502 standard state";
    case 0x0401:
        return "Electron ULA state";
    case 0x0402:
        return "WD1770 state";
    case 0x0403:
        return "JIM paging register state";
    case 0x0410:
        return "standard memory data";
    case 0x0411:
        return "multiplexed memory data";
    case 0x0412:
        return "multiplexed (partial) 6502 state";
    case 0x0420:
        return "Slogger Master RAM Board State";
    case 0xFF00:
        return "emulator identification string";
    }
}
