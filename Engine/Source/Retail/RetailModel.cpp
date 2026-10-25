#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RetailAnimation.h"
#include "Engine/Source/Retail/RetailBoneWindows.h"
#include "Engine/Source/Retail/RetailModelData.h"
#include "Engine/Source/Retail/RetailRecords.h"
#include "Engine/Source/Retail/RetailShading.h"
#include "Engine/Source/Retail/RetailSkeleton.h"
#include "Engine/Source/Retail/RetailSkinLayout.h"
#include "Engine/Source/Retail/RetailViews.h"
#include <algorithm>
#include <cstring>
#include <map>

namespace Retail {

constexpr uint32_t FIRST_VERSION = 272;
constexpr uint32_t LAST_VERSION = 274;
constexpr uint32_t LEGACY_VERSION = 264;
constexpr uint32_t LEGACY_MAX_TEXTURE_TYPE = 14;
// The bits the stock 3.3.5 models carry (wow-toolbox-editor downport/limits.rs, measured on 22,112 models).
constexpr uint32_t GLOBAL_FLAGS_MASK = 0x1F;
constexpr uint32_t BONE_FLAGS_MASK = 0x27F;
constexpr uint16_t RENDER_FLAGS_MASK = 0x1DF;
constexpr uint16_t BLEND_BLEND_ADD = 7;
constexpr uint16_t BLEND_ADD = 4;
constexpr uint8_t BATCH_HIDDEN_IN_335 = 0x80;
constexpr uint16_t CLIENT_MAX_TEXTURES = 2;
constexpr uint16_t EFFECT_FLAG = 0x8000;
// Character skin, hair and extra skin, and the Shadowlands layers from 19: a model on a skeleton with one is a player
// race model, built from character customization tables 3.3.5 has no equivalent for (downport/read/skeletal.rs).
constexpr uint32_t CHARACTER_TEXTURE_TYPES[] = { 1, 6, 8 };
constexpr uint32_t FIRST_CUSTOMIZATION_LAYER_TYPE = 19;
constexpr size_t ATTACHMENT_RECORD_SIZE = 0x28;

static bool MaskBonesAndMaterials(std::vector<uint8_t>& m2, std::string& error) {
    Span bones;
    Span materials;
    if (!ReadSpan(m2, M2At::BONES, BONE_SIZE, "bones", bones, error) || !ReadSpan(m2, M2At::MATERIALS, MATERIAL_SIZE, "materials", materials, error)) {
        return false;
    }

    for (uint32_t i = 0; i < bones.count; i++) {
        size_t at = bones.At(i, BONE_SIZE) + 4;
        Put<uint32_t>(m2, at, Get<uint32_t>(m2, at) & BONE_FLAGS_MASK);
    }

    for (uint32_t i = 0; i < materials.count; i++) {
        size_t at = materials.At(i, MATERIAL_SIZE);
        Put<uint16_t>(m2, at, Get<uint16_t>(m2, at) & RENDER_FLAGS_MASK);

        if (Get<uint16_t>(m2, at + 2) == BLEND_BLEND_ADD) {
            Put<uint16_t>(m2, at + 2, BLEND_ADD);
        }
    }

    return true;
}

// Points the texture record at to retail/<fdid>.blp, appended.
static void NameTexture(std::vector<uint8_t>& m2, size_t at, uint32_t fileDataId) {
    std::string name = "retail/" + std::to_string(fileDataId) + ".blp";
    m2.resize((m2.size() + 15) & ~size_t(15), 0);
    uint32_t offset = static_cast<uint32_t>(m2.size());
    m2.insert(m2.end(), name.begin(), name.end());
    m2.push_back(0);
    Put<uint32_t>(m2, at + 8, static_cast<uint32_t>(name.size() + 1));
    Put<uint32_t>(m2, at + 12, offset);
}

// Type-0 textures named by TXID get retail/<fdid>.blp, appended.
static bool ConvertTextures(std::vector<uint8_t>& m2, const std::vector<uint32_t>& ids, std::string& error) {
    Span textures;
    if (!ReadSpan(m2, M2At::TEXTURES, TEXTURE_SIZE, "textures", textures, error)) {
        return false;
    }

    for (uint32_t i = 0; i < textures.count && i < ids.size(); i++) {
        size_t at = textures.At(i, TEXTURE_SIZE);
        if (Get<uint32_t>(m2, at) == 0 && ids[i] != 0) {
            NameTexture(m2, at, ids[i]);
        }
    }

    return true;
}

// Types 3.3.5 lacks become file textures: named ones keep their file, which no 3.3.5 display replaces, and
// unnamed ones are blank. Runs after the skin is named, which needs the retail types.
static bool MaskTextureTypes(std::vector<uint8_t>& m2, std::string& error) {
    Span textures;
    if (!ReadSpan(m2, M2At::TEXTURES, TEXTURE_SIZE, "textures", textures, error)) {
        return false;
    }

    for (uint32_t i = 0; i < textures.count; i++) {
        size_t at = textures.At(i, TEXTURE_SIZE);
        if (Get<uint32_t>(m2, at) <= LEGACY_MAX_TEXTURE_TYPE) {
            continue;
        }

        Put<uint32_t>(m2, at, 0);
        if (Get<uint32_t>(m2, at + 8) <= 1) {
            Put<uint32_t>(m2, at + 8, 0);
            Put<uint32_t>(m2, at + 12, 0);
        }
    }

    return true;
}

// A model naming a skeleton keeps no sequences, bones or attachments itself; one that does is refused, as which copy is
// meant is not known. So is a player race model.
static bool CheckSkeletalModel(const std::vector<uint8_t>& m2, std::string& error) {
    Span sequences;
    Span bones;
    Span attachments;
    Span textures;
    if (!ReadSpan(m2, M2At::SEQUENCES, SEQUENCE_SIZE, "sequences", sequences, error) || !ReadSpan(m2, M2At::BONES, BONE_SIZE, "bones", bones, error)
        || !ReadSpan(m2, M2At::ATTACHMENTS, ATTACHMENT_RECORD_SIZE, "attachments", attachments, error) || !ReadSpan(m2, M2At::TEXTURES, TEXTURE_SIZE, "textures", textures, error)) {
        return false;
    }

    std::string filled;
    for (const auto& block : { std::make_pair(sequences.count, "sequences"), std::make_pair(bones.count, "bones"), std::make_pair(attachments.count, "attachments") }) {
        if (block.first) {
            filled += (filled.empty() ? "" : ", ") + std::to_string(block.first) + " " + block.second;
        }
    }

    if (!filled.empty()) {
        return Fail(error, "the model names a skeleton file (SKID) and also stores " + filled + " itself, so which copy is meant is not known");
    }

    for (uint32_t i = 0; i < textures.count; i++) {
        uint32_t type = Get<uint32_t>(m2, textures.At(i, TEXTURE_SIZE));
        if (std::count(std::begin(CHARACTER_TEXTURE_TYPES), std::end(CHARACTER_TEXTURE_TYPES), type) || type >= FIRST_CUSTOMIZATION_LAYER_TYPE) {
            return Fail(error, "the model has a character texture (type " + std::to_string(type) + ") and a skeleton file, so it is a player race model, "
                "assembled from character customization tables 3.3.5 has no equivalent for");
        }
    }

    return true;
}

// The sequences, bones and attachments of the skeleton the model names, merged with its parents.
static bool TakeSkeleton(const std::vector<uint8_t>& skid, std::vector<uint8_t>& m2, const FileReader& read, ConvertedModel& converted, std::string& error) {
    if (skid.size() < 4) {
        return Fail(error, "the SKID chunk is truncated");
    }

    std::vector<SkeletonFile> chain;
    converted.skeleton.fileDataId = Get<uint32_t>(skid, 0);
    return CheckSkeletalModel(m2, error) && LoadSkeletonChain(converted.skeleton.fileDataId, read, chain, error)
        && MergeSkeleton(m2, chain, read, converted.skeleton, converted.warnings, error);
}

// The parts of a model every conversion needs, skins included: header checks, masks, TXID texture names and the
// shading tables. Animation and records come after it in ConvertModel only. A model conversion (converted) also
// merges the skeleton a model names; a skin needs only the geometry, which the model keeps.
static bool ConvertGeometry(const std::map<std::string, std::vector<uint8_t>>& chunks, std::vector<uint8_t>& m2, const FileReader* read, ConvertedModel* converted,
    std::string& error) {
    auto md21 = chunks.find("MD21");
    if (md21 == chunks.end()) {
        return Fail(error, "no MD21 chunk");
    }

    m2 = md21->second;
    if (m2.size() < M2At::HEADER || std::memcmp(m2.data(), "MD20", 4) != 0) {
        return Fail(error, "the MD20 header is missing or truncated");
    }

    uint32_t version = Get<uint32_t>(m2, M2At::VERSION);
    if (version < FIRST_VERSION || version > LAST_VERSION) {
        return Fail(error, "version " + std::to_string(version) + " is not converted (272 to 274 are)");
    }

    Span vertices;
    if (!ReadSpan(m2, M2At::VERTICES, VERTEX_SIZE, "vertices", vertices, error)) {
        return false;
    }

    auto skid = chunks.find("SKID");
    if (converted && skid != chunks.end() && !TakeSkeleton(skid->second, m2, *read, *converted, error)) {
        return false;
    }

    auto txid = chunks.find("TXID");
    std::vector<uint32_t> textureIds = txid == chunks.end() ? std::vector<uint32_t>() : U32s(txid->second);
    return AppendShadingTables(m2, error) && MaskBonesAndMaterials(m2, error) && ConvertTextures(m2, textureIds, error);
}

// Unnamed slots of a type the model's default skin supplies name its file (the lowest, when it has several) and keep
// their type: the client loads a named file for any texture (M2Shared.cpp) until a display replaces it
// (CM2Model::ReplaceTexture).
static bool NameModelSkin(std::vector<uint8_t>& m2, const ModelSkin* skin, std::vector<std::string>& warnings, std::string& error) {
    Span textures;
    if (!skin || !ReadSpan(m2, M2At::TEXTURES, TEXTURE_SIZE, "textures", textures, error)) {
        return !skin;
    }

    std::string named;
    for (uint32_t i = 0; i < textures.count; i++) {
        size_t at = textures.At(i, TEXTURE_SIZE);
        uint32_t type = Get<uint32_t>(m2, at);
        uint32_t file = 0;
        for (const SkinTexture& texture : skin->textures) {
            if (texture.type == type && (!file || texture.fileDataId < file)) {
                file = texture.fileDataId;
            }
        }

        if (!file || Get<uint32_t>(m2, at + 8) > 1) {
            continue;
        }

        NameTexture(m2, at, file);
        named += (named.empty() ? "" : ", ") + std::to_string(i);
    }

    if (!named.empty()) {
        std::string shown = skin->displays == 1 ? "1 display shows" : std::to_string(skin->displays) + " displays show";
        std::string others = skin->others == 1 ? "; 1 other skin of the model shows only through its own displays"
            : skin->others ? "; " + std::to_string(skin->others) + " other skins of the model show only through their own displays" : "";
        warnings.push_back("textures " + named + " (skin slots) name the skin of display " + std::to_string(skin->displayId) + ", which "
            + shown + ", until a display replaces it" + others);
    }

    return true;
}

// Retail names particle models by FileDataID (GPID geometry, RPID recursion); the client loads names only.
static void WarnParticleModels(const std::map<std::string, std::vector<uint8_t>>& chunks, std::vector<std::string>& warnings) {
    for (const char* tag : { "GPID", "RPID" }) {
        auto chunk = chunks.find(tag);
        if (chunk == chunks.end()) {
            continue;
        }

        std::vector<uint32_t> ids = U32s(chunk->second);
        auto named = std::count_if(ids.begin(), ids.end(), [](uint32_t id) { return id != 0; });
        if (named) {
            warnings.push_back(std::to_string(named) + " particle models named by " + tag + " are not loaded");
        }
    }
}

// What the model needs from the original skin of every view it keeps (ChooseViews): the view count, the bone
// windows, the UV1 copy and the effect report. Nothing is split or copied unless every view was read: the windows of
// a view follow every earlier view's, and any view may read UV2.
static bool ConvertForViews(std::vector<uint8_t>& m2, const std::vector<uint32_t>& sfid, const FileReader& read, std::vector<uint32_t>& skins, std::vector<std::string>& warnings,
    std::string& error) {
    ViewSkins chosen;
    std::string readError;
    bool chose = ChooseViews(m2, sfid, read, chosen, readError);
    skins = chosen.fileDataIds;
    if (!chose) {
        warnings.push_back(readError + ", so no batch gets UV1 copied into UV2 and no submesh is split");
        return true;
    }

    warnings.insert(warnings.end(), chosen.warnings.begin(), chosen.warnings.end());
    if (!skins.empty()) {
        Put<uint32_t>(m2, M2At::VIEWS, static_cast<uint32_t>(skins.size()));
    }

    BoneWindowPlan plan;
    if (!PlanBoneWindows(m2, chosen.originals, plan, error)) {
        return false;
    }

    ReportBoneWindows(m2, chosen.originals, plan, warnings);
    ReportLayouts(m2, chosen.originals, plan, warnings);
    return AppendBoneWindows(m2, plan, error) && CopyFirstUvSet(m2, chosen.originals, warnings, error) && ReportEffects(m2, chosen.originals, warnings, error);
}

bool ConvertModel(const std::vector<uint8_t>& file, const ModelSources& sources, ConvertedModel& model, std::string& error) {
    auto chunks = ReadChunks(file);
    std::vector<uint8_t> m2;
    std::vector<uint32_t> compressedGravity;
    ConvertedModel converted;

    if (!ConvertGeometry(chunks, m2, &sources.read, &converted, error) || !ConvertCameras(m2, error) || !ConvertParticles(m2, compressedGravity, converted.warnings, error)) {
        return false;
    }

    const ModelSkeleton& skeleton = converted.skeleton;
    std::vector<uint8_t> afid = chunks["AFID"];
    afid.insert(afid.end(), skeleton.afid.begin(), skeleton.afid.end());

    if ((skeleton.fileDataId && !PadRuns(m2, skeleton.own, static_cast<uint32_t>(skeleton.parentIndex.size()), error))
        || !InlineAnimations(m2, afid, sources.read, skeleton, converted.warnings, error)
        || (skeleton.fileDataId && !ApplyParentEvents(m2, chunks["PEDC"], skeleton, converted.warnings, error))
        || !DecodeParticleGravity(m2, compressedGravity, error) || !EmbedAliases(m2, error) || !RemapSequences(m2, sources.fallbacks, converted.remapped, error)) {
        return false;
    }

    WarnParticleModels(chunks, converted.warnings);
    if (!NameModelSkin(m2, sources.skin, converted.warnings, error) || !MaskTextureTypes(m2, error)) {
        return false;
    }

    std::vector<uint32_t> sfid = U32s(chunks["SFID"]);
    uint32_t views = Get<uint32_t>(m2, M2At::VIEWS);

    if (!sfid.empty() && sfid.size() < views) {
        return Fail(error, "SFID lists " + std::to_string(sfid.size()) + " skins for " + std::to_string(views) + " views");
    }

    std::vector<uint32_t> skins;
    if (!ConvertForViews(m2, sfid, sources.read, skins, converted.warnings, error)) {
        return false;
    }

    Put<uint32_t>(m2, M2At::VERSION, LEGACY_VERSION);
    Put<uint32_t>(m2, M2At::FLAGS, Get<uint32_t>(m2, M2At::FLAGS) & GLOBAL_FLAGS_MASK);

    converted.m2 = std::move(m2);
    converted.skins = std::move(skins);
    model = std::move(converted);
    return true;
}

// The most non-zero bone weights a vertex of the section carries, at least 1. 0 makes the client skin with none.
static bool FitInfluences(std::vector<uint8_t>& skin, size_t section, const std::vector<uint8_t>& m2, std::string& error) {
    Span lookup;
    Span indices;
    Span vertices;
    if (!ReadSpan(skin, SkinAt::VERTICES, 2, "skin vertices", lookup, error) || !ReadSpan(skin, SkinAt::INDICES, 2, "skin indices", indices, error)
        || !ReadSpan(m2, M2At::VERTICES, VERTEX_SIZE, "vertices", vertices, error)) {
        return false;
    }

    uint32_t start = Get<uint16_t>(skin, section + 0x08);
    uint32_t count = Get<uint16_t>(skin, section + 0x0A);
    if (start + count > indices.count) {
        return Fail(error, "a submesh runs past the skin's indices");
    }

    uint16_t used = 1;

    for (uint32_t i = start; i < start + count; i++) {
        uint16_t corner = Get<uint16_t>(skin, indices.At(i, 2));
        if (corner >= lookup.count) {
            return Fail(error, "a skin index names a vertex past the skin's vertex list");
        }

        uint16_t vertex = Get<uint16_t>(skin, lookup.At(corner, 2));
        if (vertex >= vertices.count) {
            return Fail(error, "a skin vertex names a vertex past the model's");
        }

        uint16_t weights = 0;
        for (size_t w = 0; w < 4; w++) {
            weights += m2[vertices.At(vertex, VERTEX_SIZE) + 0x0C + w] != 0;
        }

        used = std::max(used, weights);
    }

    Put<uint16_t>(skin, section + 0x10, used);
    return true;
}

// The flags and checks of one batch; ShadeBatches converts its shading afterwards.
static bool ConvertBatch(std::vector<uint8_t>& skin, size_t batch, const ShadingTables& tables, std::string& error) {
    // Retail reuses geosetIndex as flags2; stock 3.3.5 files keep it equal to the submesh index.
    skin[batch] &= static_cast<uint8_t>(~BATCH_HIDDEN_IN_335);
    Put<uint16_t>(skin, batch + 0x06, Get<uint16_t>(skin, batch + 0x04));

    uint16_t textures = Get<uint16_t>(skin, batch + 0x0E);
    uint16_t textureCombo = Get<uint16_t>(skin, batch + 0x10);

    // An effect's base decides how many it draws (ShadeBatches).
    if (textures > CLIENT_MAX_TEXTURES && !(Get<uint16_t>(skin, batch + 0x02) & EFFECT_FLAG)) {
        return Fail(error, std::to_string(textures) + " textures in one batch without a shader effect have no 3.3.5 shader");
    }

    if (uint32_t(textureCombo) + textures > tables.textureCombos) {
        return Fail(error, "a batch reads texture combo " + std::to_string(textureCombo) + " past the model's " + std::to_string(tables.textureCombos));
    }

    return true;
}

// The original skin of every view the model keeps, as ChooseViews picks them on the model side. Without a reader, the
// skin of a one-view model alone, and none for more views, which the model side then does not split either.
static std::vector<std::vector<uint8_t>> OriginalSkins(const SkinInput& input, const std::vector<uint8_t>& m2) {
    std::vector<uint32_t> sfid = U32s(ReadChunks(input.modelFile)["SFID"]);
    ViewSkins chosen;
    std::string readError;
    if (input.read && sfid.size() >= Get<uint32_t>(m2, M2At::VIEWS) && ChooseViews(m2, sfid, input.read, chosen, readError) && !chosen.originals.empty()) {
        return chosen.originals;
    }

    return Get<uint32_t>(m2, M2At::VIEWS) == 1 ? std::vector<std::vector<uint8_t>>{ input.skin } : std::vector<std::vector<uint8_t>>();
}

bool ConvertSkin(const SkinInput& input, std::vector<uint8_t>& out, std::string& error) {
    const std::vector<uint8_t>& skin = input.skin;
    uint32_t view = input.view;
    if (skin.size() < SkinAt::HEADER || std::memcmp(skin.data(), "SKIN", 4) != 0) {
        return Fail(error, "not a skin file");
    }

    std::vector<uint8_t> m2;
    if (!ConvertGeometry(ReadChunks(input.modelFile), m2, nullptr, nullptr, error)) {
        return false;
    }

    std::vector<std::vector<uint8_t>> originals = OriginalSkins(input, m2);
    uint32_t views = originals.empty() ? Get<uint32_t>(m2, M2At::VIEWS) : static_cast<uint32_t>(originals.size());
    if (view >= views) {
        return Fail(error, "skin view " + std::to_string(view) + " is past the model's " + std::to_string(views) + " views");
    }

    std::vector<uint8_t> converted = originals.empty() ? skin : originals[view];
    BoneWindowPlan plan;
    SkinLayout layout;
    if (!PlanBoneWindows(m2, originals, plan, error) || !SplitSections(converted, m2, plan, view, error) || !LayOutSections(converted, m2, layout, error)) {
        return false;
    }

    Span sections;
    Span batches;
    ShadingTables tables;
    if (!ReadSpan(converted, SkinAt::SECTIONS, SECTION_SIZE, "submeshes", sections, error) || !ReadSpan(converted, SkinAt::BATCHES, BATCH_SIZE, "batches", batches, error)
        || !ReadShadingTables(m2, tables, error)) {
        return false;
    }

    for (uint32_t i = 0; i < sections.count; i++) {
        if (!FitInfluences(converted, sections.At(i, SECTION_SIZE), m2, error)) {
            return false;
        }
    }

    for (uint32_t i = 0; i < batches.count; i++) {
        if (!ConvertBatch(converted, batches.At(i, BATCH_SIZE), tables, error)) {
            return false;
        }
    }

    if (!ShadeBatches(converted, tables, error)) {
        return false;
    }

    Put<uint32_t>(converted, SkinAt::BONE_COUNT_MAX, ViewBonePalette(view, views));
    out = std::move(converted);
    return true;
}

}
