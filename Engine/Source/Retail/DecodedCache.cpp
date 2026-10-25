#include "Engine/Source/Retail/DecodedCache.h"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <windows.h>
#include "Engine/Source/Retail/Checksum.h"

namespace Retail {

// Entry layout: magic, byte count, checksum, then the original bytes. RDC2 checks it with WordChecksum; RDC1, the first
// format, with byte-wise FNV-1a 64, which took most of a large entry's read.
static constexpr uint32_t CACHE_MAGIC = 0x32434452; // "RDC2"
static constexpr uint32_t FIRST_MAGIC = 0x31434452; // "RDC1"
static constexpr uint32_t HEADER_SIZE = DECODED_CACHE_HEADER_SIZE;

namespace {

struct Header {
    uint32_t magic;
    uint32_t count;
    uint64_t checksum;
};

}

static_assert(sizeof(Header) == HEADER_SIZE, "the header is written as it is laid out");

static uint64_t FirstChecksum(const std::vector<uint8_t>& bytes) {
    uint64_t hash = 14695981039346656037ull;

    for (uint8_t byte : bytes) {
        hash = (hash ^ byte) * 1099511628211ull;
    }

    return hash;
}

std::filesystem::path DecodedCacheFolder(const Config& config) {
    return BuildCacheFolder(config) / "decoded";
}

std::filesystem::path DecodedCachePath(const Config& config, uint32_t fileDataId) {
    return DecodedCacheFolder(config) / std::to_string(fileDataId);
}

static bool ReadExactly(HANDLE file, void* out, uint32_t size) {
    DWORD read = 0;
    return size == 0 || (ReadFile(file, out, size, &read, nullptr) && read == size);
}

// With the right to set its times when the file allows it, so a hit can mark the entry used.
static HANDLE OpenEntry(const std::filesystem::path& path, DWORD access) {
    DWORD share = FILE_SHARE_READ | FILE_SHARE_DELETE;
    HANDLE file = CreateFileW(path.c_str(), access, share, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE && access != GENERIC_READ && GetLastError() == ERROR_ACCESS_DENIED) {
        file = CreateFileW(path.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    }
    return file;
}

// The header, when it agrees with the file's size.
static bool ReadHeader(HANDLE file, Header& header) {
    LARGE_INTEGER size;
    return GetFileSizeEx(file, &size) && ReadExactly(file, &header, HEADER_SIZE) && (header.magic == CACHE_MAGIC || header.magic == FIRST_MAGIC)
        && header.count <= DECODED_CACHE_LIMIT && uint64_t(size.QuadPart) == uint64_t(header.count) + HEADER_SIZE;
}

static bool ReadEntry(HANDLE file, std::vector<uint8_t>& bytes, bool& firstFormat) {
    Header header;
    if (!ReadHeader(file, header)) {
        return false;
    }

    bytes.resize(header.count);
    if (!ReadExactly(file, bytes.data(), header.count)) {
        return false;
    }

    firstFormat = header.magic == FIRST_MAGIC;
    return (firstFormat ? FirstChecksum(bytes) : WordChecksum(bytes.data(), bytes.size())) == header.checksum;
}

bool ReadDecodedCache(const std::filesystem::path& path, std::vector<uint8_t>& bytes) {
    bytes.clear();
    HANDLE file = OpenEntry(path, GENERIC_READ | FILE_WRITE_ATTRIBUTES);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::vector<uint8_t> data;
    bool firstFormat = false;
    bool hit = ReadEntry(file, data, firstFormat);
    if (hit) {
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        SetFileTime(file, nullptr, nullptr, &now);
    }
    CloseHandle(file);

    if (!hit) {
        return false;
    }

    // A failed rewrite leaves the RDC1 entry, which still reads.
    std::string error;
    if (firstFormat) {
        WriteDecodedCache(path, data, error);
    }

    bytes = std::move(data);
    return true;
}

bool PeekDecodedCache(const std::filesystem::path& path, DecodedCacheHead& head) {
    head = DecodedCacheHead();
    HANDLE file = OpenEntry(path, GENERIC_READ);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    Header header;
    bool read = ReadHeader(file, header) && ReadExactly(file, head.magic, std::min<uint32_t>(header.count, sizeof(head.magic)));
    CloseHandle(file);

    if (!read) {
        head = DecodedCacheHead();
        return false;
    }

    head.size = header.count;
    return true;
}

bool WriteDecodedCache(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string& error) {
    if (bytes.size() > DECODED_CACHE_LIMIT) {
        error = "decoded asset exceeds the " + std::to_string(DECODED_CACHE_LIMIT) + " byte cache limit";
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

    Header header = { CACHE_MAGIC, static_cast<uint32_t>(bytes.size()), WordChecksum(bytes.data(), bytes.size()) };
    bool written;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(&header), HEADER_SIZE);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
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
