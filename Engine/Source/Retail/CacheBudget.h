#ifndef ENGINE_SOURCE_RETAIL_CACHEBUDGET_H
#define ENGINE_SOURCE_RETAIL_CACHEBUDGET_H

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Retail {

// The bytes folder's entries take. Over cap, the least recently used are deleted until they take 90% of it. Only
// entries are counted or deleted: files named by a FileDataID alone.
uint64_t TrimCacheFolder(const std::filesystem::path& folder, uint64_t cap);

// The cache folders of product's other builds directly in cache, deleted: "<product>-<build key>" or "<product>-local",
// as BuildCacheFolder names them, but not current. Returns the names of those it deleted.
std::vector<std::string> RemoveOtherBuilds(const std::filesystem::path& cache, const std::string& product, const std::string& current);

// Keeps a cache folder (decoded or converted) under its cap off the reading threads: the folder is summed once on a worker, published
// entries are added to that sum, and once it passes the cap a worker trims the folder. A cap of 0 never trims.
class CacheBudget {
    public:
    CacheBudget() = default;
    ~CacheBudget();
    CacheBudget(const CacheBudget&) = delete;
    CacheBudget& operator=(const CacheBudget&) = delete;

    void Start(const std::filesystem::path& folder, uint64_t cap);
    void Added(uint64_t bytes);
    // Until the worker in flight, if any, finishes.
    void Wait();

    private:
    void Launch();

    std::mutex m_mutex;
    std::thread m_worker;
    std::filesystem::path m_folder;
    uint64_t m_cap = 0;
    uint64_t m_bytes = 0;
    uint64_t m_addedDuringTrim = 0;
    bool m_counted = false;
    bool m_trimming = false;
};

}

#endif
