#include "Engine/Source/Retail/CreatureSkins.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"
#include "Tools/Tests/Wdc5Builder.h"

using Retail::CreatureSkin;
using Retail::CreatureSkins;
using Retail::Db2Table;
using Retail::SkinTexture;

// A retail creature model leaves its skin slots (texture types 11 to 13, and 5 for retail's fourth variation) to
// CreatureDisplayInfo. The client loads a named file for any texture until a display replaces it (M2Shared.cpp,
// M2Model.cpp ReplaceTexture), so the skin most displays show is named in those slots and their types are kept.

constexpr uint32_t BOAR = 7939217;
constexpr size_t DISPLAY_COLUMNS = 27;   // ModelID is column 1; TextureVariationFileDataID[4] is column 27.

using Row = std::pair<uint32_t, std::vector<uint32_t>>;   // Id, then every column's values in order.

// A table of plain 32-bit columns; widths[c] is how many values column c holds.
static Db2Table PlainTable(uint32_t layout, const std::vector<uint32_t>& widths, const std::vector<Row>& rows) {
    Wdc5Spec spec;
    spec.layoutHash = layout;
    uint16_t offset = 0;
    for (uint32_t width : widths) {
        spec.columns.push_back({ 0, offset, static_cast<uint16_t>(width * 32), 0, 0, { 0, 0, 0 } });
        offset = static_cast<uint16_t>(offset + width * 32);
    }
    spec.recordSize = offset / 8;

    Wdc5Section section;
    for (const Row& row : rows) {
        section.ids.push_back(row.first);
        for (uint32_t value : row.second) {
            AppendU32(section.records, value);
        }
    }
    spec.sections = { section };

    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error));
    return table;
}

// CreatureModelData rows: id and the model's FileDataID (column 2).
static Db2Table ModelData(const std::vector<std::pair<uint32_t, uint32_t>>& models, uint32_t layout = Retail::CREATURE_MODEL_DATA_LAYOUT) {
    std::vector<Row> rows;
    for (const auto& model : models) {
        rows.push_back({ model.first, { 0, 0, model.second } });
    }
    return PlainTable(layout, { 1, 1, 1 }, rows);
}

struct Display {
    uint32_t id;
    uint32_t model;
    std::vector<uint32_t> variations;   // Four texture FileDataIDs, 0 for none.
};

static Db2Table DisplayInfo(const std::vector<Display>& displays, uint32_t layout = Retail::CREATURE_DISPLAY_INFO_LAYOUT) {
    std::vector<uint32_t> widths(DISPLAY_COLUMNS, 1);
    widths.push_back(4);
    std::vector<Row> rows;
    for (const Display& display : displays) {
        std::vector<uint32_t> values(DISPLAY_COLUMNS, 0);
        values[1] = display.model;
        values.insert(values.end(), display.variations.begin(), display.variations.end());
        rows.push_back({ display.id, values });
    }
    return PlainTable(layout, widths, rows);
}

