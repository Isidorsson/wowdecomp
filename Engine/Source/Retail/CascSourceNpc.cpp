#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/BakeBlp.h"
#include "Engine/Source/Retail/BodyMatch.h"
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/HelmFit.h"
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailModelData.h"

// The retail NPC half of CascSource: the NPC tables, the installed bodies matched to the retail HD ones, and the HD
// bakes warped onto them (RetailNpcDisplay.h).

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
    });
    return this->m_hasNpcTables ? &this->m_npcTables : nullptr;
}

bool CascSource::InstalledBodyFits(uint32_t race, uint32_t sex) {
    return this->BodyFor(race, sex).fits;
}

const CascSource::NpcBody& CascSource::BodyFor(uint32_t race, uint32_t sex) {
    std::lock_guard<std::mutex> lock(this->m_bodyMutex);
    uint32_t key = race * 2 + sex;
    auto found = this->m_bodies.find(key);
    if (found == this->m_bodies.end()) {
        found = this->m_bodies.emplace(key, this->MatchBody(race, sex)).first;
    }
    return found->second;
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
void CascSource::ReadBaseSkin(uint32_t race, uint32_t sex, NpcBody& body) {
    std::string path = InstalledBaseSkinPath(race, sex);
    std::vector<uint8_t> bytes;
    std::string error = "unreadable";
    body.hasBase = this->m_readStock(path, bytes) && DecodeBakeBlp(bytes, body.base, error);
    if (!body.hasBase) {
        this->Warn(path + ": no installed skin for the eyes of warped bakes, " + error);
    }
}

// The installed body and its view-0 skin from the client's archives, the retail HD body from the storage.
CascSource::NpcBody CascSource::MatchBody(uint32_t race, uint32_t sex) {
    NpcBody body;
    std::string path = InstalledBodyPath(race, sex);
    uint32_t retailId = RetailHdBodyFileDataId(race, sex);
    std::vector<uint8_t> stock;
    std::vector<uint8_t> skin;
    BodyMesh installedMesh;
    BodyMesh retailMesh;
    std::string error;
    if (path.empty() || !this->m_readStock || !this->m_readStock(path, stock) || !this->m_readStock(InstalledBodySkinPath(race, sex), skin)) {
        this->Warn("race " + std::to_string(race) + " sex " + std::to_string(sex) + ": no installed body to warp HD bakes onto");
        return body;
    }
    if (!ReadBodyMesh(stock, &skin, installedMesh, error) || !this->ReadRetailBody(retailId, retailMesh, error)) {
        this->Warn(path + ": cannot match to the retail HD body, " + error);
        return body;
    }

    ShrinkBody(retailMesh, HelmFitFor(race, sex)->scale);
    VertexMatches matches = MatchVertices(installedMesh, retailMesh);
    double ratio = BodyMatchRatio(installedMesh, matches);
    body.fits = ratio >= BODY_MATCH_THRESHOLD;
    if (body.fits) {
        body.warp = BuildBakeWarp(installedMesh, retailMesh, matches, PlayerBodyRegions);
        this->ReadBaseSkin(race, sex, body);
    }
    this->Warn(path + ": " + std::to_string(int(ratio * 1000) / 10.0).substr(0, 4) + "% of its skin vertices lie on retail " + std::to_string(retailId)
        + (body.fits ? ", so HD bakes are warped onto it (" + std::to_string(body.warp.triangles.size()) + " triangles)" : ", not the HD port: HD bakes are refused"));
    return body;
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
