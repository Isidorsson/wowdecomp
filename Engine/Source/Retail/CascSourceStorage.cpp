#include <CascLib.h>
#include <algorithm>
#include "Engine/Source/Retail/CascSource.h"

// The read path of CascSource below the routing: memory, then the seed, then the decoded cache, then the CASC storage, opened once
// apart from the reads; the handoff from Exists to the open that follows; and the browser's icon prefetch.

namespace Retail {

static bool CascFail(std::string& error, const std::string& message) {
    error = message + " (CascLib error " + std::to_string(GetCascError()) + ")";
    return false;
}

static void* OpenCascStorage(const Config& config, std::string& error) {
    bool online = config.mode == StorageMode::Online;
    std::string path = config.path.string();

    CASC_OPEN_STORAGE_ARGS args = {};
    args.Size = sizeof(args);
    args.szLocalPath = path.c_str();
    args.szCodeName = config.product.c_str();
    args.szRegion = config.region.c_str();
    args.szBuildKey = config.build.empty() ? nullptr : config.build.c_str();
    args.dwLocaleMask = config.locale;

    if (online) {
        std::error_code ec;
        std::filesystem::create_directories(config.path, ec);
    }

    HANDLE storage = nullptr;
    if (!CascOpenStorageEx(nullptr, &args, online, &storage)) {
        CascFail(error, "cannot open " + std::string(online ? "online" : "local") + " CASC storage at " + path);
        return nullptr;
    }
    return storage;
}

bool CascSource::ReadOriginal(uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
    return this->ReadCached(id, CASC_STRICT_DATA_CHECK, bytes, error);
}

bool CascSource::ReadTable(uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
    return this->ReadCached(id, CASC_STRICT_DATA_CHECK | CASC_OVERCOME_ENCRYPTED, bytes, error);
}

// Only a miss in the seed and the decoded cache waits for the storage, so a hit never waits for an open or another
// thread's fetch. The seed is read first and never written, touched or trimmed.
bool CascSource::ReadCached(uint32_t id, uint32_t openFlags, std::vector<uint8_t>& bytes, std::string& error) {
    if (this->m_memory.Find(id, bytes)) {
        return true;
    }

    if (ReadSeedEntry(SeedEntryPath(this->m_config, id), bytes, &this->m_verified)) {
        this->m_memory.Put(id, bytes);
        return true;
    }

    std::filesystem::path cachePath = DecodedCachePath(this->m_config, id);
    if (ReadDecodedCache(cachePath, bytes, &this->m_verified)) {
        this->m_memory.Put(id, bytes);
        return true;
    }

    if (!this->OpenStorage(error)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(this->m_storageMutex);

    // Another thread may have fetched it while this one waited for the lock.
    if (ReadDecodedCache(cachePath, bytes, &this->m_verified)) {
        this->m_memory.Put(id, bytes);
        return true;
    }

    if (!this->ReadStorage(id, openFlags, bytes, error)) {
        return false;
    }

    // A failed cache write only costs a refetch next launch; the bytes are still good.
    std::string cacheError;
    if (WriteDecodedCache(cachePath, bytes, cacheError)) {
        this->m_decodedBudget.Added(bytes.size() + DECODED_CACHE_HEADER_SIZE);
    }
    this->m_memory.Put(id, bytes);
    return true;
}

bool CascSource::OpenStorage(std::string& error) {
    std::unique_lock<std::mutex> lock(this->m_openMutex);
    if (this->m_storage) {
        return true;
    }

    if (this->m_opening) {
        this->m_opened.wait(lock, [this] { return !this->m_opening; });
        error = this->m_openError;
        return this->m_storage != nullptr;
    }

    this->m_opening = true;
    lock.unlock();
    std::string openError;
    void* storage = this->m_openStorage ? this->m_openStorage(openError) : OpenCascStorage(this->m_config, openError);
    lock.lock();

    this->m_storage = storage;
    this->m_openError = openError;
    this->m_opening = false;
    this->m_opened.notify_all();
    error = openError;
    return storage != nullptr;
}

// With the storage open and m_storageMutex held.
bool CascSource::ReadStorage(uint32_t fileDataId, uint32_t openFlags, std::vector<uint8_t>& bytes, std::string& error) {
    HANDLE file = nullptr;
    if (!CascOpenFile(this->m_storage, CASC_FILE_DATA_ID(fileDataId), this->m_config.locale, CASC_OPEN_BY_FILEID | openFlags, &file)) {
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

// One file: the client opens what it just found, so a newer Exists replaces what an older one kept.
void CascSource::KeepForOpen(const std::string& normalized, std::vector<uint8_t> bytes) {
    std::lock_guard<std::mutex> lock(this->m_keptMutex);
    this->m_keptName = normalized;
    this->m_keptBytes = std::move(bytes);
}

bool CascSource::TakeKept(const std::string& normalized, std::vector<uint8_t>& bytes) {
    std::lock_guard<std::mutex> lock(this->m_keptMutex);
    if (this->m_keptName.empty() || this->m_keptName != normalized) {
        return false;
    }

    bytes = std::move(this->m_keptBytes);
    this->m_keptName.clear();
    this->m_keptBytes = {};
    return true;
}

// Files already in memory are skipped, so a page shown again has nothing pending.
void CascSource::PrefetchIcons(const std::vector<uint32_t>& fileDataIds) {
    if (!this->m_configured) {
        return;
    }

    std::call_once(this->m_iconWorkerStarted, [this] { this->m_iconWorker = std::thread([this] { this->IconPrefetchLoop(); }); });
    std::lock_guard<std::mutex> lock(this->m_iconMutex);
    this->m_iconQueue.clear();
    for (uint32_t id : fileDataIds) {
        if (id && !this->m_memory.Has(id)) {
            this->m_iconQueue.push_back(id);
        }
    }
    this->m_iconWake.notify_one();
}

bool CascSource::IconPending(uint32_t fileDataId) const {
    std::lock_guard<std::mutex> lock(this->m_iconMutex);
    return this->m_iconInFlight == fileDataId
        || std::find(this->m_iconQueue.begin(), this->m_iconQueue.end(), fileDataId) != this->m_iconQueue.end();
}

// A failed read is dropped here: the client's own read of the icon fails the same way and logs why.
void CascSource::IconPrefetchLoop() {
    std::unique_lock<std::mutex> lock(this->m_iconMutex);
    while (true) {
        this->m_iconWake.wait(lock, [this] { return this->m_iconStopping || !this->m_iconQueue.empty(); });
        if (this->m_iconStopping) {
            return;
        }

        this->m_iconInFlight = this->m_iconQueue.front();
        this->m_iconQueue.pop_front();
        uint32_t id = this->m_iconInFlight;
        lock.unlock();
        std::vector<uint8_t> bytes;
        std::string error;
        this->ReadOriginal(id, bytes, error);
        lock.lock();
        this->m_iconInFlight = 0;
    }
}

void CascSource::StopIconPrefetch() {
    {
        std::lock_guard<std::mutex> lock(this->m_iconMutex);
        this->m_iconStopping = true;
        this->m_iconQueue.clear();
    }
    this->m_iconWake.notify_one();
    if (this->m_iconWorker.joinable()) {
        this->m_iconWorker.join();
    }
}

}
