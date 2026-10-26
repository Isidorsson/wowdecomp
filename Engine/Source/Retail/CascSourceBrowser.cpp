#include <chrono>
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/ItemTables.h"

// The retail browser half of CascSource: its search index, built once on a worker from the browser tables and the
// display loaders, so the UI never waits on a table read (Interface\AddOns\RetailBrowser polls RetailBrowserInfo).

namespace Retail {

BrowserIndexView CascSource::BrowserIndex() {
    if (!this->m_configured) {
        return {};
    }
    std::call_once(this->m_browserStarted, [this] {
        this->m_browserState = IndexState::Building;
        this->m_browserWorker = std::thread([this] {
            auto start = std::chrono::steady_clock::now();
            std::string error;
            if (!this->LoadBrowserIndex(error)) {
                this->m_browserError = error;
                this->Warn("retail browser: " + error);
                this->m_browserState = IndexState::Failed;
                return;
            }
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            this->Warn("retail browser: indexed " + std::to_string(this->m_browserIndex.Count(BrowseKind::Creature)) + " creatures, " +
                std::to_string(this->m_browserIndex.Count(BrowseKind::Item)) + " items, " + std::to_string(this->m_browserIndex.Count(BrowseKind::Set)) +
                " sets, " + std::to_string(this->m_browserIndex.Count(BrowseKind::Model)) + " models and " +
                std::to_string(this->m_browserIndex.Count(BrowseKind::Mount)) + " mounts in " + std::to_string(ms) + " ms");
            this->m_browserState = IndexState::Ready;
        });
    });
    IndexState state = this->m_browserState;
    return { state, state == IndexState::Ready ? &this->m_browserIndex : nullptr, state == IndexState::Failed ? this->m_browserError : std::string() };
}

// What the display loaders know of a display; a loader that failed knows nothing.
BrowserLookups CascSource::BrowserDisplayLookups() {
    const RetailDisplays* displays = this->Displays();
    const RetailItemDisplays* items = this->ItemDisplays();
    const RetailNpcTables* npc = this->NpcTables();
    auto creature = [displays, npc](uint32_t id, CreatureDisplayFacts& facts) {
        const RetailDisplay* display = displays ? displays->Display(id) : nullptr;
        if (!display) {
            return false;
        }
        const RetailCreatureModel* model = displays->Model(display->modelId);
        facts.modelId = display->modelId;
        facts.modelFdid = model ? model->fileDataId : 0;
        facts.scale = display->scale;
        facts.extraId = display->extendedDisplayId;
        const NpcExtra* extra = facts.extraId && npc ? npc->Extra(facts.extraId) : nullptr;
        if (extra) {
            facts.race = extra->race;
            facts.sex = extra->sex;
            facts.sdBake = extra->sdBake;
            facts.hdBake = extra->hdBake;
        }
        return true;
    };
    auto item = [items](uint32_t id, ItemDisplayFacts& facts) {
        const RetailItemDisplay* display = items ? items->Display(id) : nullptr;
        if (!display) {
            return false;
        }
        facts.models = display->models;
        facts.icon = display->icon;
        return true;
    };
    return { creature, item };
}

bool CascSource::LoadBrowserIndex(std::string& error) {
    Db2Table creatures, items, searchNames, modifiedAppearances, appearances, transmogSets, transmogSetItems, itemSets, mounts, mountDisplays,
        creatureTypes, creatureFamilies, itemSubClasses;
    std::vector<std::pair<uint32_t, Db2Table*>> tables = { { CREATURE_FDID, &creatures }, { ITEM_FDID, &items },
        { ITEM_SEARCH_NAME_FDID, &searchNames }, { ITEM_MODIFIED_APPEARANCE_FDID, &modifiedAppearances },
        { TRANSMOG_SET_FDID, &transmogSets }, { TRANSMOG_SET_ITEM_FDID, &transmogSetItems }, { ITEM_SET_FDID, &itemSets }, { MOUNT_FDID, &mounts },
        { MOUNT_X_DISPLAY_FDID, &mountDisplays }, { CREATURE_TYPE_FDID, &creatureTypes }, { CREATURE_FAMILY_FDID, &creatureFamilies },
        { ITEM_SUB_CLASS_FDID, &itemSubClasses } };
    // ItemAppearance is parsed once, by the item tables, unless they failed before it.
    BrowserLookups lookups = this->BrowserDisplayLookups();
    bool sharedAppearances = this->m_hasItemAppearances;
    if (!sharedAppearances) {
        tables.push_back({ ITEM_APPEARANCE_FDID, &appearances });
    }
    if (!this->ReadTables(tables, error)) {
        return false;
    }
    for (const auto& [fileDataId, table] : tables) {
        if (table->SkippedRows()) {
            this->Warn("table " + std::to_string(fileDataId) + ": skipped " + std::to_string(table->SkippedRows()) + " still-locked rows");
        }
    }
    BrowserTables browser = { creatures, items, searchNames, modifiedAppearances, sharedAppearances ? this->m_itemAppearances : appearances,
        transmogSets, transmogSetItems, itemSets, mounts, mountDisplays, creatureTypes, creatureFamilies, itemSubClasses };
    if (!this->m_browserIndex.Build(browser, lookups, error)) {
        return false;
    }
    std::string statsError;
    if (!this->LoadBrowserStats(statsError)) {
        this->Warn("retail browser: item stats: " + statsError);
    }
    return true;
}

// ItemSparse and the stat budget tables: without them the browser still searches, its items just have no stats.
bool CascSource::LoadBrowserStats(std::string& error) {
    std::vector<uint8_t> bytes;
    Db2Table sparse;
    if (!this->ReadTable(ITEM_SPARSE_FDID, bytes, error) || !sparse.Parse(std::move(bytes), error, ItemSparseStrings()) ||
        !this->m_browserIndex.AttachSparse(sparse, error)) {
        return false;
    }
    if (sparse.SkippedRows()) {
        this->Warn("table " + std::to_string(ITEM_SPARSE_FDID) + ": skipped " + std::to_string(sparse.SkippedRows()) + " still-locked rows");
    }
    Db2Table randProp;
    StatBudget budget;
    if (!this->ReadTables({ { RAND_PROP_POINTS_FDID, &randProp } }, error) || !ReadRandPropPoints(randProp, budget.randProp, error)) {
        return false;
    }
    const std::pair<uint32_t, GameTable*> gameTables[] = { { COMBAT_RATINGS_MULT_FDID, &budget.combatRatings }, { STAMINA_MULT_FDID, &budget.stamina },
        { SOCKET_COST_FDID, &budget.socketCost } };
    for (const auto& [fileDataId, table] : gameTables) {
        std::vector<uint8_t> text;
        if (!this->ReadTable(fileDataId, text, error) || !ParseGameTable(std::string(text.begin(), text.end()), *table, error)) {
            error = "game table " + std::to_string(fileDataId) + ": " + error;
            return false;
        }
    }
    std::string armourError;
    if (!this->LoadArmourTables(budget.armour, armourError)) {
        this->Warn("retail browser: armour and damage: " + armourError);
    }
    this->m_browserIndex.SetBudget(std::move(budget));
    return true;
}

// ItemArmorQuality, ItemArmorTotal, ArmorLocation, ItemArmorShield and the ItemDamage tables: without them the Build
// panel keeps the base item's armour and damage.
bool CascSource::LoadArmourTables(ArmourTables& tables, std::string& error) {
    Db2Table quality, total, location, shield;
    std::array<Db2Table, DAMAGE_TABLES> damage;
    std::vector<std::pair<uint32_t, Db2Table*>> files = { { ITEM_ARMOR_QUALITY_FDID, &quality }, { ITEM_ARMOR_TOTAL_FDID, &total },
        { ARMOR_LOCATION_FDID, &location }, { ITEM_ARMOR_SHIELD_FDID, &shield } };
    std::array<const Db2Table*, DAMAGE_TABLES> damageTables = {};
    for (size_t t = 0; t < DAMAGE_TABLES; t++) {
        files.push_back({ ITEM_DAMAGE_FDIDS[t], &damage[t] });
        damageTables[t] = &damage[t];
    }
    return this->ReadTables(files, error) && ReadArmourTables({ quality, total, location, shield, damageTables }, tables, error);
}

}
