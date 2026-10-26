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

static HANDLE OpenEntry(const std::filesystem::path& path) {
    return CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
}

// The header, when it agrees with the file's size.
static bool ReadHeader(HANDLE file, Header& header) {
    LARGE_INTEGER size;
    return GetFileSizeEx(file, &size) && ReadExactly(file, &header, HEADER_SIZE) && (header.magic == CACHE_MAGIC || header.magic == FIRST_MAGIC)
        && header.count <= DECODED_CACHE_LIMIT && uint64_t(size.QuadPart) == uint64_t(header.count) + HEADER_SIZE;
}

static bool ReadEntry(HANDLE file, Header& header, std::vector<uint8_t>& bytes) {
    if (!ReadHeader(file, header)) {
        return false;
    }

    bytes.resize(header.count);
    return ReadExactly(file, bytes.data(), header.count);
}

static bool ChecksumHolds(const Header& header, const std::vector<uint8_t>& bytes) {
    uint64_t checksum = header.magic == FIRST_MAGIC ? FirstChecksum(bytes) : WordChecksum(bytes.data(), bytes.size());
    return checksum == header.checksum;
}

static uint64_t Ticks(const FILETIME& time) {
    return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

// Whether the entry was last marked used DECODED_CACHE_TOUCH_AGE or more ago (FILETIME counts 100 ns).
static bool IsTouchDue(HANDLE file) {
    FILETIME written;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    constexpr uint64_t TICKS_PER_SECOND = 10000000;
    uint64_t age = uint64_t(std::chrono::seconds(DECODED_CACHE_TOUCH_AGE).count()) * TICKS_PER_SECOND;
    return GetFileTime(file, nullptr, nullptr, &written) && Ticks(now) - Ticks(written) >= age;
}

// Opened apart with the right to set its times, so a hit that needs no touch writes nothing. A failure only ages it.
static void TouchEntry(const std::filesystem::path& path) {
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE file = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, share, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    SetFileTime(file, nullptr, nullptr, &now);
    CloseHandle(file);
}

bool VerifiedEntries::Has(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    return this->m_paths.count(path.native()) != 0;
}

void VerifiedEntries::Add(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    this->m_paths.insert(path.native());
}

bool ReadDecodedCache(const std::filesystem::path& path, std::vector<uint8_t>& bytes, VerifiedEntries* verified) {
    bytes.clear();
    HANDLE file = OpenEntry(path);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    Header header;
    std::vector<uint8_t> data;
    bool known = verified && verified->Has(path);
    bool hit = ReadEntry(file, header, data) && (known || ChecksumHolds(header, data));
    bool touch = hit && IsTouchDue(file);
    CloseHandle(file);

    if (!hit) {
        return false;
    }

    if (touch) {
        TouchEntry(path);
    }
    if (verified && !known) {
        verified->Add(path);
    }

    // A failed rewrite leaves the RDC1 entry, which still reads.
    std::string error;
    if (header.magic == FIRST_MAGIC) {
        WriteDecodedCache(path, data, error);
    }

    bytes = std::move(data);
    return true;
}

bool PeekDecodedCache(const std::filesystem::path& path, DecodedCacheHead& head) {
    head = DecodedCacheHead();
    HANDLE file = OpenEntry(path);
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
