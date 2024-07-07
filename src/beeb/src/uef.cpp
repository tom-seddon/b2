#include <shared/system.h>
#include <shared/log.h>
#include <shared/load_store.h>
#include <shared/debug.h>
#include <beeb/uef.h>
#include <inttypes.h>

// https://mdfs.net/Docs/Comp/BBC/FileFormat/UEFSpecs.htm

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char UEF_IDENTIFIER[] = "UEF File!";
static_assert(sizeof UEF_IDENTIFIER == 10);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

UEFReader::UEFReader(const LogSet *logs)
    : m_logs(logs) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool UEFReader::Load(std::vector<uint8_t> data) {
    if (data.size() < 12) {
        m_logs->e.f("not a UEF file: too small\n");
        return false;
    }

    if (memcmp(data.data(), UEF_IDENTIFIER, sizeof UEF_IDENTIFIER) != 0) {
        m_logs->e.f("not a UEF file: UEF identifier missing\n");
        return false;
    }

    std::vector<Chunk> chunks;
    size_t i = 12;
    while (i + 6 < data.size()) {
        uint16_t id = Load16LE(&data[i + 0]);
        uint32_t size = Load32LE(&data[i + 2]);

        if (i + 6 + size > data.size()) {
            m_logs->e.f("bad UEF file: chunk $%04" PRIx16 " (+%zu) runs off end of file\n", id, i);
            return false;
        }

        chunks.push_back({id, size, i + 6});

        i += 6;
        i += size;
    }

    m_data = std::move(data);
    m_chunks = std::move(chunks);
    m_minor_version = data[10];
    m_major_version = data[11];

    return true;
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
        m_logs->e.f("chunk 0x%04X too large: %zu (0x%zx) bytes\n", id, data.size(), data.size());
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
