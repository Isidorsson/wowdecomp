#ifndef ENGINE_SOURCE_RETAIL_RETAILPATH_H
#define ENGINE_SOURCE_RETAIL_RETAILPATH_H

#include <cstdint>
#include <map>
#include <string>

namespace Retail {

constexpr const char* RETAIL_PREFIX = "retail/";

// Lowercase with forward slashes, the form every lookup below expects.
std::string NormalizePath(const char* name);

bool IsRetailPath(const std::string& normalized);

// "retail/916541.m2" -> 916541. False for other paths and for ids that are not plain decimal or overflow.
bool ParseRetailPath(const std::string& normalized, uint32_t& fileDataId);

// Rows of "FDID;client\path" that serve a retail file in place of a stock path.
class AliasTable {
    public:
    bool Parse(const std::string& text, std::string& error);
    bool Find(const std::string& normalized, uint32_t& fileDataId) const;
    size_t Size() const { return this->m_ids.size(); }

    private:
    std::map<std::string, uint32_t> m_ids;
};

// What CASC serves: retail/<FileDataID>.<ext> directly under retail/, and aliased stock paths. Named paths
// under retail/ stay with the MPQs, where downported content already lives.
bool ClaimsPath(const std::string& normalized, const AliasTable& aliases);

}

#endif
