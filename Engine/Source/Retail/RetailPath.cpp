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

// A body texture as the client asks for it: "<fdid>_u" for a file of both genders, "<male>-<female>_m" or "_f"
// for gendered ones. A gender without a file (0) is not found, so the section draws nothing for it.
static bool ParseBodyTexture(const std::string& stem, uint32_t& fileDataId) {
    size_t suffix = stem.size() - 2;
    if (stem.size() < 3 || stem[suffix] != '_') {
        return false;
    }

    char gender = stem[suffix + 1];
    std::string files = stem.substr(0, suffix);
    size_t dash = files.find('-');
    if (gender == 'u') {
        return dash == std::string::npos && ParseFileDataId(files, fileDataId);
    }
    if ((gender != 'm' && gender != 'f') || dash == std::string::npos) {
        return false;
    }

    uint32_t male = 0;
    uint32_t female = 0;
    if (!ParseFileDataId(files.substr(0, dash), male) || !ParseFileDataId(files.substr(dash + 1), female)) {
        return false;
    }
    fileDataId = gender == 'm' ? male : female;
    return fileDataId != 0;
}

// A retail file name, "retail-<fdid>.<ext>" or a body texture's, as the last part of a path in any folder.
static bool ParseRetailFileName(const std::string& normalized, uint32_t& fileDataId) {
    size_t name = normalized.rfind('/') + 1;   // 0 without a folder.
    if (normalized.compare(name, std::strlen(RETAIL_FILE_PREFIX), RETAIL_FILE_PREFIX) != 0) {
        return false;
    }

    std::string stem = normalized.substr(name + std::strlen(RETAIL_FILE_PREFIX));
    stem = stem.substr(0, stem.find('.'));
    return ParseFileDataId(stem, fileDataId) || ParseBodyTexture(stem, fileDataId);
}

bool ParseRetailPath(const std::string& normalized, uint32_t& fileDataId) {
    if (!IsRetailPath(normalized)) {
        return ParseRetailFileName(normalized, fileDataId);
    }

    std::string stem = normalized.substr(std::strlen(RETAIL_PREFIX));
    stem = stem.substr(0, stem.find('.'));
    return ParseFileDataId(stem, fileDataId);
}

// ChrRaces ClientPrefix of the stock races 1 to 11, lowercase.
static const char* const RACE_PREFIXES[] = { "hu", "or", "dw", "ni", "sc", "ta", "gn", "tr", "go", "be", "dr" };
constexpr uint32_t STOCK_RACES = sizeof(RACE_PREFIXES) / sizeof(RACE_PREFIXES[0]);
constexpr size_t RACE_SUFFIX = 4;   // "_hum".

bool ParseRaceFileName(const std::string& normalized, RaceFileRequest& request) {
    size_t name = normalized.rfind('/') + 1;
    if (normalized.compare(name, std::strlen(RETAIL_FILE_PREFIX), RETAIL_FILE_PREFIX) != 0) {
        return false;
    }

    std::string stem = normalized.substr(name + std::strlen(RETAIL_FILE_PREFIX));
    stem = stem.substr(0, stem.find('.'));
    if (stem.size() <= RACE_SUFFIX || stem[stem.size() - RACE_SUFFIX] != '_') {
        return false;
    }

    char gender = stem.back();
    std::string prefix = stem.substr(stem.size() - RACE_SUFFIX + 1, 2);
    uint32_t race = 1;
    while (race <= STOCK_RACES && prefix != RACE_PREFIXES[race - 1]) {
        race++;
    }
    uint32_t fileDataId = 0;
    if ((gender != 'm' && gender != 'f') || race > STOCK_RACES || !ParseFileDataId(stem.substr(0, stem.size() - RACE_SUFFIX), fileDataId)) {
        return false;
    }

    request = { fileDataId, race, gender == 'm' ? 0u : 1u };
    return true;
}

constexpr const char* BAKE_MARK = "_bake";

std::string WarpedBakeName(uint32_t fileDataId, uint32_t race, uint32_t gender) {
    return RetailFileName(fileDataId) + BAKE_MARK + std::to_string(race) + (gender ? "f" : "m") + ".blp";
}

bool ParseWarpedBakeName(const std::string& normalized, WarpedBakeRequest& request) {
    size_t name = normalized.rfind('/') + 1;
    if (normalized.compare(name, std::strlen(RETAIL_FILE_PREFIX), RETAIL_FILE_PREFIX) != 0) {
        return false;
    }

    std::string stem = normalized.substr(name + std::strlen(RETAIL_FILE_PREFIX));
    stem = stem.substr(0, stem.find('.'));
    size_t mark = stem.find(BAKE_MARK);
    if (mark == std::string::npos || stem.size() < mark + std::strlen(BAKE_MARK) + 2) {
        return false;
    }

    char gender = stem.back();
    std::string race = stem.substr(mark + std::strlen(BAKE_MARK), stem.size() - mark - std::strlen(BAKE_MARK) - 1);
    uint32_t fileDataId = 0;
    uint32_t raceId = 0;
    if ((gender != 'm' && gender != 'f') || !ParseFileDataId(stem.substr(0, mark), fileDataId) || !ParseFileDataId(race, raceId)) {
        return false;
    }

    request = { fileDataId, raceId, gender == 'm' ? 0u : 1u };
    return true;
}

std::string RacePrefix(uint32_t race) {
    return race >= 1 && race <= STOCK_RACES ? RACE_PREFIXES[race - 1] : std::string();
}

std::string RetailFileName(uint32_t fileDataId) {
    return RETAIL_FILE_PREFIX + std::to_string(fileDataId);
}

std::string RetailBodyTextureName(uint32_t male, uint32_t female) {
    if (male == female) {
        return RetailFileName(male);
    }
    return RetailFileName(male) + "-" + std::to_string(female);
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
    RaceFileRequest request;
    WarpedBakeRequest bake;
    return ParseRetailPath(normalized, id) || ParseRaceFileName(normalized, request) || ParseWarpedBakeName(normalized, bake)
        || (aliases.Size() && aliases.Find(normalized, id));
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
