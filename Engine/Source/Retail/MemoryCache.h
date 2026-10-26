#ifndef ENGINE_SOURCE_RETAIL_MEMORYCACHE_H
#define ENGINE_SOURCE_RETAIL_MEMORYCACHE_H

#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Retail {

// Small decoded originals kept in memory by FileDataID, so a file read again (an icon scrolled back into view) skips the
// disk. Past the byte cap the least recently used are dropped; a file over the entry limit is never kept. Thread-safe.
class MemoryCache {
    public:
    MemoryCache(uint64_t cap, uint32_t entryLimit) : m_cap(cap), m_entryLimit(entryLimit) {}
    MemoryCache(const MemoryCache&) = delete;
    MemoryCache& operator=(const MemoryCache&) = delete;

    // A copy of the entry, which becomes the most recently used.
    bool Find(uint32_t fileDataId, std::vector<uint8_t>& bytes);
    // Without changing what is least recently used.
    bool Has(uint32_t fileDataId) const;
    void Put(uint32_t fileDataId, const std::vector<uint8_t>& bytes);
    uint64_t Bytes() const;

    private:
    using Entry = std::pair<uint32_t, std::vector<uint8_t>>;

    void Drop(std::list<Entry>::iterator entry);

    mutable std::mutex m_mutex;
    std::list<Entry> m_entries;   // Most recently used first.
    std::unordered_map<uint32_t, std::list<Entry>::iterator> m_byId;
    uint64_t m_cap;
    uint32_t m_entryLimit;
    uint64_t m_bytes = 0;
};

}

#endif
