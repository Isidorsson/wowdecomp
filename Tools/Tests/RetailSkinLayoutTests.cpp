#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RetailSkinLayout.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"
#include <algorithm>
#include <map>

using Retail::ConvertedModel;
using Retail::LayOutSections;
using Retail::SkinLayout;

// Retail skins past 65,535 corners name the high half of a submesh's first index in Level. The client ignores
// Level and keeps each submesh's start in its index buffer, submeshes in order, in 16 bits (CM2Shared::SetIndices).

namespace SectionAt {
    constexpr size_t ID = 0x00;
    constexpr size_t LEVEL = 0x02;
    constexpr size_t INDEX_START = 0x08;
    constexpr size_t INDEX_COUNT = 0x0A;
}

constexpr uint16_t FIRST_ID = 100;
constexpr uint16_t GAP = 0xEEEE;
constexpr uint32_t VIEW_SKIN = 7000;
constexpr uint32_t FIRST_LOD = 7001;
static const std::vector<uint8_t> MODEL = MinimalMd20();

struct Range {
    uint32_t start;
    uint16_t count;
};

static uint16_t Field(const std::vector<uint8_t>& skin, uint32_t section, size_t field) {
    return GetU16(skin, RecordAt(skin, SkinAt::SECTIONS, section, 0x30) + field);
}

static uint32_t Start(const std::vector<uint8_t>& skin, uint32_t section) {
    return uint32_t(Field(skin, section, SectionAt::LEVEL)) << 16 | Field(skin, section, SectionAt::INDEX_START);
}

// The indices of a submesh's range, Level included.
static std::vector<uint16_t> RangeOf(const std::vector<uint8_t>& skin, uint32_t section) {
    std::vector<uint16_t> indices = U16sAt(skin, SkinAt::INDICES);
    uint32_t start = Start(skin, section);
    uint32_t end = std::min<uint32_t>(start + Field(skin, section, SectionAt::INDEX_COUNT), static_cast<uint32_t>(indices.size()));
    return start < end ? std::vector<uint16_t>(indices.begin() + start, indices.begin() + end) : std::vector<uint16_t>();
}

// Submesh s (id FIRST_ID + s) holds ranges[s], each index naming skin vertex s; unused indices are GAP. Every skin
// vertex is model vertex first, and batch s (tagged s in its second byte) draws submesh s.
static std::vector<uint8_t> LeveledSkin(const std::vector<Range>& ranges, uint32_t first = 0) {
    uint32_t total = 0;
    for (const Range& range : ranges) {
        total = std::max(total, range.start + range.count);
    }

    std::vector<uint8_t> indices(total * 2);
    for (uint32_t i = 0; i < total; i++) {
        PutU16(indices, i * 2, GAP);
    }
    std::vector<uint8_t> sections;
    std::vector<uint8_t> batches;
    std::vector<uint8_t> lookup;
    for (uint16_t s = 0; s < ranges.size(); s++) {
        for (uint32_t i = ranges[s].start; i < ranges[s].start + ranges[s].count; i++) {
            PutU16(indices, i * 2, s);
        }
        std::vector<uint8_t> section = SkinSection(1, 0, 1, ranges[s].count, static_cast<uint16_t>(ranges[s].start >> 16));
        PutU16(section, SectionAt::ID, FIRST_ID + s);
        PutU16(section, SectionAt::INDEX_START, static_cast<uint16_t>(ranges[s].start));
        std::vector<uint8_t> batch = SkinBatch(0, 0, 1);
        batch[1] = static_cast<uint8_t>(s);
        PutU16(batch, 0x04, s);
        sections.insert(sections.end(), section.begin(), section.end());
        batches.insert(batches.end(), batch.begin(), batch.end());
        lookup.insert(lookup.end(), { static_cast<uint8_t>(first), static_cast<uint8_t>(first >> 8) });
    }

    std::vector<uint8_t> skin = NewSkin();
    uint32_t count = static_cast<uint32_t>(ranges.size());
    AddArray(skin, SkinAt::VERTICES, count, lookup);
    AddArray(skin, SkinAt::BONE_PROPERTIES, count, std::vector<uint8_t>(count * 4, 0));
    AddArray(skin, SkinAt::INDICES, total, indices);
    AddArray(skin, SkinAt::SECTIONS, count, sections);
    AddArray(skin, SkinAt::BATCHES, count, batches);
    return skin;
}

