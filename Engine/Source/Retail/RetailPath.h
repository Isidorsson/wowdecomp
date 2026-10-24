#ifndef ENGINE_SOURCE_RETAIL_RETAILPATH_H
#define ENGINE_SOURCE_RETAIL_RETAILPATH_H

#include <cstdint>
#include <map>
#include <string>

namespace Retail {

constexpr const char* RETAIL_PREFIX = "retail/";
constexpr const char* RETAIL_FILE_PREFIX = "retail-";

// Lowercase with forward slashes, the form every lookup below expects.
std::string NormalizePath(const char* name);

bool IsRetailPath(const std::string& normalized);

// "retail/916541.m2" -> 916541, and a retail file name in any folder: "item/objectcomponents/weapon/
// retail-8266369.m2" -> 8266369. False for other paths and for ids that are not plain decimal or overflow.
bool ParseRetailPath(const std::string& normalized, uint32_t& fileDataId);

// The name a served display row gives a retail file where the client puts a stock folder before it: "retail-<fdid>",
// to which the client adds its own extension (or the row does, for a model).
std::string RetailFileName(uint32_t fileDataId);

// The name a served display row gives a body section's texture, to which the client adds "_U", else "_M" or "_F" by
// the character's gender: "retail-<fdid>" when one file is for both, else "retail-<male>-<female>" (0 for none).
// ParseRetailPath reads back "_u" of the first and "_m" / "_f" of the second.
std::string RetailBodyTextureName(uint32_t male, uint32_t female);

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

// The skin the client asks for for view of model, as MakeSkinFileName builds it: the path without its
// extension, then the two-digit view and ".skin".
std::string SkinPathFor(const std::string& model, uint32_t view);

}

#endif
