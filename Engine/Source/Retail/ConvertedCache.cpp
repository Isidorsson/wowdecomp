#include "Engine/Source/Retail/ConvertedCache.h"
#include <atomic>
#include <cstring>
#include <fstream>
#include <windows.h>
#if WOWDECOMP_RETAIL_SOURCE_HASH
#include "RetailConverterHash.h"
#else
#define RETAIL_CONVERTER_SOURCE_HASH 0ull
#endif

namespace Retail {

// Entry layout: magic, converter key, the m2's size and checksum, the rest's size and checksum, the m2, then the rest:
// the skin FileDataIDs, the skins (a count of NO_SKIN_BYTES when the conversion kept none), the remapped sequences and
// the warnings. The m2 is read straight into place, since it is most of a large entry.
static constexpr uint32_t CACHE_MAGIC = 0x31434352; // "RCC1"
static constexpr uint64_t HEADER_SIZE = 44;
static constexpr uint32_t NO_SKIN_BYTES = 0xFFFFFFFF;

// Word-wise, so a 36 MB entry checks in a few milliseconds; the tail is mixed in byte by byte.
static uint64_t Checksum(const std::vector<uint8_t>& bytes) {
    uint64_t hash = 14695981039346656037ull ^ bytes.size();
    size_t words = bytes.size() / 8;

    for (size_t i = 0; i < words; i++) {
        uint64_t word;
        std::memcpy(&word, bytes.data() + i * 8, 8);
        hash = (hash ^ word) * 0x9e3779b97f4a7c15ull;
        hash ^= hash >> 29;
    }

    for (size_t i = words * 8; i < bytes.size(); i++) {
        hash = (hash ^ bytes[i]) * 1099511628211ull;
    }

    return hash;
}

uint64_t ConverterKey() {
    return (uint64_t(CONVERTER_VERSION) * 0x9e3779b97f4a7c15ull) ^ RETAIL_CONVERTER_SOURCE_HASH;
}

std::filesystem::path ConvertedCachePath(const Config& config, uint32_t fileDataId) {
    std::string build = config.build.empty() ? "local" : config.build;
    return config.cache / (config.product + "-" + build) / "converted" / std::to_string(fileDataId);
}

namespace {

class Writer {
    public:
    std::vector<uint8_t> bytes;

    void U16(uint16_t value) { this->Raw(&value, 2); }
    void U32(uint32_t value) { this->Raw(&value, 4); }
    void Blob(const void* data, size_t size) {
        this->U32(static_cast<uint32_t>(size));
        this->Raw(data, size);
    }
    void Raw(const void* data, size_t size) {
        const uint8_t* begin = static_cast<const uint8_t*>(data);
        this->bytes.insert(this->bytes.end(), begin, begin + size);
    }
};

// Every read checks the bytes left, so a corrupt count fails instead of reading past the payload.
class Reader {
    public:
    explicit Reader(const std::vector<uint8_t>& bytes) : m_bytes(bytes) {}

    bool U16(uint16_t& value) { return this->Raw(&value, 2); }
    bool U32(uint32_t& value) { return this->Raw(&value, 4); }
    bool Count(uint32_t& count, size_t itemSize) { return this->U32(count) && uint64_t(count) * itemSize <= this->Left(); }
    template <typename Bytes> bool Blob(Bytes& out) {
        uint32_t size;
        if (!this->Count(size, 1)) {
            return false;
        }
        out.assign(this->m_bytes.begin() + this->m_at, this->m_bytes.begin() + this->m_at + size);
        this->m_at += size;
        return true;
    }
    bool Raw(void* out, size_t size) {
        if (size > this->Left()) {
            return false;
        }
        if (size) {
            std::memcpy(out, this->m_bytes.data() + this->m_at, size);
        }
        this->m_at += size;
        return true;
    }
    size_t Left() const { return this->m_bytes.size() - this->m_at; }