// A view of 65,535 + 65,535 + 3 corners: whatever goes last, the others pass 65,535 together.
static std::vector<uint8_t> TooLargeSkin() {
    return LeveledSkin({ { 0, 65535 }, { 65535, 65535 }, { 131070, 3 } });
}

// What submesh s of the source became: the laid out submesh with its id.
static uint32_t Moved(const std::vector<uint8_t>& skin, uint32_t s) {
    for (uint32_t i = 0; i < GetU32(skin, SkinAt::SECTIONS); i++) {
        if (Field(skin, i, SectionAt::ID) == FIRST_ID + s) {
            return i;
        }
    }
    return 0xFFFF;
}

static bool SameRanges(const std::vector<uint8_t>& before, const std::vector<uint8_t>& after) {
    bool same = GetU32(before, SkinAt::SECTIONS) == GetU32(after, SkinAt::SECTIONS);
    for (uint32_t s = 0; same && s < GetU32(before, SkinAt::SECTIONS); s++) {
        same = Moved(after, s) != 0xFFFF && RangeOf(before, s) == RangeOf(after, Moved(after, s));
    }
    return same;
}

// Laid out one after another from index 0, Level 0; an empty submesh starts at 0.
static bool Packed(const std::vector<uint8_t>& skin) {
    uint32_t next = 0;
    bool packed = true;
    for (uint32_t s = 0; s < GetU32(skin, SkinAt::SECTIONS); s++) {
        uint32_t start = Field(skin, s, SectionAt::INDEX_COUNT) ? next : 0;
        packed &= Field(skin, s, SectionAt::LEVEL) == 0 && Field(skin, s, SectionAt::INDEX_START) == start;
        next += Field(skin, s, SectionAt::INDEX_COUNT);
    }
    return packed && next == GetU32(skin, SkinAt::INDICES);
}

static uint16_t BatchSection(const std::vector<uint8_t>& skin, uint8_t tag) {
    for (uint32_t b = 0; b < GetU32(skin, SkinAt::BATCHES); b++) {
        size_t at = RecordAt(skin, SkinAt::BATCHES, b, 0x18);
        if (skin[at + 1] == tag) {
            return GetU16(skin, at + 0x04);
        }
    }
    return 0xFFFF;
}

TEST(LevelIsTheHighHalfOfASubmeshesStart) {
    std::vector<uint8_t> skin = LeveledSkin({ { 0, 3 }, { 65538, 3 } });

    CHECK(Retail::SectionIndexStart(skin, RecordAt(skin, SkinAt::SECTIONS, 1, 0x30)) == 65538);
    CHECK(Retail::SectionCounts(skin) == std::vector<uint32_t>({ 3, 3 }));
}

TEST(OnlyTheLastSubmeshMayStartPast65535) {
    CHECK(Retail::SectionsFit({ 65535, 3 }));
    CHECK(Retail::SectionsFit({ 65532, 3, 65535 }));
    // The largest goes last when submesh order does not fit.
    CHECK(Retail::SectionsFit({ 65535, 3, 3 }));
    CHECK(!Retail::SectionsFit({ 65535, 65535, 3 }));
}

TEST(SkinsWithoutLevelThatFitAreLeftAsTheyAre) {
    std::vector<uint8_t> skin = LeveledSkin({ { 0, 6 }, { 6, 3 } });
    std::vector<uint8_t> laid = skin;
    SkinLayout layout;
    std::string error;

    CHECK(LayOutSections(laid, MODEL, layout, error));
    CHECK(laid == skin && !layout.changed);
}

TEST(SkinsWithoutSubmeshesAreLeftAsTheyAre) {
    std::vector<uint8_t> skin = NewSkin();
    std::vector<uint8_t> laid = skin;
    SkinLayout layout;
    std::string error;

    CHECK(LayOutSections(laid, MODEL, layout, error));
    CHECK(laid == skin && !layout.changed);
}

TEST(LeveledRangesAreLaidOutInSubmeshOrderWhenThatFits) {
    std::vector<uint8_t> skin = LeveledSkin({ { 0, 3 }, { 65538, 3 } });
    std::vector<uint8_t> laid = skin;
    SkinLayout layout;
    std::string error;

    CHECK(LayOutSections(laid, MODEL, layout, error));
    CHECK(layout.changed && layout.leveled == 1 && !layout.reordered);
    CHECK(Moved(laid, 0) == 0 && Moved(laid, 1) == 1);
    CHECK(Packed(laid) && SameRanges(skin, laid));
}

