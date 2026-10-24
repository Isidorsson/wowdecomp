#include "Engine/Source/Retail/CascSource.h"
#include <CascLib.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "Engine/Source/Retail/DecodedCache.h"

namespace Retail {

static const char* DEFAULT_CONFIG = "Data/Retail/RetailCasc.ini";

static bool ReadText(const std::filesystem::path& path, std::string& text) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    std::ostringstream contents;
    contents << input.rdbuf();
    text = contents.str();
    return true;
}

static bool CascFail(std::string& error, const std::string& message) {
    error = message + " (CascLib error " + std::to_string(GetCascError()) + ")";
    return false;
}

CascSource& CascSource::Instance() {
    static CascSource s_source;
    static std::once_flag s_configured;

    std::call_once(s_configured, [] {
        const char* configured = std::getenv("WOW_RETAIL_CONFIG");
        std::filesystem::path path = configured && *configured ? configured : DEFAULT_CONFIG;
        std::error_code ec;

        if (configured || std::filesystem::exists(path, ec)) {
            std::string error;
            if (!s_source.Configure(path, error)) {
                s_source.m_configError = error;
            }
        }
    });

    return s_source;
}

CascSource::CascSource() = default;

CascSource::~CascSource() {
    if (this->m_storage) {
        CascCloseStorage(this->m_storage);
    }
}

bool CascSource::Configure(const std::filesystem::path& configPath, std::string& error) {
    std::string text;
    if (!ReadText(configPath, text)) {
        error = "cannot read " + configPath.string();
        return false;
    }

    Config config;
    if (!ParseConfig(text, std::filesystem::absolute(configPath).parent_path(), config, error)) {
        return false;
    }

    if (config.mode == StorageMode::Local) {
        std::string buildInfo;
        if (!ReadText(config.path / ".build.info", buildInfo)) {
            error = "no .build.info in " + config.path.string();
            return false;
        }

        if (!ParseBuildInfo(buildInfo, config.product, config.build, error)) {
            return false;
        }
    }

    AliasTable aliases;
    if (!config.aliases.empty()) {
        std::string rows;
        if (!ReadText(config.aliases, rows)) {
            error = "cannot read " + config.aliases.string();
            return false;
        }

        if (!aliases.Parse(rows, error)) {
            return false;
        }
    }

    this->m_config = config;
    this->m_aliases = aliases;
    this->m_configured = true;
    return true;
}

bool CascSource::Handles(const char* name) const {
    // Skips the allocation for nearly every stock path. retail/<FileDataID> is claimed even when
    // unconfigured, so it fails loudly in Logs/RetailAssets.log instead of reaching the MPQs.
    if (!this->m_aliases.Size() && (name[0] | 0x20) != 'r') {
        return false;
    }

    return ClaimsPath(NormalizePath(name), this->m_aliases);
}

bool CascSource::Resolve(const char* name, uint32_t& fileDataId, std::string& error) const {
    if (!this->m_configured) {
        error = this->m_configError.empty() ? std::string("no ") + DEFAULT_CONFIG + " (or WOW_RETAIL_CONFIG)" : this->m_configError;
        return false;
    }

    std::string normalized = NormalizePath(name);
    if (this->m_aliases.Find(normalized, fileDataId) || ParseRetailPath(normalized, fileDataId)) {
        return true;
    }

    error = "not a retail/<FileDataID> path or alias";
    return false;
}

bool CascSource::IsCached(const char* name, uint32_t& size) {
    uint32_t id;
    std::string error;
    std::vector<uint8_t> bytes;

    if (!this->Resolve(name, id, error) || !ReadDecodedCache(DecodedCachePath(this->m_config, id), bytes)) {
        return false;
    }

    size = static_cast<uint32_t>(bytes.size());
    return true;
}

bool CascSource::Exists(const char* name, std::string& error) {
    std::vector<uint8_t> bytes;
    return this->Read(name, bytes, error);
}

bool CascSource::Read(const char* name, std::vector<uint8_t>& bytes, std::string& error) {
    uint32_t id;
    if (!this->Resolve(name, id, error)) {
        return false;
    }

    std::filesystem::path cachePath = DecodedCachePath(this->m_config, id);
    if (ReadDecodedCache(cachePath, bytes)) {
        return true;
    }

    std::lock_guard<std::mutex> lock(this->m_storageMutex);

    // Another thread may have fetched it while this one waited for the lock.
    if (ReadDecodedCache(cachePath, bytes)) {
        return true;
    }

    if (!this->ReadStorage(id, bytes, error)) {
        return false;
    }

    // A failed cache write only costs a refetch next launch; the bytes are still good.
    std::string cacheError;
    WriteDecodedCache(cachePath, bytes, cacheError);
    return true;
}

bool CascSource::OpenStorage(std::string& error) {
    if (this->m_storage) {
        return true;
    }

    bool online = this->m_config.mode == StorageMode::Online;
    std::string path = this->m_config.path.string();

    CASC_OPEN_STORAGE_ARGS args = {};
    args.Size = sizeof(args);
    args.szLocalPath = path.c_str();
    args.szCodeName = this->m_config.product.c_str();
    args.szRegion = this->m_config.region.c_str();
    args.szBuildKey = this->m_config.build.empty() ? nullptr : this->m_config.build.c_str();
    args.dwLocaleMask = this->m_config.locale;

    if (online) {
        std::error_code ec;
        std::filesystem::create_directories(this->m_config.path, ec);
    }

    HANDLE storage = nullptr;
    if (!CascOpenStorageEx(nullptr, &args, online, &storage)) {
        return CascFail(error, "cannot open " + std::string(online ? "online" : "local") + " CASC storage at " + path);
    }

    this->m_storage = storage;
    return true;
}

bool CascSource::ReadStorage(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error) {
    if (!this->OpenStorage(error)) {
        return false;
    }

    HANDLE file = nullptr;
    if (!CascOpenFile(this->m_storage, CASC_FILE_DATA_ID(fileDataId), this->m_config.locale, CASC_OPEN_BY_FILEID | CASC_STRICT_DATA_CHECK, &file)) {
        return CascFail(error, "FileDataID " + std::to_string(fileDataId) + " is not in this build");
    }

    ULONGLONG size = 0;
    DWORD read = 0;
    bool ok = CascGetFileSize64(file, &size) && size <= DECODED_CACHE_LIMIT;

    if (ok) {
        bytes.resize(static_cast<size_t>(size));
        ok = size == 0 || (CascReadFile(file, bytes.data(), static_cast<DWORD>(size), &read) && read == size);
    }

    CascCloseFile(file);

    if (!ok) {
        bytes.clear();
        return CascFail(error, "cannot read FileDataID " + std::to_string(fileDataId) + " (missing data, encryption key or over the size limit)");
    }

    return true;
}

}
