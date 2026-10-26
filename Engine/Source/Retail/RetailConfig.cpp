#include "Engine/Source/Retail/RetailConfig.h"
#include <cctype>
#include <sstream>
#include <vector>

namespace Retail {

static std::string Trim(const std::string& text) {
    size_t begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos) {
        return {};
    }

    size_t end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1);
}

static bool IsBuildKey(const std::string& text) {
    if (text.size() != 32) {
        return false;
    }

    for (char c : text) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

static bool ParseLocale(const std::string& text, uint32_t& locale) {
    if (text.empty() || text.size() > 8 || text.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
        return false;
    }

    locale = static_cast<uint32_t>(std::stoul(text, nullptr, 16));
    return true;
}

static bool ParseMegabytes(const std::string& text, uint32_t& megabytes) {
    if (text.empty() || text.size() > 7 || text.find_first_not_of("0123456789") != std::string::npos) {
        return false;
    }

    megabytes = static_cast<uint32_t>(std::stoul(text));
    return true;
}

bool ParseCreatureDisplays(const std::string& text, CreatureDisplays& mode) {
    if (text == "stock") {
        mode = CreatureDisplays::Stock;
    } else if (text == "models") {
        mode = CreatureDisplays::Models;
    } else if (text == "all") {
        mode = CreatureDisplays::All;
    } else {
        return false;
    }
    return true;
}

static bool Fail(std::string& error, const std::string& message) {
    error = "RetailCasc.ini: " + message;
    return false;
}

static bool SetValue(const std::string& key, const std::string& value, const std::filesystem::path& baseDir, Config& config, std::string& error) {
    if (key == "mode") {
        if (value != "online" && value != "local") {
            return Fail(error, "mode must be online or local, not '" + value + "'");
        }

        config.mode = value == "online" ? StorageMode::Online : StorageMode::Local;
    } else if (key == "path") {
        config.path = baseDir / value;
    } else if (key == "cache") {
        config.cache = baseDir / value;
    } else if (key == "seed") {
        config.seed = baseDir / value;
    } else if (key == "product") {
        config.product = value;
    } else if (key == "region") {
        config.region = value;
    } else if (key == "build") {
        config.build = value;
    } else if (key == "locale") {
        if (!ParseLocale(value, config.locale)) {
            return Fail(error, "locale must be a hexadecimal CASC locale mask, not '" + value + "'");
        }
    } else if (key == "converted_cache_mb") {
        if (!ParseMegabytes(value, config.convertedCacheMb)) {
            return Fail(error, "converted_cache_mb must be a whole number of megabytes, not '" + value + "'");
        }
    } else if (key == "decoded_cache_mb") {
        if (!ParseMegabytes(value, config.decodedCacheMb)) {
            return Fail(error, "decoded_cache_mb must be a whole number of megabytes, not '" + value + "'");
        }
    } else if (key == "aliases") {
        config.aliases = baseDir / value;
    } else if (key == "creature_displays") {
        if (!ParseCreatureDisplays(value, config.creatureDisplays)) {
            return Fail(error, "creature_displays must be stock, models or all, not '" + value + "'");
        }
    } else {
        return Fail(error, "unknown key '" + key + "'");
    }

    return true;
}

static std::string BuildFolderName(const Config& config) {
    return config.product + "-" + (config.build.empty() ? std::string("local") : config.build);
}

std::filesystem::path BuildCacheFolder(const Config& config) {
    return config.cache / BuildFolderName(config);
}

std::filesystem::path SeedFolder(const Config& config) {
    return config.seed / BuildFolderName(config);
}

bool ParseConfig(const std::string& text, const std::filesystem::path& baseDir, Config& config, std::string& error) {
    config = Config();
    std::istringstream lines(text);
    std::string line;
    bool hasPath = false;

    for (int number = 1; std::getline(lines, line); number++) {
        line = Trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t equals = line.find('=');
        if (equals == std::string::npos) {
            return Fail(error, "line " + std::to_string(number) + " is not key=value");
        }

        std::string key = Trim(line.substr(0, equals));
        hasPath |= key == "path";

        if (!SetValue(key, Trim(line.substr(equals + 1)), baseDir, config, error)) {
            return false;
        }
    }

    if (!hasPath) {
        return Fail(error, "path is required");
    }

    if (config.cache.empty()) {
        config.cache = baseDir / "Cache";
    }

    if (config.seed.empty()) {
        config.seed = baseDir / "Seed";
    }

    if (config.mode == StorageMode::Online && !IsBuildKey(config.build)) {
        return Fail(error, "online mode needs build=<32-digit build config key>, not '" + config.build + "'");
    }

    return true;
}

static std::vector<std::string> SplitColumns(const std::string& line) {
    std::vector<std::string> columns;
    std::istringstream stream(line);
    std::string column;

    while (std::getline(stream, column, '|')) {
        columns.push_back(Trim(column));
    }

    return columns;
}

static int FindColumn(const std::vector<std::string>& header, const char* name) {
    for (size_t i = 0; i < header.size(); i++) {
        if (header[i].compare(0, header[i].find('!'), name) == 0) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool ParseBuildInfo(const std::string& text, const std::string& product, std::string& buildKey, std::string& error) {
    std::istringstream lines(text);
    std::string line;

    if (!std::getline(lines, line)) {
        error = ".build.info is empty";
        return false;
    }

    std::vector<std::string> header = SplitColumns(line);
    int active = FindColumn(header, "Active");
    int key = FindColumn(header, "Build Key");
    int name = FindColumn(header, "Product");

    if (active < 0 || key < 0 || name < 0) {
        error = ".build.info has no Active, Build Key or Product column";
        return false;
    }

    while (std::getline(lines, line)) {
        std::vector<std::string> row = SplitColumns(line);

        if (row.size() == header.size() && row[active] == "1" && row[name] == product && IsBuildKey(row[key])) {
            buildKey = row[key];
            return true;
        }
    }

    error = ".build.info has no active " + product + " build";
    return false;
}

}
