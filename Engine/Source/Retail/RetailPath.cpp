#include "Engine/Source/Retail/RetailPath.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace Retail {

static bool ParseFileDataId(const std::string& digits, uint32_t& fileDataId) {
    if (digits.empty() || digits.size() > 10 || digits.find_first_not_of("0123456789") != std::string::npos) {
        return false;
    }

    uint64_t value = std::stoull(digits);
    if (value > UINT32_MAX) {
        return false;
    }

    fileDataId = static_cast<uint32_t>(value);
    return true;
}

std::string NormalizePath(const char* name) {
    std::string path(name);

    for (char& c : path) {
        c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return path;
}

bool IsRetailPath(const std::string& normalized) {
    return normalized.compare(0, std::strlen(RETAIL_PREFIX), RETAIL_PREFIX) == 0;
}

bool ParseRetailPath(const std::string& normalized, uint32_t& fileDataId) {
    if (!IsRetailPath(normalized)) {
        return false;
    }

    std::string stem = normalized.substr(std::strlen(RETAIL_PREFIX));
    stem = stem.substr(0, stem.find('.'));
    return ParseFileDataId(stem, fileDataId);
}

bool AliasTable::Parse(const std::string& text, std::string& error) {
    this->m_ids.clear();
    std::istringstream lines(text);
    std::string line;

    for (int number = 1; std::getline(lines, line); number++) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t separator = line.find(';');
        uint32_t id = 0;

        if (separator == std::string::npos || separator + 1 == line.size() || !ParseFileDataId(line.substr(0, separator), id)) {
            error = "aliases line " + std::to_string(number) + " is not FDID;client\\path";
            return false;
        }

        std::string path = NormalizePath(line.substr(separator + 1).c_str());
        if (!this->m_ids.emplace(path, id).second) {
            error = "aliases line " + std::to_string(number) + " repeats " + path;
            return false;
        }
    }

    return true;
}

bool ClaimsPath(const std::string& normalized, const AliasTable& aliases) {
    uint32_t id;
    return ParseRetailPath(normalized, id) || (aliases.Size() && aliases.Find(normalized, id));
}

std::string SkinPathFor(const std::string& model, uint32_t view) {
    char suffix[16];
    std::snprintf(suffix, sizeof(suffix), "%02u.skin", view);
    return model.substr(0, model.rfind('.')) + suffix;
}

bool AliasTable::Find(const std::string& normalized, uint32_t& fileDataId) const {
    auto it = this->m_ids.find(normalized);
    if (it == this->m_ids.end()) {
        return false;
    }

    fileDataId = it->second;
    return true;
}

}