    private:
    const std::vector<uint8_t>& m_bytes;
    size_t m_at = 0;
};

}

static std::vector<uint8_t> Serialize(const ConvertedModel& model) {
    Writer out;
    out.U32(static_cast<uint32_t>(model.skins.size()));
    out.Raw(model.skins.data(), model.skins.size() * 4);

    bool kept = model.skinBytes.size() == model.skins.size();
    out.U32(kept ? static_cast<uint32_t>(model.skinBytes.size()) : NO_SKIN_BYTES);
    for (size_t view = 0; kept && view < model.skinBytes.size(); view++) {
        out.Blob(model.skinBytes[view].data(), model.skinBytes[view].size());
    }

    out.U32(static_cast<uint32_t>(model.remapped.size()));
    for (const RemappedSequence& sequence : model.remapped) {
        out.U16(sequence.index);
        out.U16(sequence.originalId);
        out.U16(sequence.originalVariation);
        out.U16(sequence.id);
    }

    out.U32(static_cast<uint32_t>(model.warnings.size()));
    for (const std::string& warning : model.warnings) {
        out.Blob(warning.data(), warning.size());
    }

    return std::move(out.bytes);
}

static bool ReadSkins(Reader& in, ConvertedModel& model) {
    uint32_t count;
    if (!in.Count(count, 4)) {
        return false;
    }
    model.skins.resize(count);
    if (!in.Raw(model.skins.data(), size_t(count) * 4) || !in.U32(count)) {
        return false;
    }
    if (count == NO_SKIN_BYTES) {
        return true;
    }
    if (count != model.skins.size()) {
        return false;
    }

    model.skinBytes.resize(count);
    for (std::vector<uint8_t>& skin : model.skinBytes) {
        if (!in.Blob(skin)) {
            return false;
        }
    }
    return true;
}

static bool Deserialize(const std::vector<uint8_t>& payload, ConvertedModel& model) {
    Reader in(payload);
    uint32_t count;

    if (!ReadSkins(in, model) || !in.Count(count, 8)) {
        return false;
    }

    model.remapped.resize(count);
    for (RemappedSequence& sequence : model.remapped) {
        if (!in.U16(sequence.index) || !in.U16(sequence.originalId) || !in.U16(sequence.originalVariation) || !in.U16(sequence.id)) {
            return false;
        }
    }

    if (!in.Count(count, 4)) {
        return false;
    }
    model.warnings.resize(count);
    for (std::string& warning : model.warnings) {
        if (!in.Blob(warning)) {
            return false;
        }
    }

    return in.Left() == 0;
}

struct Header {
    uint32_t magic;
    uint64_t key;
    uint64_t m2Size;
    uint64_t m2Checksum;
    uint64_t restSize;
    uint64_t restChecksum;
};

static bool ReadExactly(HANDLE file, void* out, uint64_t size) {
    DWORD read = 0;
    return size == 0 || (ReadFile(file, out, static_cast<DWORD>(size), &read, nullptr) && read == size);
}

// Win32 reads: a 36 MB entry through ifstream takes twice as long.
static bool ReadEntry(HANDLE file, uint64_t key, ConvertedModel& model) {
    LARGE_INTEGER size;
    Header header;
    if (!GetFileSizeEx(file, &size) || !ReadExactly(file, &header.magic, 4) || !ReadExactly(file, &header.key, 40)) {
        return false;
    }

    if (header.magic != CACHE_MAGIC || header.key != key || header.m2Size > CONVERTED_CACHE_LIMIT || header.restSize > CONVERTED_CACHE_LIMIT
        || uint64_t(size.QuadPart) != HEADER_SIZE + header.m2Size + header.restSize) {
        return false;
    }

    std::vector<uint8_t> rest(static_cast<size_t>(header.restSize));
    model.m2.resize(static_cast<size_t>(header.m2Size));
    return ReadExactly(file, model.m2.data(), header.m2Size) && ReadExactly(file, rest.data(), header.restSize)
        && Checksum(model.m2) == header.m2Checksum && Checksum(rest) == header.restChecksum && Deserialize(rest, model);
}

bool ReadConvertedCache(const std::filesystem::path& path, uint64_t key, ConvertedModel& model) {
    model = ConvertedModel();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    ConvertedModel read;
    bool ok = ReadEntry(file, key, read);
    CloseHandle(file);
    if (ok) {
        model = std::move(read);
    }
    return ok;
}

bool WriteConvertedCache(const std::filesystem::path& path, uint64_t key, const ConvertedModel& model, std::string& error) {
    std::vector<uint8_t> rest = Serialize(model);
    if (model.m2.size() > CONVERTED_CACHE_LIMIT || rest.size() > CONVERTED_CACHE_LIMIT) {
        error = "converted model exceeds the " + std::to_string(CONVERTED_CACHE_LIMIT) + " byte cache limit";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "cannot create " + path.parent_path().string() + ": " + ec.message();
        return false;
    }

    // Unique per process and call: two clients sharing a cache never write the same temporary file.
    static std::atomic<uint32_t> s_serial { 0 };
    std::filesystem::path temporary = path;
    temporary += "." + std::to_string(GetCurrentProcessId()) + "." + std::to_string(++s_serial) + ".tmp";

    Header header = { CACHE_MAGIC, key, model.m2.size(), Checksum(model.m2), rest.size(), Checksum(rest) };
    bool written;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(&header.magic), 4);
        output.write(reinterpret_cast<const char*>(&header.key), 40);
        output.write(reinterpret_cast<const char*>(model.m2.data()), model.m2.size());
        output.write(reinterpret_cast<const char*>(rest.data()), rest.size());
        output.close();
        written = !output.fail();
    }

    if (written) {
        written = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }

    if (!written) {
        std::filesystem::remove(temporary, ec);
        error = "cannot write " + path.string();
    }

    return written;
}

}
