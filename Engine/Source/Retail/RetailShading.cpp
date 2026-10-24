#include "Engine/Source/Retail/RetailShading.h"
#include "Engine/Source/Retail/RetailEffects.h"
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

// The client turns a batch's two combiner entries back into the packed shader retail stores
// (SubstituteSimpleShaders, M2Shared.cpp), and picks its shader from the pair (GetEffect).
constexpr uint32_t USES_COMBINER_COMBOS = 0x8;
constexpr size_t COMBINER_SLOT = M2At::HEADER;
constexpr size_t HEADER_WITH_COMBINERS = COMBINER_SLOT + 8;
constexpr uint16_t COMBINER_OPS = 8;                // M2COMBINER_OP_MASK + 1.
constexpr uint32_t COMBINER_ENTRIES = COMBINER_OPS * COMBINER_OPS * 2;

// Texture coordinate sources, in textureCoordCombos values, in the order of UV1, UV2 and ENV.
constexpr uint16_t COORD_SOURCES[] = { 0, 1, 0xFFFF };
constexpr uint32_t COORD_SOURCE_COUNT = 3;
constexpr uint32_t COORD_PAIR_ENTRIES = COORD_SOURCE_COUNT * COORD_SOURCE_COUNT * 2;

// Each material is followed by layer blocks for passes: [copy, Add], [copy, Alpha, Alpha], [copy, Mod]. The
// client takes a layer's base material from materialIndex - materialLayer (M2Scene.cpp), which is the copy.
constexpr uint16_t LAYER_BLOCK_SIZE = 7;
constexpr uint16_t ADD_BLOCK = 0;
constexpr uint16_t ALPHA_BLOCK = 2;
constexpr uint16_t MOD_BLOCK = 5;
constexpr uint16_t LAYER_BLENDS[LAYER_BLOCK_SIZE] = { 0, BLEND_ADD, 0, BLEND_ALPHA, BLEND_ALPHA, 0, BLEND_MOD };
constexpr uint16_t RENDER_NO_DEPTH_WRITE = 0x10;

struct HeaderArray {
    size_t header;
    size_t recordSize;
    const char* name;
};

// Every array of the retail header, to move the one whose data starts in the slot.
static const HeaderArray HEADER_ARRAYS[] = {
    { 0x08, 1, "name" }, { 0x14, 4, "global loops" }, { 0x1C, 0x40, "sequences" }, { 0x24, 2, "sequence lookup" },
    { 0x2C, 0x58, "bones" }, { 0x34, 2, "bone lookup" }, { 0x3C, 0x30, "vertices" }, { 0x48, 0x28, "colors" },
    { 0x50, 0x10, "textures" }, { 0x58, 0x14, "texture weights" }, { 0x60, 0x3C, "texture transforms" },
    { 0x68, 2, "texture lookup" }, { 0x70, 4, "materials" }, { 0x78, 2, "bone combos" }, { 0x80, 2, "texture combos" },
    { 0x88, 2, "coordinate combos" }, { 0x90, 2, "weight combos" }, { 0x98, 2, "transform combos" },
    { 0xD8, 2, "collision indices" }, { 0xE0, 12, "collision positions" }, { 0xE8, 12, "collision normals" },
    { 0xF0, 0x28, "attachments" }, { 0xF8, 2, "attachment lookup" }, { 0x100, 0x24, "events" }, { 0x108, 0x9C, "lights" },
    { 0x110, 116, "cameras" }, { 0x118, 2, "camera lookup" }, { 0x120, 0xB0, "ribbons" }, { 0x128, 492, "particle emitters" },
};

constexpr size_t WEIGHT_COMBOS = 0x90;
constexpr size_t TRANSFORM_COMBOS = 0x98;

// Retail headers end at the slot and their first array starts there; that array is copied to the end.
static bool FreeCombinerSlot(std::vector<uint8_t>& m2, std::string& error) {
    if (m2.size() < HEADER_WITH_COMBINERS) {
        m2.resize(HEADER_WITH_COMBINERS, 0);
    }

    for (const HeaderArray& array : HEADER_ARRAYS) {
        Span span;
        if (!ReadSpan(m2, array.header, array.recordSize, array.name, span, error)) {
            return false;
        }

        if (!span.count || span.offset >= HEADER_WITH_COMBINERS) {
            continue;
        }

        std::vector<uint8_t> records(m2.begin() + span.offset, m2.begin() + span.offset + span.count * array.recordSize);
        Put<uint32_t>(m2, array.header + 4, AppendAligned(m2, records));
    }

    return true;
}

