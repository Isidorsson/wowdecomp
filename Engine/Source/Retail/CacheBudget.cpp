#include "Engine/Source/Retail/CacheBudget.h"
#include <algorithm>
#include <string>
#include <vector>

namespace Retail {

static bool IsEntryName(const std::string& name) {
    return !name.empty() && name.size() <= 10 && name.find_first_not_of("0123456789") == std::string::npos;
}

namespace {

struct CachedEntry {
    std::filesystem::file_time_type used;
    uint64_t size;
    std::filesystem::path path;
};

}

uint64_t TrimCacheFolder(const std::filesystem::path& folder, uint64_t cap) {
    std::vector<CachedEntry> entries;
    uint64_t total = 0;
    std::error_code ec;

    for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code entryError;
        CachedEntry entry { it->last_write_time(entryError), 0, it->path() };
        entry.size = it->is_regular_file(entryError) ? it->file_size(entryError) : 0;
        if (!entryError && entry.size && IsEntryName(entry.path.filename().string())) {
            total += entry.size;
            entries.push_back(std::move(entry));
        }
    }

    if (total <= cap) {
        return total;
    }

    std::sort(entries.begin(), entries.end(), [](const CachedEntry& a, const CachedEntry& b) { return a.used < b.used; });
    uint64_t target = cap / 10 * 9;
    for (const CachedEntry& entry : entries) {
        if (total <= target) {
            break;
        }
        // Read or rewritten since the listing, so in use again.
        std::error_code entryError;
        if (std::filesystem::last_write_time(entry.path, entryError) != entry.used || entryError) {
            continue;
        }
        if (std::filesystem::remove(entry.path, ec)) {
            total -= entry.size;
        }
    }
    return total;
}

// The build part as the build cache folders have it: a build key of 32 hex digits, or "local" without one.
static bool IsBuildName(const std::string& build) {
    return build == "local" || (build.size() == 32 && build.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos);
}

std::vector<std::string> RemoveOtherBuilds(const std::filesystem::path& cache, const std::string& product, const std::string& current) {
    std::vector<std::string> removed;
    std::string prefix = product + "-";
    std::error_code ec;

    for (std::filesystem::directory_iterator it(cache, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code entryError;
        std::string name = it->path().filename().string();
        if (name == current || name.compare(0, prefix.size(), prefix) != 0 || !IsBuildName(name.substr(prefix.size()))
            || !it->is_directory(entryError) || it->is_symlink(entryError)) {
            continue;
        }
        std::filesystem::remove_all(it->path(), entryError);
        if (!entryError) {
            removed.push_back(name);
        }
    }
    return removed;
}

CacheBudget::~CacheBudget() {
    this->Wait();
}

void CacheBudget::Start(const std::filesystem::path& folder, uint64_t cap) {
    this->Wait();
    std::lock_guard<std::mutex> lock(this->m_mutex);
    this->m_folder = folder;
    this->m_cap = cap;
    this->m_bytes = 0;
    this->m_counted = false;
    if (cap) {
        this->Launch();
    }
}

void CacheBudget::Added(uint64_t bytes) {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    if (!this->m_cap) {
        return;
    }

    this->m_bytes += bytes;
    if (this->m_trimming) {
        this->m_addedDuringTrim += bytes;
    } else if (this->m_counted && this->m_bytes > this->m_cap) {
        this->Launch();
    }
}

void CacheBudget::Wait() {
    std::thread worker;
    {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        worker = std::move(this->m_worker);
    }
    if (worker.joinable()) {
        worker.join();
    }
}

// Under m_mutex. A finished worker has already left the lock, so joining it here never waits on it.
void CacheBudget::Launch() {
    if (this->m_worker.joinable()) {
        this->m_worker.join();
    }

    this->m_trimming = true;
    this->m_addedDuringTrim = 0;
    this->m_worker = std::thread([this, folder = this->m_folder, cap = this->m_cap] {
        uint64_t total = TrimCacheFolder(folder, cap);
        std::lock_guard<std::mutex> lock(this->m_mutex);
        this->m_bytes = total + this->m_addedDuringTrim;
        this->m_counted = true;
        this->m_trimming = false;
    });
}

}
