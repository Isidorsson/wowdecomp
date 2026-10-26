#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/BakeBlp.h"
#include "Engine/Source/Retail/BodyMatch.h"
#include "Engine/Source/Retail/BodyMatchCache.h"
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/HelmFit.h"
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailBodyLook.h"
#include "Engine/Source/Retail/RetailModelData.h"

// The retail NPC half of CascSource: the NPC and race tables, the installed bodies matched to the retail HD ones, the
// HD bakes warped onto them (RetailNpcDisplay.h) and the geosets of the retail bodies (RetailBodyLook.h).

namespace Retail {

// The NPC tables, read once. Their still-locked rows (encrypted for unreleased content) are skipped, one warning a table.
bool CascSource::LoadNpcTables(std::string& error) {
    Db2Table extra, itemSlots, options, choices, elements, geosets, materials, textureFiles;
    const std::vector<std::pair<uint32_t, Db2Table*>> tables = { { CREATURE_DISPLAY_INFO_EXTRA_FDID, &extra },
        { NPC_MODEL_ITEM_SLOT_DISPLAY_INFO_FDID, &itemSlots }, { CREATURE_DISPLAY_INFO_OPTION_FDID, &options }, { CHR_CUSTOMIZATION_CHOICE_FDID, &choices },
        { CHR_CUSTOMIZATION_ELEMENT_FDID, &elements }, { CHR_CUSTOMIZATION_GEOSET_FDID, &geosets }, { CHR_CUSTOMIZATION_MATERIAL_FDID, &materials },
        { TEXTURE_FILE_DATA_FDID, &textureFiles } };
    if (!this->ReadTables(tables, error)) {
        return false;
    }
    for (const auto& [fileDataId, table] : tables) {
        if (table->SkippedRows()) {
            this->Warn("table " + std::to_string(fileDataId) + ": skipped " + std::to_string(table->SkippedRows()) + " still-locked rows");
        }
    }
    this->m_hasNpcTables = this->m_npcTables.Load({ extra, itemSlots, options, choices, elements, geosets, materials, textureFiles }, error);
    return this->m_hasNpcTables;
}

const RetailNpcTables* CascSource::NpcTables() {
    if (!this->m_configured) {
        return nullptr;
    }
    std::call_once(this->m_npcTablesRead, [this] {
        std::string error;
        if (!this->LoadNpcTables(error)) {
            this->Warn("NPC tables: " + error);
        }
        this->m_npcTablesLoaded = true;
    });
    return this->m_hasNpcTables ? &this->m_npcTables : nullptr;
}

bool CascSource::LoadRaceTables(std::string& error) {
    Db2Table races, raceModels, models, layers;
    if (!this->ReadTables({ { CHR_RACES_FDID, &races }, { CHR_RACE_X_CHR_MODEL_FDID, &raceModels }, { CHR_MODEL_FDID, &models },
            { CHR_MODEL_TEXTURE_LAYER_FDID, &layers } }, error)) {
        return false;
    }
    this->m_hasRaces = this->m_races.Load({ races, raceModels, models, layers }, error);
    return this->m_hasRaces;
}

const RetailRaces* CascSource::Races() {
    if (!this->m_configured) {
        return nullptr;
    }
    std::call_once(this->m_raceTablesRead, [this] {
        std::string error;
        if (!this->LoadRaceTables(error)) {
            this->Warn("race tables: " + error);
        }
    });
    return this->m_hasRaces ? &this->m_races : nullptr;
}

// A stock race by its prefix, a served one by the retail ChrRaces row of that ClientPrefix.
bool CascSource::ResolveRaceFile(const RaceFileRequest& request, uint32_t& fileDataId, std::string& error) {
    const RetailRaces* races = request.race ? nullptr : this->Races();
    uint32_t race = request.race ? request.race : (races ? races->RaceOfPrefix(request.prefix) : 0);
    if (!race) {
        error = "no race has the prefix " + request.prefix;
        return false;
    }
    const RetailItemDisplays* displays = this->ItemDisplays();
    fileDataId = displays ? displays->RaceFile(request.fileDataId, race, request.gender) : 0;
    if (!fileDataId) {
        error = displays ? "model " + std::to_string(request.fileDataId) + " has no file for race " + std::to_string(race) + (request.gender ? " female" : " male")
                         : "no item tables to choose the race file from";
    }
    return fileDataId != 0;
}

const std::vector<uint32_t>& CascSource::ModelGeosets(uint32_t fileDataId) {
    std::lock_guard<std::mutex> lock(this->m_geosetMutex);
    auto [found, added] = this->m_modelGeosets.try_emplace(fileDataId);
    if (!added) {
        return found->second;
    }
    std::vector<uint8_t> model;
    std::vector<uint8_t> skin;
    std::string error;
    std::vector<uint32_t> skins;
    bool read = this->ReadOriginal(fileDataId, model, error);
    if (read) {
        skins = U32s(ReadChunks(model)["SFID"]);
        read = !skins.empty() && this->ReadOriginal(skins[0], skin, error) && SkinGeosetIds(skin, found->second, error);
    }
    if (!read) {
        this->Warn("retail body " + std::to_string(fileDataId) + ": no geosets, " + (error.empty() ? std::string("no skin") : error));
    }
    return found->second;
}

// An NPC body's own entry, apart from the plain model's (which refuses a player race model).
CascSource::Conversion CascSource::ConversionFor(const std::string& normalized, uint32_t fileDataId) const {
    uint32_t bodyId = 0;
    bool playerBody = ParseNpcBodyPath(normalized, bodyId);
    return { fileDataId, playerBody ? ConvertedNpcBodyPath(this->m_config, fileDataId) : ConvertedCachePath(this->m_config, fileDataId), playerBody };
}

bool CascSource::InstalledBodyFits(uint32_t race, uint32_t sex) {
    return this->BodyFor(race, sex).fits;
}

CascSource::BodySlot& CascSource::BodySlotFor(uint32_t race, uint32_t sex) {
    std::lock_guard<std::mutex> lock(this->m_bodyMutex);
    std::unique_ptr<BodySlot>& slot = this->m_bodies[race * 2 + sex];
    if (!slot) {
        slot = std::make_unique<BodySlot>();
    }
    return *slot;
}

const CascSource::NpcBody& CascSource::BodyFor(uint32_t race, uint32_t sex) {
    BodySlot& slot = this->BodySlotFor(race, sex);
    std::call_once(slot.matched, [&] {
        slot.body = this->MatchBody(race, sex, this->ReadStockBody(race, sex));
        slot.done = true;
    });
    return slot.body;
}

const CascSource::NpcBody& CascSource::BodyFrom(uint32_t race, uint32_t sex, const StockBody& stock) {
    BodySlot& slot = this->BodySlotFor(race, sex);
    std::call_once(slot.matched, [&] {
        slot.body = this->MatchBody(race, sex, stock);
        slot.done = true;
    });
    return slot.body;
}

// The installed body and its view-0 skin from the client's archives, and its base skin for the eyes of warped bakes.
CascSource::StockBody CascSource::ReadStockBody(uint32_t race, uint32_t sex) {
    StockBody stock;
    std::string path = InstalledBodyPath(race, sex);
    stock.read = !path.empty() && this->m_readStock && this->m_readStock(path, stock.model) && this->m_readStock(InstalledBodySkinPath(race, sex), stock.skin);
    if (stock.read && !this->m_readStock(InstalledBaseSkinPath(race, sex), stock.base)) {
        stock.base.clear();
    }
    return stock;
}

// The retail body with every skin's geosets (view 0 first, then the LOD skins the ports were made from). A skin that
// does not read only leaves its vertices' geosets unknown, so their seam copies stay candidates.
bool CascSource::ReadRetailBody(uint32_t fileDataId, BodyMesh& mesh, std::string& error) {
    std::vector<uint8_t> model;
    if (!this->ReadOriginal(fileDataId, model, error) || !ReadBodyMesh(model, nullptr, mesh, error)) {
        return false;
    }
    for (uint32_t skinId : U32s(ReadChunks(model)["SFID"])) {
        std::vector<uint8_t> skin;
        std::string skinError;
        if (!this->ReadOriginal(skinId, skin, skinError) || !AddSkinGeosets(model, skin, mesh, skinError)) {
            this->Warn("retail body " + std::to_string(fileDataId) + ": skin " + std::to_string(skinId) + " gives no geosets, " + skinError);
        }
    }
    return true;
}

// Without it the triangles retail draws with other textures (the eyes) keep the background.
void CascSource::DecodeBaseSkin(uint32_t race, uint32_t sex, const StockBody& stock, NpcBody& body) {
    std::string error = "unreadable";
    body.hasBase = !stock.base.empty() && DecodeBakeBlp(stock.base, body.base, error);
    if (!body.hasBase) {
        this->Warn(InstalledBaseSkinPath(race, sex) + ": no installed skin for the eyes of warped bakes, " + error);
    }
}

// The match kept from an earlier session when it is of this installed body and converter; else matched and kept.
CascSource::NpcBody CascSource::MatchBody(uint32_t race, uint32_t sex, const StockBody& stock) {
    NpcBody body;
    if (!stock.read) {
        this->Warn("race " + std::to_string(race) + " sex " + std::to_string(sex) + ": no installed body to warp HD bakes onto");
        return body;
    }
    std::filesystem::path kept = ConvertedBodyMatchPath(this->m_config, race, sex);
    uint64_t key = BodyMatchKey(ConverterKey(), stock.model, stock.skin, RetailHdBodyFileDataId(race, sex));
    BodyMatchResult match;
    if (!ReadBodyMatchCache(kept, key, match)) {
        if (!this->MatchStockBody(race, sex, stock, match)) {
            return body;
        }
        // A failed write only costs a match next session.
        std::string cacheError;
        std::error_code ec;
        uintmax_t size = WriteBodyMatchCache(kept, key, match, cacheError) ? std::filesystem::file_size(kept, ec) : 0;
        if (size && !ec) {
            this->m_convertedBudget.Added(size);
        }
    }
    body.fits = match.fits;
    body.warp = std::move(match.warp);
    if (body.fits) {
        this->DecodeBaseSkin(race, sex, stock, body);
    }
    return body;
}

// The installed body's vertices matched to the retail HD body's, from the storage.
bool CascSource::MatchStockBody(uint32_t race, uint32_t sex, const StockBody& stock, BodyMatchResult& match) {
    std::string path = InstalledBodyPath(race, sex);
    uint32_t retailId = RetailHdBodyFileDataId(race, sex);
    BodyMesh installedMesh;
    BodyMesh retailMesh;
    std::string error;
    if (!ReadBodyMesh(stock.model, &stock.skin, installedMesh, error) || !this->ReadRetailBody(retailId, retailMesh, error)) {
        this->Warn(path + ": cannot match to the retail HD body, " + error);
        return false;
    }

    ShrinkBody(retailMesh, HelmFitFor(race, sex)->scale);
    VertexMatches matches = MatchVertices(installedMesh, retailMesh);
    match.ratio = BodyMatchRatio(installedMesh, matches);
    match.fits = match.ratio >= BODY_MATCH_THRESHOLD;
    if (match.fits) {
        match.warp = BuildBakeWarp(installedMesh, retailMesh, matches, PlayerBodyRegions);
    }
    this->Warn(path + ": " + std::to_string(int(match.ratio * 1000) / 10.0).substr(0, 4) + "% of its skin vertices lie on retail " + std::to_string(retailId)
        + (match.fits ? ", so HD bakes are warped onto it (" + std::to_string(match.warp.triangles.size()) + " triangles)" : ", not the HD port: HD bakes are refused"));
    return true;
}

// From the converted cache when it holds it; else read, warped and kept there.
bool CascSource::ReadWarpedBake(const WarpedBakeRequest& request, std::vector<uint8_t>& bytes, std::string& error) {
    if (!this->m_configured) {
        error = this->m_configError.empty() ? std::string("retail assets are not configured") : this->m_configError;
        return false;
    }
    std::filesystem::path path = ConvertedBakePath(this->m_config, request.fileDataId, request.race, request.gender);
    ConvertedModel cached;
    if (ReadConvertedCache(path, ConverterKey(), cached)) {
        bytes = std::move(cached.m2);
        return true;
    }

    const NpcBody& body = this->BodyFor(request.race, request.gender);
    if (!body.fits) {
        error = "the installed body of race " + std::to_string(request.race) + " sex " + std::to_string(request.gender) + " takes no warped bake";
        return false;
    }
    std::vector<uint8_t> original;
    Image bake;
    if (!this->ReadOriginal(request.fileDataId, original, error) || !DecodeBakeBlp(original, bake, error)) {
        return false;
    }

    ConvertedModel warped;
    warped.m2 = EncodeRawBlp(ApplyBakeWarp(body.warp, bake, body.hasBase ? &body.base : nullptr));
    std::string cacheError;
    std::error_code ec;
    uintmax_t size = WriteConvertedCache(path, ConverterKey(), warped, cacheError) ? std::filesystem::file_size(path, ec) : 0;
    if (size && !ec) {
        this->m_convertedBudget.Added(size);
    }
    bytes = std::move(warped.m2);
    return true;
}

}