static void AppendLayerMaterials(std::vector<uint8_t>& m2, const Span& materials) {
    std::vector<uint8_t> all(m2.begin() + materials.offset, m2.begin() + materials.offset + materials.count * MATERIAL_SIZE);
    all.resize(materials.count * MATERIAL_SIZE * (1 + LAYER_BLOCK_SIZE), 0);

    for (uint32_t m = 0; m < materials.count; m++) {
        uint16_t flags = Get<uint16_t>(m2, materials.At(m, MATERIAL_SIZE));
        uint16_t blend = Get<uint16_t>(m2, materials.At(m, MATERIAL_SIZE) + 2);

        for (uint16_t slot = 0; slot < LAYER_BLOCK_SIZE; slot++) {
            size_t at = (materials.count + m * LAYER_BLOCK_SIZE + slot) * MATERIAL_SIZE;
            Put<uint16_t>(all, at, LAYER_BLENDS[slot] ? flags | RENDER_NO_DEPTH_WRITE : flags);
            Put<uint16_t>(all, at + 2, LAYER_BLENDS[slot] ? LAYER_BLENDS[slot] : blend);
        }
    }

    Put<uint32_t>(m2, M2At::MATERIALS, materials.count * (1 + LAYER_BLOCK_SIZE));
    Put<uint32_t>(m2, M2At::MATERIALS + 4, AppendAligned(m2, all));
}

bool AppendShadingTables(std::vector<uint8_t>& m2, std::string& error) {
    Span coords;
    Span materials;
    if (!FreeCombinerSlot(m2, error) || !ReadSpan(m2, M2At::COORD_COMBOS, 2, "coordinate combos", coords, error)
        || !ReadSpan(m2, M2At::MATERIALS, MATERIAL_SIZE, "materials", materials, error)) {
        return false;
    }

    std::vector<uint8_t> combiners(COMBINER_ENTRIES * 2, 0);
    for (uint16_t first = 0; first < COMBINER_OPS; first++) {
        for (uint16_t second = 0; second < COMBINER_OPS; second++) {
            size_t at = (first * COMBINER_OPS + second) * 4;
            Put<uint16_t>(combiners, at, first);
            Put<uint16_t>(combiners, at + 2, second);
        }
    }

    std::vector<uint8_t> pairs(m2.begin() + coords.offset, m2.begin() + coords.offset + coords.count * 2);
    for (uint16_t first : COORD_SOURCES) {
        for (uint16_t second : COORD_SOURCES) {
            pairs.insert(pairs.end(), { uint8_t(first), uint8_t(first >> 8), uint8_t(second), uint8_t(second >> 8) });
        }
    }

    Put<uint32_t>(m2, COMBINER_SLOT, COMBINER_ENTRIES);
    Put<uint32_t>(m2, COMBINER_SLOT + 4, AppendAligned(m2, combiners));
    Put<uint32_t>(m2, M2At::COORD_COMBOS, static_cast<uint32_t>(pairs.size() / 2));
    Put<uint32_t>(m2, M2At::COORD_COMBOS + 4, AppendAligned(m2, pairs));
    Put<uint32_t>(m2, M2At::FLAGS, Get<uint32_t>(m2, M2At::FLAGS) | USES_COMBINER_COMBOS);
    AppendLayerMaterials(m2, materials);
    return true;
}

bool ReadShadingTables(const std::vector<uint8_t>& m2, ShadingTables& tables, std::string& error) {
    Span combiners;
    Span coords;
    Span materials;
    Span textures;
    Span weights;
    Span transforms;
    if (m2.size() < HEADER_WITH_COMBINERS || !(Get<uint32_t>(m2, M2At::FLAGS) & USES_COMBINER_COMBOS)
        || !ReadSpan(m2, COMBINER_SLOT, 2, "combiner combos", combiners, error) || !ReadSpan(m2, M2At::COORD_COMBOS, 2, "coordinate combos", coords, error)
        || !ReadSpan(m2, M2At::MATERIALS, MATERIAL_SIZE, "materials", materials, error) || !ReadSpan(m2, M2At::TEXTURE_COMBOS, 2, "texture combos", textures, error)
        || !ReadSpan(m2, WEIGHT_COMBOS, 2, "weight combos", weights, error) || !ReadSpan(m2, TRANSFORM_COMBOS, 2, "transform combos", transforms, error)
        || combiners.count != COMBINER_ENTRIES || coords.count < COORD_PAIR_ENTRIES || materials.count % (1 + LAYER_BLOCK_SIZE)) {
        return Fail(error, "the skin's model is not converted");
    }

    tables = { coords.count - COORD_PAIR_ENTRIES, materials.count / (1 + LAYER_BLOCK_SIZE), textures.count, weights.count, transforms.count };
    return true;
}

static uint16_t CoordPair(const ShadingTables& tables, uint16_t first, uint16_t second) {
    return static_cast<uint16_t>(tables.coordPairs + (first * COORD_SOURCE_COUNT + second) * 2);
}

static uint16_t CombinerPair(uint16_t first, uint16_t second) {
    return static_cast<uint16_t>((first * COMBINER_OPS + second) * 2);
}

