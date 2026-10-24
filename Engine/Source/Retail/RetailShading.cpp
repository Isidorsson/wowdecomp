#include "Engine/Source/Retail/RetailShading.h"
#include <algorithm>
#include <map>
#include <set>
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

// The client turns a batch's two combiner entries back into the packed shader retail stores
// (SubstituteSimpleShaders, M2Shared.cpp), and picks its shader from the pair (GetEffect).
constexpr uint32_t USES_COMBINER_COMBOS = 0x8;
constexpr size_t COMBINER_SLOT = M2At::HEADER;
constexpr size_t HEADER_WITH_COMBINERS = COMBINER_SLOT + 8;
constexpr uint16_t COMBINER_OPS = 8;                // M2COMBINER_OP_MASK + 1.
constexpr uint32_t COMBINER_ENTRIES = COMBINER_OPS * COMBINER_OPS * 2;

// M2COMBINER operations.
constexpr uint16_t OP_OPAQUE = 0;
constexpr uint16_t OP_MOD = 1;
constexpr uint16_t OP_ADD = 3;
constexpr uint16_t OP_MOD2X = 4;
constexpr uint16_t OP_MOD2X_NA = 6;
constexpr uint16_t OP_ADD_NA = 7;

// Texture coordinate sources, in textureCoordCombos values: UV1, UV2, the environment map.
constexpr uint16_t COORD_SOURCES[] = { 0, 1, 0xFFFF };
constexpr uint16_t UV1 = 0;
constexpr uint16_t UV2 = 1;
constexpr uint16_t ENV = 2;
constexpr uint32_t COORD_SOURCE_COUNT = 3;
constexpr uint32_t COORD_PAIR_ENTRIES = COORD_SOURCE_COUNT * COORD_SOURCE_COUNT * 2;

// The bits of a retail shader without the effect flag (wow.export ShaderMapper, as wow-toolbox-editor's
// downport/transform/shading.rs decodes them).
constexpr uint16_t SHADER_FIRST_MOD = 0x70;
constexpr uint16_t SHADER_FIRST_ENV = 0x80;
constexpr uint16_t SHADER_SECOND_OP = 0x07;
constexpr uint16_t SHADER_SECOND_ENV = 0x08;
constexpr uint16_t SHADER_LAST_UV2 = 0x4000;
constexpr uint16_t EFFECT_FLAG = 0x8000;
constexpr uint16_t CLIENT_EFFECTS = 3;

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

