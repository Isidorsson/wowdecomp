#include <algorithm>
#include <set>
#include "Engine/Source/Retail/RetailEffects.h"
#include "Engine/Source/Retail/RetailModelData.h"
#include "Engine/Source/Retail/RetailShading.h"

namespace Retail {

constexpr size_t VERTEX_UV1 = 0x20;
constexpr size_t VERTEX_UV2 = 0x28;
constexpr size_t UV_SIZE = 8;

// One batch of a view: the model vertices its submesh draws and the UV sets it and its passes sample.
struct UvBatch {
    uint32_t view;
    uint32_t index;
    std::vector<uint16_t> vertices;
    bool firstSetTwice;
    bool readsUv2;
};

static bool DrawnVertices(const std::vector<uint8_t>& skin, size_t batch, uint32_t modelVertices, std::vector<uint16_t>& vertices, std::string& error) {
    Span sections;
    Span lookup;
    if (!ReadSpan(skin, SkinAt::SECTIONS, SECTION_SIZE, "submeshes", sections, error) || !ReadSpan(skin, SkinAt::VERTICES, 2, "skin vertices", lookup, error)) {
        return false;
    }

    uint16_t section = Get<uint16_t>(skin, batch + 0x04);
    if (section >= sections.count) {
        return Fail(error, "a batch names submesh " + std::to_string(section) + " past the skin's " + std::to_string(sections.count));
    }

    uint32_t start = Get<uint16_t>(skin, sections.At(section, SECTION_SIZE) + 0x04);
    uint32_t count = Get<uint16_t>(skin, sections.At(section, SECTION_SIZE) + 0x06);
    if (start + count > lookup.count) {
        return Fail(error, "a submesh runs past the skin's vertex list");
    }

    for (uint32_t i = start; i < start + count; i++) {
        uint16_t vertex = Get<uint16_t>(skin, lookup.At(i, 2));
        if (vertex >= modelVertices) {
            return Fail(error, "a skin vertex names a vertex past the model's");
        }
        vertices.push_back(vertex);
    }

    return true;
}

static bool PassesReadUv2(const BaseShading& base, uint16_t declared) {
    if (!base.effect) {
        return false;
    }

    for (uint16_t p = 0; p < base.effect->passCount; p++) {
        if (base.effect->passes[p].coord == UV2 && base.effect->passes[p].texture < declared) {
            return true;
        }
    }
    return false;
}

static UvBatch ClassifyBatch(uint32_t view, uint32_t index, uint16_t shader, uint16_t declared) {
    BaseShading base = ResolveShading(shader, declared);
    const Shading& shading = base.shading;
    bool combiners = base.clientEffect == NO_CLIENT_EFFECT;
    bool firstSetTwice = combiners && base.textures == 2 && shading.coords[0] == UV1 && shading.coords[1] == UV1;
    bool readsUv2 = (combiners && (shading.coords[0] == UV2 || (base.textures == 2 && shading.coords[1] == UV2))) || PassesReadUv2(base, declared);
    return { view, index, {}, firstSetTwice, readsUv2 };
}

static bool ReadUvBatches(const std::vector<std::vector<uint8_t>>& skins, uint32_t modelVertices, std::vector<UvBatch>& batches, std::string& error) {
    for (uint32_t view = 0; view < skins.size(); view++) {
        Span records;
        if (!ReadSpan(skins[view], SkinAt::BATCHES, BATCH_SIZE, "batches", records, error)) {
            return false;
        }

        for (uint32_t i = 0; i < records.count; i++) {
            size_t at = records.At(i, BATCH_SIZE);
            UvBatch batch = ClassifyBatch(view, i, Get<uint16_t>(skins[view], at + 0x02), Get<uint16_t>(skins[view], at + 0x0E));
            if ((batch.firstSetTwice || batch.readsUv2) && !DrawnVertices(skins[view], at, modelVertices, batch.vertices, error)) {
                return false;
            }
            batches.push_back(std::move(batch));
        }
    }

    return true;
}

static std::map<uint32_t, std::string> BatchNames(const std::vector<const UvBatch*>& batches) {
    std::map<uint32_t, std::vector<uint32_t>> byView;
    for (const UvBatch* batch : batches) {
        byView[batch->view].push_back(batch->index);
    }
    return BatchNames(byView);
}

bool CopyFirstUvSet(std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, std::vector<std::string>& warnings, std::string& error) {
    Span vertices;
    std::vector<UvBatch> batches;
    if (!ReadSpan(m2, M2At::VERTICES, VERTEX_SIZE, "vertices", vertices, error) || !ReadUvBatches(skins, vertices.count, batches, error)) {
        return false;
    }

    std::set<uint16_t> readingUv2;
    for (const UvBatch& batch : batches) {
        if (batch.readsUv2) {
            readingUv2.insert(batch.vertices.begin(), batch.vertices.end());
        }
    }

    std::vector<const UvBatch*> copied;
    std::vector<const UvBatch*> kept;
    std::set<uint16_t> copiedVertices;
    for (const UvBatch& batch : batches) {
        if (!batch.firstSetTwice) {
            continue;
        }

        bool shared = std::any_of(batch.vertices.begin(), batch.vertices.end(), [&](uint16_t vertex) { return readingUv2.count(vertex); });
        (shared ? kept : copied).push_back(&batch);
        if (!shared) {
            copiedVertices.insert(batch.vertices.begin(), batch.vertices.end());
        }
    }

    for (uint16_t vertex : copiedVertices) {
        size_t at = vertices.At(vertex, VERTEX_SIZE);
        std::memcpy(m2.data() + at + VERTEX_UV2, m2.data() + at + VERTEX_UV1, UV_SIZE);
    }

    // The downporter's report entries (second_uv_set).
    for (const auto& name : BatchNames(copied)) {
        warnings.push_back(name.second + ": drew both textures with the first UV set, which the 3.3.5 client cannot (its second texture always reads the second set); the first set was copied into the second on the "
            + std::to_string(copiedVertices.size()) + " vertices they draw");
    }

    for (const auto& name : BatchNames(kept)) {
        warnings.push_back(name.second + ": draw both textures with the first UV set, but other batches draw the same vertices with the second set, so it was kept; their second texture reads the second UV set in the 3.3.5 client");
    }

    return true;
}

}
