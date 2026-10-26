#include "Engine/Source/Retail/MemoryCache.h"
#include <iterator>

namespace Retail {

bool MemoryCache::Find(uint32_t fileDataId, std::vector<uint8_t>& bytes) {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    auto found = this->m_byId.find(fileDataId);
    if (found == this->m_byId.end()) {
        return false;
    }

    this->m_entries.splice(this->m_entries.begin(), this->m_entries, found->second);
    bytes = found->second->second;
    return true;
}

bool MemoryCache::Has(uint32_t fileDataId) const {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    return this->m_byId.count(fileDataId) != 0;
}

void MemoryCache::Put(uint32_t fileDataId, const std::vector<uint8_t>& bytes) {
    if (bytes.size() > this->m_entryLimit || bytes.size() > this->m_cap) {
        return;
    }

    std::lock_guard<std::mutex> lock(this->m_mutex);
    auto found = this->m_byId.find(fileDataId);
    if (found != this->m_byId.end()) {
        this->Drop(found->second);
    }

    while (this->m_bytes + bytes.size() > this->m_cap) {
        this->Drop(std::prev(this->m_entries.end()));
    }

    this->m_entries.emplace_front(fileDataId, bytes);
    this->m_byId[fileDataId] = this->m_entries.begin();
    this->m_bytes += bytes.size();
}

uint64_t MemoryCache::Bytes() const {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    return this->m_bytes;
}

// With the mutex held.
void MemoryCache::Drop(std::list<Entry>::iterator entry) {
    this->m_bytes -= entry->second.size();
    this->m_byId.erase(entry->first);
    this->m_entries.erase(entry);
}

}