bool AppendShadingTables(std::vector<uint8_t>& m2, std::string& error) {
    Span coords;
    if (!FreeCombinerSlot(m2, error) || !ReadSpan(m2, M2At::COORD_COMBOS, 2, "coordinate combos", coords, error)) {
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
    return true;
}

bool ReadShadingTables(const std::vector<uint8_t>& m2, ShadingTables& tables, std::string& error) {
    Span combiners;
    Span coords;
    if (m2.size() < HEADER_WITH_COMBINERS || !(Get<uint32_t>(m2, M2At::FLAGS) & USES_COMBINER_COMBOS)
        || !ReadSpan(m2, COMBINER_SLOT, 2, "combiner combos", combiners, error) || !ReadSpan(m2, M2At::COORD_COMBOS, 2, "coordinate combos", coords, error)
        || combiners.count != COMBINER_ENTRIES || coords.count < COORD_PAIR_ENTRIES) {
        return Fail(error, "the skin's model is not converted");
    }

    tables = { coords.count - COORD_PAIR_ENTRIES };
    return true;
}

struct Shading {
    uint16_t ops[2];
    uint16_t coords[2];     // Indices into COORD_SOURCES.
};

static uint16_t SecondOp(uint16_t shader, uint16_t first) {
    uint16_t op = shader & SHADER_SECOND_OP;

    if (first == OP_OPAQUE && op == 0) {
        return OP_OPAQUE;
    }

    if (first == OP_OPAQUE && op == OP_ADD_NA) {
        return OP_ADD;
    }

    // Decal (2) and Fade (5) have no two-texture 3.3.5 shader.
    return op == OP_ADD || op == OP_MOD2X || op == OP_MOD2X_NA || op == OP_ADD_NA ? op : OP_MOD;
}

static Shading DecodeShader(uint16_t shader, uint16_t textures) {
    uint16_t first = shader & SHADER_FIRST_MOD ? OP_MOD : OP_OPAQUE;
    bool firstEnv = shader & SHADER_FIRST_ENV;
    bool lastUv2 = shader & SHADER_LAST_UV2;

    if (textures <= 1) {
        return { { first, OP_OPAQUE }, { firstEnv ? ENV : lastUv2 ? UV2 : UV1, UV1 } };
    }

    uint16_t secondCoord = shader & SHADER_SECOND_ENV ? ENV : !firstEnv && lastUv2 ? UV2 : UV1;
    return { { first, SecondOp(shader, first) }, { firstEnv ? ENV : UV1, secondCoord } };
}

static uint16_t CoordPair(const ShadingTables& tables, uint16_t first, uint16_t second) {
    return static_cast<uint16_t>(tables.coordPairs + (first * COORD_SOURCE_COUNT + second) * 2);
}

bool ShadeBatch(std::vector<uint8_t>& skin, size_t batch, const ShadingTables& tables, std::string& error) {
    uint16_t shader = Get<uint16_t>(skin, batch + 0x02);

    if (shader & EFFECT_FLAG) {
        uint16_t effect = shader & ~EFFECT_FLAG;
        if (effect >= CLIENT_EFFECTS) {
            return Fail(error, "shader effect " + std::to_string(effect) + " has no 3.3.5 shader (converter slice C5b)");
        }

        // Retail counts effects from 0; the client's table starts at 1. Its effects are Diffuse_T1_Env.
        Put<uint16_t>(skin, batch + 0x02, static_cast<uint16_t>(EFFECT_FLAG | (effect + 1)));
        Put<uint16_t>(skin, batch + 0x12, CoordPair(tables, UV1, ENV));
        return true;
    }

    Shading shading = DecodeShader(shader, Get<uint16_t>(skin, batch + 0x0E));
    Put<uint16_t>(skin, batch + 0x02, static_cast<uint16_t>((shading.ops[0] * COMBINER_OPS + shading.ops[1]) * 2));
    Put<uint16_t>(skin, batch + 0x12, CoordPair(tables, shading.coords[0], shading.coords[1]));
    return true;
}

constexpr size_t VERTEX_UV1 = 0x20;
constexpr size_t VERTEX_UV2 = 0x28;
constexpr size_t UV_SIZE = 8;

// One classic batch of a view: the model vertices its submesh draws and the UV sets it samples.
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

static bool ReadUvBatches(const std::vector<std::vector<uint8_t>>& skins, uint32_t modelVertices, std::vector<UvBatch>& batches, std::string& error) {
    for (uint32_t view = 0; view < skins.size(); view++) {
        Span records;
        if (!ReadSpan(skins[view], SkinAt::BATCHES, BATCH_SIZE, "batches", records, error)) {
            return false;
        }

        for (uint32_t i = 0; i < records.count; i++) {
            size_t at = records.At(i, BATCH_SIZE);
            uint16_t shader = Get<uint16_t>(skins[view], at + 0x02);
            uint16_t textures = Get<uint16_t>(skins[view], at + 0x0E);
            if (shader & EFFECT_FLAG) {
                continue;
            }

            Shading shading = DecodeShader(shader, textures);
            UvBatch batch { view, i, {}, textures == 2 && shading.coords[0] == UV1 && shading.coords[1] == UV1,
                shading.coords[0] == UV2 || (textures == 2 && shading.coords[1] == UV2) };
            if ((batch.firstSetTwice || batch.readsUv2) && !DrawnVertices(skins[view], at, modelVertices, batch.vertices, error)) {
                return false;
            }
            batches.push_back(std::move(batch));
        }
    }

    return true;
}

// "skin 0: batches 3, 4" for the warnings, one per view.
static std::map<uint32_t, std::string> BatchNames(const std::vector<const UvBatch*>& batches) {
    std::map<uint32_t, std::vector<uint32_t>> byView;
    for (const UvBatch* batch : batches) {
        byView[batch->view].push_back(batch->index);
    }

    std::map<uint32_t, std::string> names;
    for (const auto& view : byView) {
        std::string name = "skin " + std::to_string(view.first) + (view.second.size() == 1 ? ": batch " : ": batches ");
        for (size_t i = 0; i < view.second.size(); i++) {
            name += (i ? ", " : "") + std::to_string(view.second[i]);
        }
        names[view.first] = name;
    }
    return names;
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