// The base's own shading: one of the client's effects or combiners, its textures cut to what the base draws.
static void ShadeBase(std::vector<uint8_t>& record, const BaseShading& base, const ShadingTables& tables) {
    if (base.clientEffect != NO_CLIENT_EFFECT) {
        // Retail counts effects from 0; the client's table starts at 1. Its effects are Diffuse_T1_Env.
        Put<uint16_t>(record, 0x02, static_cast<uint16_t>(EFFECT_FLAG | (base.clientEffect + 1)));
        Put<uint16_t>(record, 0x0E, base.textures);
        Put<uint16_t>(record, 0x12, CoordPair(tables, UV1, ENV));
        return;
    }

    Put<uint16_t>(record, 0x02, CombinerPair(base.shading.ops[0], base.shading.ops[1]));
    Put<uint16_t>(record, 0x0E, base.textures);
    Put<uint16_t>(record, 0x12, CoordPair(tables, base.shading.coords[0], base.shading.coords[1]));
}

// Entry offset after start of a lookup of count entries, or start when the lookup is not that long.
static uint16_t Slot(uint16_t start, uint16_t offset, uint32_t count) {
    return uint32_t(start) + offset < count ? static_cast<uint16_t>(start + offset) : start;
}

static uint16_t LayerBlock(const Effect& effect) {
    uint16_t blend = effect.passes[0].blend;
    return blend == BLEND_ADD ? ADD_BLOCK : blend == BLEND_ALPHA ? ALPHA_BLOCK : MOD_BLOCK;
}

// The base of an effect with passes draws with its layer block's copy; each pass that fits follows it.
static bool AppendPasses(std::vector<uint8_t>& out, std::vector<uint8_t> base, const Effect& effect, uint16_t declared, const ShadingTables& tables, std::string& error) {
    uint16_t material = Get<uint16_t>(base, 0x0A);
    if (material >= tables.materials) {
        return Fail(error, "a batch names material " + std::to_string(material) + " past the model's " + std::to_string(tables.materials));
    }

    uint16_t copy = static_cast<uint16_t>(tables.materials + material * LAYER_BLOCK_SIZE + LayerBlock(effect));
    uint16_t layer = Get<uint16_t>(base, 0x0C);
    uint16_t textureCombo = Get<uint16_t>(base, 0x10);
    Put<uint16_t>(base, 0x0A, copy);
    out.insert(out.end(), base.begin(), base.end());

    for (uint16_t p = 0; p < effect.passCount; p++) {
        const Pass& pass = effect.passes[p];
        if (!PassFits(pass, declared, textureCombo, tables.textureCombos)) {
            continue;
        }

        // The pass's blend weighs the texture by its alpha, which Mod keeps.
        std::vector<uint8_t> record = base;
        Put<uint16_t>(record, 0x02, CombinerPair(OP_MOD, OP_OPAQUE));
        Put<uint16_t>(record, 0x0A, static_cast<uint16_t>(copy + p + 1));
        Put<uint16_t>(record, 0x0C, static_cast<uint16_t>(layer + p + 1));
        Put<uint16_t>(record, 0x0E, 1);
        Put<uint16_t>(record, 0x10, static_cast<uint16_t>(textureCombo + pass.texture));
        Put<uint16_t>(record, 0x12, CoordPair(tables, pass.coord, UV1));
        Put<uint16_t>(record, 0x14, Slot(Get<uint16_t>(base, 0x14), pass.weight, tables.weightCombos));
        // A pass on UV2 uses the batch's second texture transform (wow.export's T2 reads tex_mat2).
        Put<uint16_t>(record, 0x16, Slot(Get<uint16_t>(base, 0x16), pass.coord == UV2, tables.transformCombos));
        out.insert(out.end(), record.begin(), record.end());
    }

    return true;
}

bool ShadeBatches(std::vector<uint8_t>& skin, const ShadingTables& tables, std::string& error) {
    Span batches;
    if (!ReadSpan(skin, SkinAt::BATCHES, BATCH_SIZE, "batches", batches, error)) {
        return false;
    }

    std::vector<uint8_t> out;
    for (uint32_t i = 0; i < batches.count; i++) {
        std::vector<uint8_t> record(skin.begin() + batches.At(i, BATCH_SIZE), skin.begin() + batches.At(i, BATCH_SIZE) + BATCH_SIZE);
        uint16_t declared = Get<uint16_t>(record, 0x0E);
        BaseShading base = ResolveShading(Get<uint16_t>(record, 0x02), declared);
        ShadeBase(record, base, tables);

        if (!base.effect || !base.effect->passCount) {
            out.insert(out.end(), record.begin(), record.end());
        } else if (!AppendPasses(out, record, *base.effect, declared, tables, error)) {
            return false;
        }
    }

    Put<uint32_t>(skin, SkinAt::BATCHES, static_cast<uint32_t>(out.size() / BATCH_SIZE));
    Put<uint32_t>(skin, SkinAt::BATCHES + 4, AppendAligned(skin, out));
    return true;
}

}