TEST(TheLargestSubmeshGoesLastWhenSubmeshOrderDoesNotFit) {
    std::vector<uint8_t> skin = LeveledSkin({ { 0, 65535 }, { 65535, 3 }, { 65538, 3 } });
    std::vector<uint8_t> laid = skin;
    SkinLayout layout;
    std::string error;

    CHECK(LayOutSections(laid, MODEL, layout, error));
    CHECK(layout.reordered && layout.leveled == 1);
    CHECK(Moved(laid, 1) == 0 && Moved(laid, 2) == 1 && Moved(laid, 0) == 2);
    CHECK(Packed(laid) && SameRanges(skin, laid));
    // Batches keep their order and follow their submesh.
    CHECK(BatchSection(laid, 0) == 2 && BatchSection(laid, 1) == 0 && BatchSection(laid, 2) == 1);
}

TEST(TheClientsStartsFollowSubmeshOrderNotTheFiles) {
    // Every start fits its 16-bit field, but in submesh order the third lands at 65,538.
    std::vector<uint8_t> skin = LeveledSkin({ { 6, 65535 }, { 0, 3 }, { 3, 3 } });
    std::vector<uint8_t> laid = skin;
    SkinLayout layout;
    std::string error;

    CHECK(LayOutSections(laid, MODEL, layout, error));
    CHECK(layout.reordered && layout.leveled == 0 && Packed(laid) && SameRanges(skin, laid));
}

// A model of a columns by columns grid of one-bone vertices in the plane z = 0, UVs equal to the position.
static std::vector<uint8_t> GridModel(uint16_t columns) {
    std::vector<uint8_t> md20 = MinimalMd20();
    std::vector<uint8_t> records;
    for (uint16_t y = 0; y < columns; y++) {
        for (uint16_t x = 0; x < columns; x++) {
            std::vector<uint8_t> vertex = Vertex(255, 0, 0, 0);
            float position[] = { float(x), float(y), 0.0f, 1.0f };   // The last is the normal's z.
            std::memcpy(vertex.data(), position, 12);
            std::memcpy(vertex.data() + 0x1C, &position[3], 4);
            std::memcpy(vertex.data() + 0x20, position, 8);
            records.insert(records.end(), vertex.begin(), vertex.end());
        }
    }
    AddArray(md20, M2At::VERTICES, columns * columns, records);
    return md20;
}

// Two triangles per cell of the grid, as skin indices into an identity lookup.
static std::vector<uint16_t> GridCorners(uint16_t columns) {
    std::vector<uint16_t> corners;
    for (uint16_t y = 0; y + 1 < columns; y++) {
        for (uint16_t x = 0; x + 1 < columns; x++) {
            uint16_t at = y * columns + x;
            for (uint16_t corner : { at, uint16_t(at + 1), uint16_t(at + columns), uint16_t(at + 1), uint16_t(at + columns + 1), uint16_t(at + columns) }) {
                corners.push_back(corner);
            }
        }
    }
    return corners;
}

TEST(OtherSubmeshesPast65535TogetherLoseTrianglesByEdgeCollapse) {
    // 65,535 corners, then two grids of 10,952 triangles: 59 too many for the 21,845 the others may hold.
    constexpr uint16_t COLUMNS = 75;
    std::vector<uint16_t> grid = GridCorners(COLUMNS);
    uint16_t gridCount = static_cast<uint16_t>(grid.size());
    std::vector<uint8_t> skin = LeveledSkin({ { 0, 65535 }, { 65535, gridCount }, { 65535u + gridCount, gridCount } });
    std::vector<uint8_t> lookup(COLUMNS * COLUMNS * 2);
    for (uint32_t v = 0; v < COLUMNS * COLUMNS; v++) {
        PutU16(lookup, v * 2, static_cast<uint16_t>(v));
    }
    AddArray(skin, SkinAt::VERTICES, COLUMNS * COLUMNS, lookup);
    size_t indices = GetU32(skin, SkinAt::INDICES + 4);
    for (uint32_t grids = 0; grids < 2; grids++) {
        for (size_t c = 0; c < grid.size(); c++) {
            PutU16(skin, indices + (65535 + grids * grid.size() + c) * 2, grid[c]);
        }
    }
    std::vector<uint8_t> laid = skin;
    SkinLayout layout;
    std::string error;

    CHECK(LayOutSections(laid, GridModel(COLUMNS), layout, error));
    CHECK(layout.reduced.size() == 2 && layout.reduced[0].section == 1 && layout.reduced[0].before == 10952);
    CHECK(layout.reduced[0].after <= 10922 && layout.reduced[1].after <= 10922 && layout.reduced[0].after > 10900);
    CHECK(Packed(laid) && Moved(laid, 0) == 2 && RangeOf(laid, 2) == RangeOf(skin, 0));
    CHECK(Retail::SectionsFit(Retail::SectionCounts(laid)));
}