static CreatureSkins Loaded(const std::vector<Display>& displays) {
    CreatureSkins skins;
    std::string error;
    if (!skins.Load(ModelData({ { 40, BOAR }, { 41, 1001 } }), DisplayInfo(displays), error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return skins;
}

TEST(TheDefaultIsTheSkinMostDisplaysShow) {
    CreatureSkins skins = Loaded({ { 10, 40, { 500, 0, 0, 0 } }, { 11, 40, { 600, 0, 0, 0 } }, { 12, 40, { 600, 0, 0, 0 } } });
    const CreatureSkin* skin = skins.Default(BOAR);

    CHECK(skin != nullptr);
    if (skin) {
        CHECK(skin->displayId == 11 && skin->displays == 2 && skin->others == 1);
        CHECK(skin->textures == std::vector<SkinTexture>({ { 11, 600 } }));
    }
}

TEST(TiesGoToTheLowestDisplay) {
    CreatureSkins skins = Loaded({ { 21, 40, { 700, 0, 0, 0 } }, { 20, 40, { 800, 0, 0, 0 } } });
    const CreatureSkin* skin = skins.Default(BOAR);

    CHECK(skin && skin->displayId == 20 && skin->textures == std::vector<SkinTexture>({ { 11, 800 } }));
}

TEST(VariationsFillTypes11To13And5) {
    CreatureSkins skins = Loaded({ { 10, 40, { 1, 0, 3, 4 } } });
    const CreatureSkin* skin = skins.Default(BOAR);

    CHECK(skin && skin->textures == std::vector<SkinTexture>({ { 11, 1 }, { 13, 3 }, { 5, 4 } }));
}

TEST(ModelsWithoutDressedDisplaysHaveNoDefault) {
    // Display 13 names no texture, display 14 a model CreatureModelData lacks.
    CreatureSkins skins = Loaded({ { 13, 41, { 0, 0, 0, 0 } }, { 14, 99, { 900, 0, 0, 0 } } });

    CHECK(skins.Default(1001) == nullptr);
    CHECK(skins.Default(BOAR) == nullptr);
}

TEST(UnknownLayoutsAreRefused) {
    CreatureSkins skins;
    std::string error;

    CHECK(!skins.Load(ModelData({ { 40, BOAR } }, 0x1111), DisplayInfo({}), error) && error.find("CreatureModelData") != std::string::npos);
    CHECK(!skins.Load(ModelData({ { 40, BOAR } }), DisplayInfo({}, 0x2222), error) && error.find("CreatureDisplayInfo") != std::string::npos);
}

// A model with a file texture named by TXID, then skin slots 11, 12, an object skin (2), 5 and 13.
static Retail::ConvertedModel Dressed(const CreatureSkin* skin) {
    std::vector<uint8_t> md20 = MinimalMd20();
    AddArray(md20, M2At::TEXTURES, 6, Concat({ Texture(0), Texture(11), Texture(12), Texture(2), Texture(5), Texture(13) }));
    Retail::ModelSources sources;
    sources.skin = skin;
    Retail::ConvertedModel model;
    std::string error;
    if (!Retail::ConvertModel(Md21File(md20, { Chunk("TXID", U32s({ 775194, 0, 0, 0, 0, 0 })) }), sources, model, error)) {
        std::printf("    (%s)\n", error.c_str());
        model.m2 = NewMd20();
    }
    return model;
}

static std::pair<uint32_t, std::string> TextureAt(const std::vector<uint8_t>& m2, size_t index) {
    size_t at = RecordAt(m2, M2At::TEXTURES, index, 0x10);
    return { GetU32(m2, at), StringAt(m2, at + 8) };
}

static bool Warned(const Retail::ConvertedModel& model, std::vector<const char*> parts) {
    for (const std::string& warning : model.warnings) {
        bool all = true;
        for (const char* part : parts) {
            all &= warning.find(part) != std::string::npos;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

TEST(SkinSlotsNameTheDefaultSkinAndKeepTheirTypes) {
    CreatureSkin skin;
    skin.displayId = 31;
    skin.displays = 4;
    skin.others = 2;
    skin.textures = { { 11, 7001 }, { 12, 7002 }, { 5, 7004 } };
    Retail::ConvertedModel model = Dressed(&skin);

    CHECK(TextureAt(model.m2, 0) == std::make_pair(uint32_t(0), std::string("retail/775194.blp")));
    CHECK(TextureAt(model.m2, 1) == std::make_pair(uint32_t(11), std::string("retail/7001.blp")));
    CHECK(TextureAt(model.m2, 2) == std::make_pair(uint32_t(12), std::string("retail/7002.blp")));
    CHECK(TextureAt(model.m2, 3) == std::make_pair(uint32_t(2), std::string()));
    CHECK(TextureAt(model.m2, 4) == std::make_pair(uint32_t(5), std::string("retail/7004.blp")));
    // The skin has no third variation.
    CHECK(TextureAt(model.m2, 5) == std::make_pair(uint32_t(13), std::string()));
    CHECK(Warned(model, { "textures 1, 2, 4", "display 31", "4 displays", "2 other skins" }));
}

TEST(ModelsWithoutADefaultSkinLeaveTheSlotsUnnamed) {
    Retail::ConvertedModel model = Dressed(nullptr);

    CHECK(TextureAt(model.m2, 1) == std::make_pair(uint32_t(11), std::string()));
    CHECK(TextureAt(model.m2, 4) == std::make_pair(uint32_t(5), std::string()));
    CHECK(!Warned(model, { "display" }));
}
