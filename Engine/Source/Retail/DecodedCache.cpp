#include "Engine/Source/Retail/DecodedCache.h"
#include <atomic>
#include <fstream>
#include <windows.h>

namespace Retail {

// Entry layout: magic, byte count, FNV-1a 64 checksum, then the original bytes.
static constexpr uint32_t CACHE_MAGIC = 0x31434452; // "RDC1"
static constexpr uint32_t HEADER_SIZE = 16;

static uint64_t Checksum(const std::vector<uint8_t>& bytes) {
    uint64_t hash = 14695981039346656037ull;

    for (uint8_t byte : bytes) {
        hash = (hash ^ byte) * 1099511628211ull;
    }

    return hash;
}

std::filesystem::path DecodedCachePath(const Config& config, uint32_t fileDataId) {
    std::string build = config.build.empty() ? "local" : config.build;
    return config.cache / (config.product + "-" + build) / "decoded" / std::to_string(fileDataId);
}

bool ReadDecodedCache(const std::filesystem::path& path, std::vector<uint8_t>& bytes) {
    bytes.clear();
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return false;
    }

    uint64_t size = static_cast<uint64_t>(input.tellg());
    uint32_t magic = 0;
    uint32_t count = 0;
    uint64_t checksum = 0;
    input.seekg(0);
    input.read(reinterpret_cast<char*>(&magic), 4);
    input.read(reinterpret_cast<char*>(&count), 4);
    input.read(reinterpret_cast<char*>(&checksum), 8);

    if (!input || magic != CACHE_MAGIC || count > DECODED_CACHE_LIMIT || size != uint64_t(count) + HEADER_SIZE) {
        return false;
    }

    std::vector<uint8_t> data(count);
    if (!input.read(reinterpret_cast<char*>(data.data()), count) || Checksum(data) != checksum) {
        return false;
    }

    bytes = std::move(data);
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

    uint32_t count = static_cast<uint32_t>(bytes.size());
    uint64_t checksum = Checksum(bytes);
    bool written;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(&CACHE_MAGIC), 4);
        output.write(reinterpret_cast<const char*>(&count), 4);
        output.write(reinterpret_cast<const char*>(&checksum), 8);
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