TEST(RangesPastTheIndicesAreRefused) {
    std::vector<uint8_t> skin = LeveledSkin({ { 0, 3 }, { 65538, 3 } });
    PutU32(skin, SkinAt::INDICES, 65539);
    SkinLayout layout;
    std::string error;

    CHECK(!LayOutSections(skin, MODEL, layout, error) && error.find("indices") != std::string::npos);
}

// A model of ten one-bone vertices whose SFID lists the view, then the LOD skins.
static std::vector<uint8_t> ModelWithLods(uint32_t lods) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(md20, M2At::VERTICES, 10, Concat(std::vector<std::vector<uint8_t>>(10, Vertex(255, 0, 0, 0))));
    AddArray(md20, M2At::TEXTURE_COMBOS, 1, Concat({ { 0, 0 } }));
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 0, 0 } }));
    std::vector<uint32_t> ids = { VIEW_SKIN };
    for (uint32_t l = 0; l < lods; l++) {
        ids.push_back(FIRST_LOD + l);
    }
    return Md21File(md20, { Chunk("SFID", U32s(ids)) });
}

// A LOD skin of one small submesh whose vertices start at first in the model.
static std::vector<uint8_t> LodSkin(uint32_t first) {
    std::vector<uint8_t> skin = LeveledSkin({ { 0, 3 } });
    PutU32(skin, SkinAt::BONE_COUNT_MAX, first);
    return skin;
}

using Files = std::map<uint32_t, std::vector<uint8_t>>;

static Retail::FileReader ReaderOf(const Files& files) {
    return [files](uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
        auto file = files.find(id);
        if (file == files.end()) {
            error = "no file " + std::to_string(id);
            return false;
        }
        bytes = file->second;
        return true;
    };
}

