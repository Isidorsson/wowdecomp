#include "Engine/Source/Retail/ConvertedCache.h"
#include <atomic>
#include <cstring>
#include <fstream>
#include <windows.h>
#include "Engine/Source/Retail/CompressedBlocks.h"
#if WOWDECOMP_RETAIL_SOURCE_HASH
#include "RetailConverterHash.h"
#else
#define RETAIL_CONVERTER_SOURCE_HASH 0ull
#endif

namespace Retail {

// Entry layout: magic, converter key, the m2's size, the rest's size, the packed size, then the m2 followed by the rest
// as CompressedBlocks. The rest is the skin FileDataIDs, the skins (a count of NO_SKIN_BYTES when the conversion kept
// none), the remapped sequences and the warnings.
static constexpr uint32_t CACHE_MAGIC = 0x32434352; // "RCC2"
static constexpr uint32_t MAGIC_FAMILY_MASK = 0x00FFFFFF;
static constexpr uint64_t HEADER_SIZE = 36;
static constexpr uint32_t NO_SKIN_BYTES = 0xFFFFFFFF;

uint64_t ConverterKey() {
    return (uint64_t(CONVERTER_VERSION) * 0x9e3779b97f4a7c15ull) ^ RETAIL_CONVERTER_SOURCE_HASH;
}

std::filesystem::path ConvertedCacheFolder(const Config& config) {
    return BuildCacheFolder(config) / "converted";
}

std::filesystem::path ConvertedCachePath(const Config& config, uint32_t fileDataId) {
    return ConvertedCacheFolder(config) / std::to_string(fileDataId);
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

#pragma pack(push, 1)
struct Header {
    uint32_t magic;
    uint64_t key;
    uint64_t m2Size;
    uint64_t restSize;
    uint64_t packedSize;
};
#pragma pack(pop)
static_assert(sizeof(Header) == HEADER_SIZE, "the header is written as it is laid out");

static bool ReadExactly(HANDLE file, void* out, uint64_t size) {
    DWORD read = 0;
    return size == 0 || (ReadFile(file, out, static_cast<DWORD>(size), &read, nullptr) && read == size);
}

enum class Entry {
    Hit,
    Miss,
    Stale,
};

// Another converted cache format or another converter's entry, as opposed to damage.
static bool IsStale(const Header& header, uint64_t key) {
    return (header.magic & MAGIC_FAMILY_MASK) == (CACHE_MAGIC & MAGIC_FAMILY_MASK) && (header.magic != CACHE_MAGIC || header.key != key);
}

// Win32 reads: a 36 MB entry through ifstream takes twice as long.
static Entry ReadEntry(HANDLE file, uint64_t key, ConvertedModel& model) {
    LARGE_INTEGER size;
    Header header;
    if (!GetFileSizeEx(file, &size) || !ReadExactly(file, &header, HEADER_SIZE)) {
        return Entry::Miss;
    }
    if (IsStale(header, key)) {
        return Entry::Stale;
    }

    if (header.magic != CACHE_MAGIC || header.m2Size > CONVERTED_CACHE_LIMIT || header.restSize > CONVERTED_CACHE_LIMIT
        || uint64_t(size.QuadPart) != HEADER_SIZE + header.packedSize) {
        return Entry::Miss;
    }

    std::vector<uint8_t> packed(static_cast<size_t>(header.packedSize));
    size_t m2Size = static_cast<size_t>(header.m2Size);
    model.m2.resize(m2Size + static_cast<size_t>(header.restSize));
    if (!ReadExactly(file, packed.data(), header.packedSize) || !DecompressBlocks(packed.data(), packed.size(), model.m2.data(), model.m2.size())) {
        return Entry::Miss;
    }

    std::vector<uint8_t> rest(model.m2.begin() + m2Size, model.m2.end());
    model.m2.resize(m2Size);
    return Deserialize(rest, model) ? Entry::Hit : Entry::Miss;
}

// The header of an entry this key reads, sized as its payload says.
static bool IsCurrentHeader(HANDLE file, uint64_t key, Header& header) {
    LARGE_INTEGER size;
    return GetFileSizeEx(file, &size) && ReadExactly(file, &header, HEADER_SIZE) && header.magic == CACHE_MAGIC && header.key == key
        && header.m2Size <= CONVERTED_CACHE_LIMIT && header.restSize <= CONVERTED_CACHE_LIMIT && uint64_t(size.QuadPart) == HEADER_SIZE + header.packedSize;
}

bool HasConvertedCache(const std::filesystem::path& path, uint64_t key) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    Header header;
    bool current = IsCurrentHeader(file, key, header);
    CloseHandle(file);
    return current;
}

// With the right to set its times when the file allows it, so a hit can mark the entry used.
static HANDLE OpenEntry(const std::filesystem::path& path) {
    DWORD share = FILE_SHARE_READ | FILE_SHARE_DELETE;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | FILE_WRITE_ATTRIBUTES, share, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED) {
        file = CreateFileW(path.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    }
    return file;
}

static void MarkUsed(HANDLE file) {
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    SetFileTime(file, nullptr, nullptr, &now);
}

bool ReadConvertedCache(const std::filesystem::path& path, uint64_t key, ConvertedModel& model) {
    model = ConvertedModel();
    HANDLE file = OpenEntry(path);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    ConvertedModel read;
    Entry entry = ReadEntry(file, key, read);
    if (entry == Entry::Hit) {
        MarkUsed(file);
    }
    CloseHandle(file);

    if (entry == Entry::Stale) {
        DeleteFileW(path.c_str());
    }
    if (entry == Entry::Hit) {
        model = std::move(read);
    }
    return entry == Entry::Hit;
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

    std::vector<uint8_t> payload;
    payload.reserve(model.m2.size() + rest.size());
    payload.insert(payload.end(), model.m2.begin(), model.m2.end());
    payload.insert(payload.end(), rest.begin(), rest.end());
    std::vector<uint8_t> packed = CompressBlocks(payload.data(), payload.size());

    // Unique per process and call: two clients sharing a cache never write the same temporary file.
    static std::atomic<uint32_t> s_serial { 0 };
    std::filesystem::path temporary = path;
    temporary += "." + std::to_string(GetCurrentProcessId()) + "." + std::to_string(++s_serial) + ".tmp";

    Header header = { CACHE_MAGIC, key, model.m2.size(), rest.size(), packed.size() };
    bool written;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(&header), HEADER_SIZE);
        output.write(reinterpret_cast<const char*>(packed.data()), packed.size());
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
