#include "Engine/Source/Retail/RetailBoneWindows.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"
#include <algorithm>
#include <array>

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ConvertSkin;
using Retail::PartitionTriangles;
using Retail::TriangleWindow;

// Submeshes wider than the bone palette the client assumes for their view are cut into pieces of whole triangles,
// each with a window of only the bones it weighs, appended to the model's bone combos in view order (wow-toolbox-
// editor downport/transform/bone_windows.rs).

namespace SectionAt {
    constexpr size_t ID = 0x00;
    constexpr size_t LEVEL = 0x02;
    constexpr size_t VERTEX_START = 0x04;
    constexpr size_t VERTEX_COUNT = 0x06;
    constexpr size_t INDEX_START = 0x08;
    constexpr size_t INDEX_COUNT = 0x0A;
    constexpr size_t BONE_COUNT = 0x0C;
    constexpr size_t BONE_COMBO = 0x0E;
    constexpr size_t INFLUENCES = 0x10;
    constexpr size_t CENTER_BONE = 0x12;
}

constexpr uint32_t FIRST_SKIN = 9000;   // The SFID of view v is FIRST_SKIN + v.
constexpr uint16_t STRIP_ID = 5;
constexpr uint16_t STRIP_CENTER_BONE = 3;
constexpr uint32_t THREE_VIEW_PALETTES[] = { 64, 53, 21 };

static std::vector<uint8_t> U16s(const std::vector<uint16_t>& values) {
    std::vector<uint8_t> bytes(values.size() * 2);
    for (size_t i = 0; i < values.size(); i++) {
        PutU16(bytes, i * 2, values[i]);
    }
    return bytes;
}

struct Strip {
    std::vector<uint8_t> md20;   // Unconverted, for what the original skin draws.
    std::vector<uint8_t> file;
    std::vector<uint8_t> skin;   // Every view's.
    uint32_t views;
};

