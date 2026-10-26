#include "Engine/Source/Retail/CascSource.h"
#include <chrono>
#include <cstring>
#include <iterator>
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/DecodedCache.h"

// The retail browser's prefetch half of CascSource: a display the selection rests on is prepared on a worker, so the
// preview that may follow finds its body match, warped bake, converted models and textures in the caches.
//
// What runs on the worker, and why it is safe there:
// - ReadOriginal / ReadCached: decoded-cache files are published by rename, the storage is behind m_storageMutex.
// - Convert: ConvertModel is pure; the display tables and fallbacks are read once (call_once) and then only read;
//   the converted cache is published by rename and CacheBudget locks. Serve is never called: the dependency map and
//   its converted skins belong to the main thread's model loads.
// - Body matches: a BodySlot's call_once, with the installed body read beforehand on the main thread (SFile is not
//   called off it), so a preview arriving mid-match waits for the worker's match instead of matching again.
// - Warped bakes: ReadWarpedBake only once the body is matched, so it never reads the installed body itself.

namespace Retail {

static uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void CascSource::Prefetch(uint32_t displayId) {
    if (!this->m_configured || !displayId) {
        return;
    }
    std::vector<std::pair<uint32_t, StockBody>> read;
    for (const auto& [race, sex] : this->PlannedBodies(displayId)) {
        uint32_t key = race * 2 + sex;
        bool needed;
        {
            std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
            needed = !this->m_prefetchStock.count(key);
        }
        if (needed && !this->BodySlotFor(race, sex).done) {
            read.emplace_back(key, this->ReadStockBody(race, sex));
        }
    }

    {
        std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
        if (this->m_prefetchStop) {
            return;
        }
        // Bodies a preview matched meanwhile are no longer needed (they take megabytes).
        for (auto held = this->m_prefetchStock.begin(); held != this->m_prefetchStock.end();) {
            held = this->BodySlotFor(held->first / 2, held->first % 2).done ? this->m_prefetchStock.erase(held) : std::next(held);
        }
        for (auto& [key, stock] : read) {
            this->m_prefetchStock.try_emplace(key, std::move(stock));
        }
        this->m_prefetchSchedule.Request(displayId, NowMs());
        if (!this->m_prefetchWorker.joinable()) {
            this->m_prefetchWorker = std::thread([this] { this->PrefetchLoop(); });
        }
    }
    this->m_prefetchWake.notify_one();
}

// The installed bodies a humanoid display's plan asks about, once the tables are read (never waited for here).
std::vector<std::pair<uint32_t, uint32_t>> CascSource::PlannedBodies(uint32_t displayId) {
    std::vector<std::pair<uint32_t, uint32_t>> asked;
    if (!this->m_displayTablesLoaded || !this->m_npcTablesLoaded || !this->m_hasDisplays || !this->m_hasNpcTables) {
        return asked;
    }
    const RetailDisplay* display = this->m_displays.Display(displayId);
    const NpcExtra* extra = display && display->extendedDisplayId ? this->m_npcTables.Extra(display->extendedDisplayId) : nullptr;
    if (!extra) {
        return asked;
    }
    const RetailCreatureModel* model = this->m_displays.Model(display->modelId);
    NpcPlan plan;
    std::string reason;
    PlanNpc(*extra, { display->modelId, model ? model->fileDataId : 0 }, [&asked](uint32_t race, uint32_t sex) {
        asked.emplace_back(race, sex);
        return true;
    }, plan, reason);
    return asked;
}

void CascSource::PrefetchLoop() {
    std::unique_lock<std::mutex> lock(this->m_prefetchMutex);
    while (!this->m_prefetchStop) {
        PrefetchSchedule::Due due = this->m_prefetchSchedule.Next(NowMs());
        if (due.kind == PrefetchSchedule::Due::Kind::Idle) {
            this->m_prefetchWake.wait(lock);
        } else if (due.kind == PrefetchSchedule::Due::Kind::Wait) {
            this->m_prefetchWake.wait_for(lock, std::chrono::milliseconds(due.waitMs));
        } else {
            lock.unlock();
            this->PrefetchDisplay(due.key, due.generation);
            lock.lock();
        }
    }
}

bool CascSource::PrefetchCurrent(uint64_t generation) {
    std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
    return !this->m_prefetchStop && this->m_prefetchSchedule.IsCurrent(generation);
}

void CascSource::StopPrefetch() {
    {
        std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
        this->m_prefetchStop = true;
        this->m_prefetchSchedule.Cancel();
    }
    this->m_prefetchWake.notify_all();
    if (this->m_prefetchWorker.joinable()) {
        this->m_prefetchWorker.join();
    }
}

void CascSource::PrefetchDisplay(uint32_t displayId, uint64_t generation) {
    const RetailDisplays* displays = this->Displays();
    const RetailDisplay* display = displays ? displays->Display(displayId) : nullptr;
    if (!display || !this->PrefetchCurrent(generation)) {
        return;
    }
    if (display->extendedDisplayId) {
        this->PrefetchNpc(*display, generation);
        return;
    }
    const RetailCreatureModel* model = displays->Model(display->modelId);
    if (model) {
        this->PrefetchModel({ model->fileDataId, ConvertedCachePath(this->m_config, model->fileDataId), false });
    }
    for (uint32_t texture : display->variations) {
        if (texture && this->PrefetchCurrent(generation)) {
            this->PrefetchFile(texture);
        }
    }
}

// What ServeRetailNpcDisplay will plan, step by step while the request is current. A body the main thread did not
// read stops it: the plan would guess.
void CascSource::PrefetchNpc(const RetailDisplay& display, uint64_t generation) {
    const RetailNpcTables* tables = this->NpcTables();
    const NpcExtra* extra = tables ? tables->Extra(display.extendedDisplayId) : nullptr;
    const RetailCreatureModel* model = this->Displays()->Model(display.modelId);
    Retail::NpcBody body = { display.modelId, model ? model->fileDataId : 0 };
    bool unknown = false;
    NpcPlan plan;
    std::string reason;
    if (!extra || !PlanNpc(*extra, body, [this, &unknown](uint32_t race, uint32_t sex) { return this->PrefetchBodyFits(race, sex, unknown); }, plan, reason)
        || unknown || !this->PrefetchCurrent(generation)) {
        return;
    }

    if (plan.bake == BakeUse::Warp) {
        WarpedBakeRequest request = { plan.bakeFile, plan.race, extra->sex };
        std::vector<uint8_t> bytes;
        std::string error;
        if (!HasConvertedCache(ConvertedBakePath(this->m_config, request.fileDataId, request.race, request.gender), ConverterKey())) {
            this->ReadWarpedBake(request, bytes, error);
        }
    } else {
        this->PrefetchFile(plan.bakeFile);
    }
    if (plan.retailBody && this->PrefetchCurrent(generation)) {
        this->PrefetchModel({ body.fileDataId, ConvertedNpcBodyPath(this->m_config, body.fileDataId), true });
        this->ModelGeosets(body.fileDataId);
    }
    this->PrefetchItems(*extra, plan.race, generation);
}

// The worn items' models (the helm as its race's file) and textures.
void CascSource::PrefetchItems(const NpcExtra& extra, uint32_t race, uint64_t generation) {
    const RetailItemDisplays* items = this->ItemDisplays();
    for (size_t slot = 0; items && slot < NPC_ITEM_SLOTS; slot++) {
        const RetailItemDisplay* item = extra.items[slot] ? items->Display(extra.items[slot]) : nullptr;
        if (!item || !this->PrefetchCurrent(generation)) {
            continue;
        }
        for (size_t side = 0; side < item->models.size(); side++) {
            uint32_t file = item->models[side];
            uint32_t raceFile = file && slot == 0 ? items->RaceFile(file, race, extra.sex) : 0;
            file = raceFile ? raceFile : file;
            if (file) {
                this->PrefetchModel({ file, ConvertedCachePath(this->m_config, file), false });
            }
            if (item->skins[side]) {
                this->PrefetchFile(item->skins[side]);
            }
        }
        for (const BodyTexture& texture : item->body) {
            this->PrefetchFile(extra.sex ? texture.female : texture.male);
        }
    }
}

// Matched now with the body the main thread read, or already matched; unknown without either.
bool CascSource::PrefetchBodyFits(uint32_t race, uint32_t sex, bool& unknown) {
    BodySlot& slot = this->BodySlotFor(race, sex);
    if (slot.done) {
        return slot.body.fits;
    }
    StockBody stock;
    {
        std::lock_guard<std::mutex> lock(this->m_prefetchMutex);
        auto found = this->m_prefetchStock.find(race * 2 + sex);
        if (found == this->m_prefetchStock.end()) {
            unknown = true;
            return false;
        }
        stock = std::move(found->second);
        this->m_prefetchStock.erase(found);
    }
    return this->BodyFrom(race, sex, stock).fits;
}

// Converted into the converted cache unless it holds it; a file that is no retail model is only read.
void CascSource::PrefetchModel(const Conversion& conversion) {
    if (HasConvertedCache(conversion.cachePath, ConverterKey())) {
        return;
    }
    std::vector<uint8_t> file;
    std::string error;
    if (!this->ReadOriginal(conversion.fileDataId, file, error) || file.size() < 4 || std::memcmp(file.data(), "MD21", 4) != 0) {
        return;
    }
    ConvertedModel model;
    this->Convert(conversion, file, model, error);
}

void CascSource::PrefetchFile(uint32_t fileDataId) {
    DecodedCacheHead head;
    if (!fileDataId || PeekDecodedCache(DecodedCachePath(this->m_config, fileDataId), head)) {
        return;
    }
    std::vector<uint8_t> bytes;
    std::string error;
    this->ReadOriginal(fileDataId, bytes, error);
}

}
