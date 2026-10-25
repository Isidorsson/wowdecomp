#ifndef ENGINE_SOURCE_RETAIL_CONVERTEDCACHEBUDGET_H
#define ENGINE_SOURCE_RETAIL_CONVERTEDCACHEBUDGET_H

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <thread>

namespace Retail {

// The bytes folder's entries take. Over cap, the least recently used are deleted until they take 90% of it. Only
// entries are counted or deleted: files named by a FileDataID alone.
uint64_t TrimConvertedCache(const std::filesystem::path& folder, uint64_t cap);

// Keeps a converted folder under its cap off the reading threads: the folder is summed once on a worker, published
// entries are added to that sum, and once it passes the cap a worker trims the folder. A cap of 0 never trims.
class ConvertedCacheBudget {
    public:
    ConvertedCacheBudget() = default;
    ~ConvertedCacheBudget();
    ConvertedCacheBudget(const ConvertedCacheBudget&) = delete;
    ConvertedCacheBudget& operator=(const ConvertedCacheBudget&) = delete;

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