// A strip of quads along a 2-row grid (the downporter's bone_windows tests): quad q weighs bones q and q + 1, so its
// submesh needs quads + 1 bones. The bone combos list the bones in reverse, so window slots differ from bone ids.
// lead vertices come first, in a one-bone submesh of separate triangles.
static Strip MakeStrip(uint16_t quads, uint32_t views, uint32_t lead = 0, std::vector<std::vector<uint8_t>> batches = { SkinBatch(0, 0, 1) }) {
    uint16_t columns = quads + 1;
    uint32_t vertices = lead + columns * 2u;
    Strip strip;
    strip.views = views;

    strip.md20 = NewMd20();
    PutU32(strip.md20, M2At::VIEWS, views);
    AddArray(strip.md20, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(strip.md20, M2At::BONES, columns, Concat(std::vector<std::vector<uint8_t>>(columns, Bone(0))));
    std::vector<uint8_t> records;
    for (uint32_t v = 0; v < vertices; v++) {
        std::vector<uint8_t> vertex = Vertex(255, 0, 0, 0);
        records.insert(records.end(), vertex.begin(), vertex.end());
    }
    AddArray(strip.md20, M2At::VERTICES, vertices, records);
    std::vector<uint16_t> combos;
    for (uint16_t c = 0; c < columns; c++) {
        combos.push_back(static_cast<uint16_t>(columns - 1 - c));
    }
    AddArray(strip.md20, M2At::BONE_COMBOS, columns, U16s(combos));
    AddArray(strip.md20, M2At::TEXTURE_COMBOS, 1, U16s({ 0 }));
    AddArray(strip.md20, M2At::COORD_COMBOS, 1, U16s({ 0 }));
    std::vector<uint32_t> ids;
    for (uint32_t v = 0; v < views; v++) {
        ids.push_back(FIRST_SKIN + v);
    }
    strip.file = Md21File(strip.md20, { Chunk("SFID", U32s(ids)) });

    std::vector<uint16_t> lookup;
    std::vector<uint8_t> properties;
    std::vector<uint16_t> indices;
    for (uint32_t v = 0; v < vertices; v++) {
        lookup.push_back(static_cast<uint16_t>(v));
        uint8_t slot = v < lead ? 0 : static_cast<uint8_t>(columns - 1 - (v - lead) % columns);
        properties.insert(properties.end(), { slot, 0, 0, 0 });
        if (v < lead) {
            indices.push_back(static_cast<uint16_t>(v));
        }
    }
    for (uint32_t q = 0; q < quads; q++) {
        uint32_t a = lead + q, b = a + 1, c = a + columns, d = c + 1;
        for (uint32_t corner : { a, b, c, b, d, c }) {
            indices.push_back(static_cast<uint16_t>(corner));
        }
    }

    std::vector<uint8_t> sections;
    if (lead) {
        sections = SkinSection(1, 0, 1, static_cast<uint16_t>(lead));
        PutU16(sections, SectionAt::VERTEX_COUNT, static_cast<uint16_t>(lead));
    }
    std::vector<uint8_t> section = SkinSection(columns, 0, 4, static_cast<uint16_t>(quads * 6));
    PutU16(section, SectionAt::ID, STRIP_ID);
    PutU16(section, SectionAt::VERTEX_START, static_cast<uint16_t>(lead));
    PutU16(section, SectionAt::VERTEX_COUNT, static_cast<uint16_t>(columns * 2));
    PutU16(section, SectionAt::INDEX_START, static_cast<uint16_t>(lead));
    PutU16(section, SectionAt::CENTER_BONE, STRIP_CENTER_BONE);
    sections.insert(sections.end(), section.begin(), section.end());
    for (std::vector<uint8_t>& batch : batches) {
        PutU16(batch, 0x04, lead ? 1 : 0);
    }

    strip.skin = NewSkin();
    AddArray(strip.skin, SkinAt::VERTICES, vertices, U16s(lookup));
    AddArray(strip.skin, SkinAt::INDICES, static_cast<uint32_t>(indices.size()), U16s(indices));
    AddArray(strip.skin, SkinAt::BONE_PROPERTIES, vertices, properties);
    AddArray(strip.skin, SkinAt::SECTIONS, lead ? 2 : 1, sections);
    AddArray(strip.skin, SkinAt::BATCHES, static_cast<uint32_t>(batches.size()), Concat(batches));
    return strip;
}

// Every view's skin, except the one named unread.
static Retail::FileReader ReaderOf(const Strip& strip, uint32_t unread = 0) {
    return [skin = strip.skin, views = strip.views, unread](uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
        if (id < FIRST_SKIN || id >= FIRST_SKIN + views || id == unread) {
            error = "no file " + std::to_string(id);
            return false;
        }
        bytes = skin;
        return true;
    };
}

static ConvertedModel ModelOf(const Strip& strip, Retail::FileReader read) {
    Retail::ModelSources sources;
    sources.read = std::move(read);
    ConvertedModel model;
    std::string error;
    if (!ConvertModel(strip.file, sources, model, error)) {
        std::printf("    (%s)\n", error.c_str());
        model.m2 = NewMd20();
    }
    return model;
}

static bool ConvertStrip(const Strip& strip, uint32_t view, Retail::FileReader read, std::vector<uint8_t>& out, std::string& error) {
    out = NewSkin();
    return ConvertSkin({ strip.skin, strip.file, view, std::move(read) }, out, error);
}

static std::vector<uint8_t> SkinFor(const Strip& strip, uint32_t view, Retail::FileReader read) {
    std::vector<uint8_t> out;
    std::string error;
    if (!ConvertStrip(strip, view, std::move(read), out, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return out;
}

static std::string SkinError(const Strip& strip, uint32_t view) {
    std::vector<uint8_t> out;
    std::string error;
    return ConvertStrip(strip, view, {}, out, error) ? std::string("converted") : error;
}

static uint32_t Sections(const std::vector<uint8_t>& skin) {
    return GetU32(skin, SkinAt::SECTIONS);
}

static uint16_t SectionField(const std::vector<uint8_t>& skin, uint32_t section, size_t field) {
    return GetU16(skin, RecordAt(skin, SkinAt::SECTIONS, section, 0x30) + field);
}

static uint16_t U16At(const std::vector<uint8_t>& blob, size_t arrayAt, size_t index) {
    return index < GetU32(blob, arrayAt) ? GetU16(blob, RecordAt(blob, arrayAt, index, 2)) : 0xFFFF;
}

using Corner = std::pair<uint16_t, uint16_t>;   // The model vertex and the model bone a triangle corner draws with.
using Triangle = std::array<Corner, 3>;

static Corner CornerAt(const std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, uint32_t section, uint16_t local) {
    if (local >= GetU32(skin, SkinAt::BONE_PROPERTIES)) {
        return { 0xFFFF, 0xFFFF };
    }
    uint8_t slot = skin[RecordAt(skin, SkinAt::BONE_PROPERTIES, local, 4)];
    return { U16At(skin, SkinAt::VERTICES, local), U16At(m2, M2At::BONE_COMBOS, SectionField(skin, section, SectionAt::BONE_COMBO) + slot) };
}

// Every triangle as what it draws, sorted so two skins compare whatever their layout.
static std::vector<Triangle> Drawn(const std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2) {
    std::vector<Triangle> drawn;
    for (uint32_t s = 0; s < Sections(skin); s++) {
        uint32_t start = SectionField(skin, s, SectionAt::INDEX_START);
        uint32_t end = start + SectionField(skin, s, SectionAt::INDEX_COUNT);
        for (uint32_t i = start; i + 2 < end; i += 3) {
            Triangle triangle;
            for (uint32_t k = 0; k < 3; k++) {
                triangle[k] = CornerAt(skin, m2, s, U16At(skin, SkinAt::INDICES, i + k));
            }
            drawn.push_back(triangle);
        }
    }
    std::sort(drawn.begin(), drawn.end());
    return drawn;
}

static bool Warned(const ConvertedModel& model, std::vector<const char*> parts) {
    return std::any_of(model.warnings.begin(), model.warnings.end(), [&](const std::string& warning) {
        return std::all_of(parts.begin(), parts.end(), [&](const char* part) { return warning.find(part) != std::string::npos; });
    });
}

TEST(PartitionGrowsEachWindowByTheBoneThatCompletesTheMostTriangles) {
    // Ties go to the lowest bone, as in the downporter.
    std::vector<TriangleWindow> windows = PartitionTriangles({ { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 } }, 3);

    CHECK(windows == std::vector<TriangleWindow>({ { { 0, 1, 2 }, { 0, 1 } }, { { 2, 3, 4 }, { 2, 3 } } }));
}

TEST(PartitionWindowsFitThePaletteAndHoldEveryTriangleOnce) {
    std::vector<std::vector<uint16_t>> triangles;
    for (uint16_t q = 0; q < 131; q++) {
        triangles.insert(triangles.end(), 2, { q, static_cast<uint16_t>(q + 1) });
    }

    std::vector<TriangleWindow> windows = PartitionTriangles(triangles, 64);

    CHECK(windows.size() == 3);
    std::vector<int> held(triangles.size(), 0);
    bool fit = true;
    bool covered = true;
    for (const TriangleWindow& window : windows) {
        fit &= window.bones.size() <= 64 && std::is_sorted(window.bones.begin(), window.bones.end());
        fit &= window.triangles.size() <= windows[0].triangles.size();
        for (uint32_t t : window.triangles) {
            held[t]++;
            covered &= std::includes(window.bones.begin(), window.bones.end(), triangles[t].begin(), triangles[t].end());
        }
    }
    CHECK(fit && covered);
    CHECK(std::all_of(held.begin(), held.end(), [](int count) { return count == 1; }));
}

TEST(PartitionGivesATriangleWiderThanThePaletteItsOwnWindow) {
    CHECK(PartitionTriangles({ { 0, 1, 2, 3 } }, 3) == std::vector<TriangleWindow>({ { { 0, 1, 2, 3 }, { 0 } } }));
}

TEST(SubmeshesWithinTheirPaletteAreLeftAsTheyAre) {
    Strip strip = MakeStrip(20, 1);
    std::vector<uint8_t> skin = SkinFor(strip, 0, {});

    CHECK(Sections(skin) == 1 && SectionField(skin, 0, SectionAt::BONE_COUNT) == 21);
    CHECK(U16sAt(skin, SkinAt::VERTICES) == U16sAt(strip.skin, SkinAt::VERTICES));
    CHECK(GetU32(ModelOf(strip, ReaderOf(strip)).m2, M2At::BONE_COMBOS) == 21);
}

TEST(WideSubmeshesAreSplitIntoWindowsOfTheirViewsPalette) {
    Strip strip = MakeStrip(131, 3);
    ConvertedModel model = ModelOf(strip, ReaderOf(strip));
    std::vector<Triangle> before = Drawn(strip.skin, strip.md20);

    for (uint32_t view = 0; view < 3; view++) {
        std::vector<uint8_t> skin = SkinFor(strip, view, ReaderOf(strip));
        bool fit = true;
        for (uint32_t s = 0; s < Sections(skin); s++) {
            fit &= SectionField(skin, s, SectionAt::BONE_COUNT) <= THREE_VIEW_PALETTES[view];
        }
        CHECK(Sections(skin) >= 3 && fit);
        CHECK(Drawn(skin, model.m2) == before);
    }
}

TEST(ModelsAppendEveryViewsWindowsAfterTheirOwnBoneCombos) {
    Strip strip = MakeStrip(131, 3);
    ConvertedModel model = ModelOf(strip, ReaderOf(strip));
    std::vector<uint16_t> own = U16sAt(strip.md20, M2At::BONE_COMBOS);
    std::vector<uint16_t> combos = U16sAt(model.m2, M2At::BONE_COMBOS);

    CHECK(combos.size() > own.size() && std::equal(own.begin(), own.end(), combos.begin()));
    // Each piece's window follows the last, view after view.
    size_t next = own.size();
    bool placed = true;
    for (uint32_t view = 0; view < 3; view++) {
        std::vector<uint8_t> skin = SkinFor(strip, view, ReaderOf(strip));
        for (uint32_t s = 0; s < Sections(skin); s++) {
            placed &= SectionField(skin, s, SectionAt::BONE_COMBO) == next;
            next += SectionField(skin, s, SectionAt::BONE_COUNT);
        }
    }
    CHECK(placed && next == combos.size());
}

TEST(EveryPieceOwnsContiguousVertexAndIndexRanges) {
    Strip strip = MakeStrip(60, 1);
    std::vector<uint8_t> skin = SkinFor(strip, 0, {});

    CHECK(Sections(skin) >= 3);
    uint32_t vertex = 0;
    uint32_t index = 0;
    bool ranges = true;
    bool inside = true;
    for (uint32_t s = 0; s < Sections(skin); s++) {
        uint32_t start = SectionField(skin, s, SectionAt::VERTEX_START);
        uint32_t count = SectionField(skin, s, SectionAt::VERTEX_COUNT);
        ranges &= start == vertex && SectionField(skin, s, SectionAt::INDEX_START) == index;
        for (uint32_t i = 0; i < SectionField(skin, s, SectionAt::INDEX_COUNT); i++) {
            uint16_t corner = U16At(skin, SkinAt::INDICES, index + i);
            inside &= corner >= start && corner < start + count;
        }
        vertex += count;
        index += SectionField(skin, s, SectionAt::INDEX_COUNT);
    }
    CHECK(ranges && inside);
    CHECK(vertex == GetU32(skin, SkinAt::VERTICES) && GetU32(skin, SkinAt::BONE_PROPERTIES) == vertex);
    CHECK(index == GetU32(skin, SkinAt::INDICES));
}

TEST(PiecesKeepTheirParentsFieldsAndFitTheirInfluences) {
    Strip strip = MakeStrip(60, 1);
    std::vector<uint8_t> skin = SkinFor(strip, 0, {});

    CHECK(Sections(skin) >= 3);
    bool kept = true;
    for (uint32_t s = 0; s < Sections(skin); s++) {
        kept &= SectionField(skin, s, SectionAt::ID) == STRIP_ID && SectionField(skin, s, SectionAt::CENTER_BONE) == STRIP_CENTER_BONE;
        kept &= SectionField(skin, s, SectionAt::INFLUENCES) == 1 && SectionField(skin, s, SectionAt::LEVEL) == 0;
    }
    CHECK(kept);
}

TEST(TheBatchesOfASplitSubmeshAreRepeatedPerPieceInOrder) {
    std::vector<uint8_t> first = SkinBatch(0, 0, 1);
    std::vector<uint8_t> second = SkinBatch(0, 0, 1);
    first[1] = 1;
    second[1] = 7;
    std::vector<uint8_t> skin = SkinFor(MakeStrip(60, 1, 0, { first, second }), 0, {});
    uint32_t pieces = Sections(skin);

    CHECK(pieces >= 3 && GetU32(skin, SkinAt::BATCHES) == 2 * pieces);
    bool repeated = true;
    for (uint32_t i = 0; i < GetU32(skin, SkinAt::BATCHES); i++) {
        size_t batch = RecordAt(skin, SkinAt::BATCHES, i, 0x18);
        uint16_t section = GetU16(skin, batch + 0x04);
        repeated &= section == i % pieces && GetU16(skin, batch + 0x06) == section && skin[batch + 1] == (i < pieces ? 1 : 7);
    }
    CHECK(repeated);
}

TEST(TheFullViewOfAFourViewModelKeepsItsWideWindows) {
    Strip strip = MakeStrip(131, 4);
    std::vector<uint8_t> full = SkinFor(strip, 0, ReaderOf(strip));

    CHECK(Sections(full) == 1 && SectionField(full, 0, SectionAt::BONE_COUNT) == 132);
    CHECK(Sections(SkinFor(strip, 1, ReaderOf(strip))) >= 3);
    CHECK(!Warned(ModelOf(strip, ReaderOf(strip)), { "skin 0:", "drew with more than" }));
}

TEST(SplitsAreReportedLikeTheDownporter) {
    Strip strip = MakeStrip(131, 3);
    ConvertedModel model = ModelOf(strip, ReaderOf(strip));

    CHECK(Warned(model, { "skin 0: 1 submesh (0) drew with more than the 64 bones view 0 of a 3-view model allows", "picks views by position, not by boneCountMax", "They became 3 submeshes", "(264 before) and the same triangles" }));
    CHECK(Warned(model, { "skin 2: 1 submesh (0) drew with more than the 21 bones view 2 of a 3-view model allows" }));
}

TEST(ModelsWithAnUnreadSkinSplitNothing) {
    Strip strip = MakeStrip(131, 3);
    ConvertedModel model = ModelOf(strip, ReaderOf(strip, FIRST_SKIN + 2));

    CHECK(GetU32(model.m2, M2At::BONE_COMBOS) == 132);
    CHECK(Warned(model, { "skin 2 (9002) not read", "no submesh is split" }));
    CHECK(Sections(SkinFor(strip, 0, ReaderOf(strip, FIRST_SKIN + 2))) == 1);
    CHECK(Sections(SkinFor(strip, 0, {})) == 1);
}

TEST(SplitsPastTheSkinsLimitsAreRefusedByName) {
    // A view of 65,536 vertices, which 16-bit indices name at most, gains copies where its pieces meet.
    CHECK(SkinError(MakeStrip(64, 1, 65406), 0).find("65,536 vertices") != std::string::npos);
}

TEST(PiecesPastIndex65535AreLaidOutWithTheLargestLast) {
    Strip strip = MakeStrip(64, 1, 65340);
    std::vector<uint8_t> skin = SkinFor(strip, 0, {});
    uint32_t last = Sections(skin) - 1;

    CHECK(Sections(skin) >= 3 && SectionField(skin, last, SectionAt::INDEX_COUNT) == 65340);
    CHECK(SectionField(skin, last, SectionAt::INDEX_START) + 65340u == GetU32(skin, SkinAt::INDICES));
    CHECK(Drawn(skin, ModelOf(strip, ReaderOf(strip)).m2) == Drawn(strip.skin, strip.md20));
}

TEST(LeveledSubmeshesSplitTheTrianglesTheirLevelNames) {
    // The strip's indices moved past 65,535 behind unused ones, its start in Level and the low half.
    Strip strip = MakeStrip(60, 1);
    std::vector<uint16_t> indices(65538, 0xEEEE);
    std::vector<uint16_t> own = U16sAt(strip.skin, SkinAt::INDICES);
    indices.insert(indices.end(), own.begin(), own.end());
    std::vector<uint8_t> leveled = strip.skin;
    AddArray(leveled, SkinAt::INDICES, static_cast<uint32_t>(indices.size()), U16s(indices));
    size_t section = RecordAt(leveled, SkinAt::SECTIONS, 0, 0x30);
    PutU16(leveled, section + SectionAt::LEVEL, 1);
    PutU16(leveled, section + SectionAt::INDEX_START, 2);
    std::vector<uint8_t> out;
    std::string error;

    CHECK(ConvertSkin({ leveled, strip.file, 0, {} }, out, error));
    CHECK(Sections(out) >= 3 && Drawn(out, ModelOf(strip, ReaderOf(strip)).m2) == Drawn(strip.skin, strip.md20));
}

TEST(ModelsKeepTheSplitSkinOfEveryView) {
    Strip strip = MakeStrip(131, 3);
    ConvertedModel model = ModelOf(strip, ReaderOf(strip));

    CHECK(model.skinBytes.size() == 3);
    for (uint32_t view = 0; view < model.skinBytes.size(); view++) {
        CHECK(!model.skinBytes[view].empty() && model.skinBytes[view] == SkinFor(strip, view, ReaderOf(strip)));
    }
}

TEST(ModelsKeepNoSkinForAViewThatDoesNotConvert) {
    Strip strip = MakeStrip(64, 1, 65406);

    CHECK(ModelOf(strip, ReaderOf(strip)).skinBytes == std::vector<std::vector<uint8_t>>(1));
}
