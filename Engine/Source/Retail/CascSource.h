#ifndef ENGINE_SOURCE_RETAIL_CASCSOURCE_H
#define ENGINE_SOURCE_RETAIL_CASCSOURCE_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailConfig.h"
#include "Engine/Source/Retail/RetailPath.h"

namespace Retail {

// Serves retail/<FileDataID> paths and aliased stock paths from one pinned CASC build. Reads come from the
// decoded cache when possible; a miss opens the storage (online: downloads build metadata) and blocks.
class CascSource {
    public:
    // Process-wide source configured from WOW_RETAIL_CONFIG, else Data/Retail/RetailCasc.ini. Without a
    // config file it handles nothing; a broken one leaves ConfigError() set.
    static CascSource& Instance();

    CascSource();
    ~CascSource();
    CascSource(const CascSource&) = delete;
    CascSource& operator=(const CascSource&) = delete;

    bool Configure(const std::filesystem::path& configPath, std::string& error);
    const std::string& ConfigError() const { return this->m_configError; }

    // Cheap and lock-free: called on every SFile open.
    bool Handles(const char* name) const;
    bool Exists(const char* name, std::string& error);
    bool IsCached(const char* name, uint32_t& size);
    bool Read(const char* name, std::vector<uint8_t>& bytes, std::string& error);

    private:
    bool Resolve(const char* name, uint32_t& fileDataId, std::string& error) const;
    bool OpenStorage(std::string& error);
    bool ReadStorage(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error);

    bool m_configured = false;
    std::string m_configError;
    Config m_config;
    AliasTable m_aliases;
    std::mutex m_storageMutex;
    void* m_storage = nullptr;
};

}

#endif
