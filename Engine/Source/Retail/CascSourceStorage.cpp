#include <CascLib.h>
#include <algorithm>
#include "Engine/Source/Retail/CascSource.h"

// The read path of CascSource below the routing: memory, then the decoded cache, then the CASC storage, opened once
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

// Only a miss in both caches waits for the storage, so a hit never waits for an open or another thread's fetch.
bool CascSource::ReadCached(uint32_t id, uint32_t openFlags, std::vector<uint8_t>& bytes, std::string& error) {
    if (this->m_memory.Find(id, bytes)) {
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

    std::call_once(this->m_prefetchStarted, [this] { this->m_prefetchWorker = std::thread([this] { this->PrefetchLoop(); }); });
    std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
    this->m_prefetchQueue.clear();
    for (uint32_t id : fileDataIds) {
        if (id && !this->m_memory.Has(id)) {
            this->m_prefetchQueue.push_back(id);
        }
    }
    this->m_prefetchWake.notify_one();
}

bool CascSource::IconPending(uint32_t fileDataId) const {
    std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
    return this->m_prefetching == fileDataId
        || std::find(this->m_prefetchQueue.begin(), this->m_prefetchQueue.end(), fileDataId) != this->m_prefetchQueue.end();
}

// A failed read is dropped here: the client's own read of the icon fails the same way and logs why.
void CascSource::PrefetchLoop() {
    std::unique_lock<std::mutex> lock(this->m_prefetchMutex);
    while (true) {
        this->m_prefetchWake.wait(lock, [this] { return this->m_stopping || !this->m_prefetchQueue.empty(); });
        if (this->m_stopping) {
            return;
        }

        this->m_prefetching = this->m_prefetchQueue.front();
        this->m_prefetchQueue.pop_front();
        uint32_t id = this->m_prefetching;
        lock.unlock();
        std::vector<uint8_t> bytes;
        std::string error;
        this->ReadOriginal(id, bytes, error);
        lock.lock();
        this->m_prefetching = 0;
    }
}

void CascSource::StopPrefetch() {
    {
        std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
        this->m_stopping = true;
        this->m_prefetchQueue.clear();
    }
    this->m_prefetchWake.notify_one();
    if (this->m_prefetchWorker.joinable()) {
        this->m_prefetchWorker.join();
    }
}

}