static ConvertedModel ModelOf(const std::vector<uint8_t>& file, const Files& files) {
    Retail::ModelSources sources;
    sources.read = ReaderOf(files);
    ConvertedModel model;
    std::string error;
    if (!Retail::ConvertModel(file, sources, model, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return model;
}

static bool Warned(const ConvertedModel& model, std::vector<const char*> parts) {
    return std::any_of(model.warnings.begin(), model.warnings.end(), [&](const std::string& warning) {
        return std::all_of(parts.begin(), parts.end(), [&](const char* part) { return warning.find(part) != std::string::npos; });
    });
}

static bool ConvertView(const std::vector<uint8_t>& file, const Files& files, uint32_t view, uint32_t skin, std::vector<uint8_t>& out, std::string& error) {
    return Retail::ConvertSkin({ files.at(skin), file, view, ReaderOf(files) }, out, error);
}

TEST(LeveledSkinsConvertLaidOut) {
    std::vector<uint8_t> file = ModelWithLods(0);
    Files files = { { VIEW_SKIN, LeveledSkin({ { 0, 65535 }, { 65535, 3 }, { 65538, 3 } }) } };
    std::vector<uint8_t> out;
    std::string error;

    CHECK(ConvertView(file, files, 0, VIEW_SKIN, out, error));
    CHECK(Packed(out) && SameRanges(files[VIEW_SKIN], out) && Moved(out, 0) == 2);
    CHECK(Warned(ModelOf(file, files), { "skin 0:", "1 submesh", "Level", "largest last" }));
}

TEST(ViewsThatFitKeepTheLodSkinsOut) {
    std::vector<uint8_t> file = ModelWithLods(1);
    Files files = { { VIEW_SKIN, LeveledSkin({ { 0, 3 } }) }, { FIRST_LOD, LodSkin(5) } };

    CHECK(ModelOf(file, files).skins == std::vector<uint32_t>({ VIEW_SKIN }));
}

TEST(LodSkinsTheClientCanDrawReplaceViewsItCannot) {
    std::vector<uint8_t> file = ModelWithLods(2);
    Files files = { { VIEW_SKIN, TooLargeSkin() }, { FIRST_LOD, LodSkin(5) }, { FIRST_LOD + 1, LodSkin(7) } };
    ConvertedModel model = ModelOf(file, files);
    std::vector<uint8_t> out;
    std::string error;

    CHECK(model.skins == std::vector<uint32_t>({ FIRST_LOD, FIRST_LOD + 1 }));
    CHECK(GetU32(model.m2, M2At::VIEWS) == 2);
    CHECK(Warned(model, { "view 0 (7000)", "131073 corners", "LOD skins 1, 2 (7001, 7002)" }));

    // A LOD skin's vertices count from its band's start in the model.
    CHECK(ConvertView(file, files, 1, FIRST_LOD + 1, out, error));
    CHECK(U16sAt(out, SkinAt::VERTICES) == std::vector<uint16_t>({ 7 }));
    CHECK(GetU32(out, SkinAt::BONE_COUNT_MAX) == 21);
}

TEST(LodSkinsPast16BitVerticesAreLeftOut) {
    std::vector<uint8_t> file = ModelWithLods(2);
    Files files = { { VIEW_SKIN, TooLargeSkin() }, { FIRST_LOD, LodSkin(65536) }, { FIRST_LOD + 1, LodSkin(7) } };
    ConvertedModel model = ModelOf(file, files);

    CHECK(model.skins == std::vector<uint32_t>({ FIRST_LOD + 1 }));
    CHECK(Warned(model, { "LOD skin 1 (7001)", "65,535" }));
}

TEST(AtMostFourViewsAreKept) {
    std::vector<uint8_t> file = ModelWithLods(5);
    Files files = { { VIEW_SKIN, TooLargeSkin() } };
    for (uint32_t l = 0; l < 5; l++) {
        files[FIRST_LOD + l] = LodSkin(1);
    }

    CHECK(ModelOf(file, files).skins == std::vector<uint32_t>({ FIRST_LOD, FIRST_LOD + 1, FIRST_LOD + 2, FIRST_LOD + 3 }));
}

TEST(ModelsWithNoDrawableSkinKeepTheFirstFourAndReduceThem) {
    std::vector<uint8_t> file = ModelWithLods(4);
    Files files = { { VIEW_SKIN, TooLargeSkin() } };
    for (uint32_t l = 0; l < 4; l++) {
        files[FIRST_LOD + l] = TooLargeSkin();
    }
    ConvertedModel model = ModelOf(file, files);
    std::vector<uint8_t> out;
    std::string error;

    CHECK(model.skins == std::vector<uint32_t>({ VIEW_SKIN, FIRST_LOD, FIRST_LOD + 1, FIRST_LOD + 2 }));
    CHECK(Warned(model, { "no view or LOD skin of the model fits", "lose triangles by edge collapse" }));
    // The second submesh's triangles name one vertex three times; the first ties for largest and comes first.
    CHECK(Warned(model, { "skin 0:", "1 submesh lost triangles", "submesh 1 from 21845 to 0", "21845 degenerate removed" }));
    CHECK(ConvertView(file, files, 0, VIEW_SKIN, out, error));
    CHECK(Packed(out) && Retail::SectionCounts(out) == std::vector<uint32_t>({ 65535, 0, 3 }));
}

// Every view's skin as the model conversion keeps it is the one its own conversion gives.
static bool KeepsEveryViewsSkin(const std::vector<uint8_t>& file, const Files& files) {
    ConvertedModel model = ModelOf(file, files);
    if (model.skins.empty() || model.skinBytes.size() != model.skins.size()) {
        return false;
    }

    for (uint32_t view = 0; view < model.skins.size(); view++) {
        std::vector<uint8_t> out;
        std::string error;
        if (!ConvertView(file, files, view, model.skins[view], out, error) || model.skinBytes[view] != out) {
            return false;
        }
    }
    return true;
}

TEST(ModelsKeepTheSkinsTheirViewsConvertTo) {
    Files reduced = { { VIEW_SKIN, TooLargeSkin() } };
    for (uint32_t l = 0; l < 4; l++) {
        reduced[FIRST_LOD + l] = TooLargeSkin();
    }

    CHECK(KeepsEveryViewsSkin(ModelWithLods(0), { { VIEW_SKIN, LeveledSkin({ { 0, 65535 }, { 65535, 3 }, { 65538, 3 } }) } }));
    CHECK(KeepsEveryViewsSkin(ModelWithLods(2), { { VIEW_SKIN, TooLargeSkin() }, { FIRST_LOD, LodSkin(5) }, { FIRST_LOD + 1, LodSkin(7) } }));
    CHECK(KeepsEveryViewsSkin(ModelWithLods(4), reduced));
}
