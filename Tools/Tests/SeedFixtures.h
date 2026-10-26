#ifndef TOOLS_TESTS_SEEDFIXTURES_H
#define TOOLS_TESTS_SEEDFIXTURES_H

#include <map>
#include <set>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Engine/Source/Retail/BakeBlp.h"
#include "Engine/Source/Retail/BodyMatch.h"
#include "Engine/Source/Retail/CreatureTables.h"
#include "Engine/Source/Retail/HelmFit.h"
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailNpcDisplay.h"
#include "Engine/Source/Retail/RetailRaces.h"
#include "Tools/Tests/BodyFixtures.h"
#include "Tools/Tests/CreatureFixtures.h"
#include "Tools/Tests/ItemFixtures.h"
#include "Tools/Tests/LocalSourceFixture.h"
#include "Tools/Tests/M2Builder.h"
#include "Tools/Tests/NpcFixtures.h"
#include "Tools/Tests/RetailModelFixtures.h"

// A small retail install for the seed export: every original and DB2 table by FileDataID, as a local install's
// decoded cache would hold them. The ids are the fixture's own; only the table and body ids the client fixes are real.

struct RetailWorld {
    std::map<uint32_t, std::vector<uint8_t>> files;
    std::set<uint32_t> tables;   // The ids among files that are DB2 tables.
};

// A beast: one view, two embedded textures (TXID), a walk sequence in an .anim file (AFID) and two variations.
constexpr uint32_t BEAST_DISPLAY = 70001;
constexpr uint32_t BEAST_MODEL_ROW = 6001;
constexpr uint32_t BEAST_MODEL = 3100000;
constexpr uint32_t BEAST_SKIN = 3100001;
constexpr uint32_t BEAST_BODY_TEXTURE = 3100010;
constexpr uint32_t BEAST_HORN_TEXTURE = 3100011;
constexpr uint32_t BEAST_ANIM = 3100020;
constexpr uint32_t BEAST_COAT = 3100030;
constexpr uint32_t BEAST_MANE = 3100031;
constexpr uint32_t BEAST_BELLY = 3100032;   // The display's fourth variation, which retail keeps beside the three of the client's row.

// A wolf on a skeleton (SKID) that has a parent, each with an .anim, and a skin slot a default skin fills. Its display
// shows pelt A; two displays that are not exported show pelt B, so pelt B is the model's default skin, which the
// converted model names.
constexpr uint32_t WOLF_DISPLAY = 70004;
constexpr uint32_t WOLF_ELDER_DISPLAY = 70005;
constexpr uint32_t WOLF_PUP_DISPLAY = 70006;
constexpr uint32_t WOLF_MODEL_ROW = 6004;
constexpr uint32_t WOLF_MODEL = 3200000;
constexpr uint32_t WOLF_SKIN = 3200001;
constexpr uint32_t WOLF_EYES = 3200010;
constexpr uint32_t WOLF_SKELETON = 3200020;
constexpr uint32_t WOLF_PARENT_SKELETON = 3200021;
constexpr uint32_t WOLF_ANIM = 3200030;
constexpr uint32_t WOLF_PARENT_ANIM = 3200031;
constexpr uint32_t WOLF_PELT_A = 3200040;
constexpr uint32_t WOLF_PELT_B = 3200041;

// A Worgen NPC, a race the installed client has no body for: it is drawn on its retail body (a model on a skeleton,
// converted as an NPC body) with its SD bake and one customization texture.
constexpr uint32_t BODY_DISPLAY = 70007;
constexpr uint32_t BODY_MODEL_ROW = 6007;
constexpr uint32_t BODY_MODEL = 3400000;
constexpr uint32_t BODY_SKIN = 3400001;
constexpr uint32_t BODY_EXTRA = 80003;
constexpr uint32_t BODY_BAKE_MATERIAL = 502;
constexpr uint32_t BODY_BAKE = 3400010;
constexpr uint32_t BODY_LOOK_MATERIAL = 601;
constexpr uint32_t BODY_LOOK = 3400020;
constexpr uint32_t WORGEN = 22;

// A human male NPC (CreatureDisplayInfoExtra) on the installed body, baked with one SD texture. Its display names a
// retail model that is never drawn: the installed body is.
constexpr uint32_t NPC_DISPLAY = 70002;
constexpr uint32_t NPC_MODEL_ROW = 6002;
constexpr uint32_t NPC_MODEL = 3100100;
constexpr uint32_t NPC_EXTRA = 80001;
constexpr uint32_t NPC_BAKE_MATERIAL = 500;
constexpr uint32_t NPC_BAKE = 3100110;
constexpr uint32_t HUMAN = 1;
constexpr uint32_t MALE = 0;

// A second human male NPC with an HD bake only: it is warped onto the installed body, which is matched to the retail HD
// body of the race and gender (a model and a skin).
constexpr uint32_t WARP_DISPLAY = 70003;
constexpr uint32_t WARP_EXTRA = 80002;
constexpr uint32_t HD_BAKE_MATERIAL = 501;
constexpr uint32_t HD_BAKE = 3100120;
constexpr uint32_t HD_BODY_SKIN = 3100130;
constexpr uint32_t HELM_SLOT = 0;
constexpr uint32_t SHOULDER_SLOT = 1;

// An armour display with a helm, an icon and body textures, and a pair of shoulders. The helm has a file for each race
// and gender of the stock races, found through its model resource; the human male file is the display's model.
constexpr uint32_t ARMOUR_DISPLAY = 90001;
constexpr uint32_t HELM_RESOURCE = 700;
constexpr uint32_t HELM_SKIN_MATERIAL = 800;
constexpr uint32_t HELM_SKIN = 4100001;
constexpr uint32_t HELM_MODEL_SKIN = 4100030;
constexpr uint32_t HELM_MODEL_TEXTURE = 4100031;
constexpr uint32_t CHEST_MATERIAL = 801;   // Torso upper, a file each for men and women.
constexpr uint32_t CHEST_MALE = 4100010;
constexpr uint32_t CHEST_FEMALE = 4100011;
constexpr uint32_t SLEEVE_MATERIAL = 802;  // Arm upper, one file for both.
constexpr uint32_t SLEEVE_BOTH = 4100012;
constexpr uint32_t ARMOUR_ICON = 4100020;
constexpr uint32_t SHOULDER_DISPLAY = 90002;
constexpr uint32_t SHOULDER_RESOURCE = 701;
constexpr uint32_t SHOULDER_SKIN_MATERIAL = 803;
constexpr uint32_t SHOULDER_LEFT = 4300000;
constexpr uint32_t SHOULDER_RIGHT = 4300001;
constexpr uint32_t SHOULDER_MODEL_SKIN = 4300002;
constexpr uint32_t SHOULDER_SKIN = 4300010;
constexpr uint32_t LAST_STOCK_RACE = 11;
constexpr uint32_t TORSO_UPPER = 3;
constexpr uint32_t ARM_UPPER = 0;

inline uint32_t HelmFile(uint32_t race, uint32_t gender) {
    return 4200000 + race * 10 + gender;
}

// The rows the tables are built from, gathered by the part of the world that adds them.
struct WorldRows {
    std::vector<CreatureModelRow> models;
    std::vector<CreatureDisplayRow> displays;
    NpcRows npc;
    std::vector<TextureFileRow> textures;
    std::vector<std::pair<uint32_t, uint32_t>> modelFiles;   // File, model resource.
    std::vector<ComponentModelRow> componentModels;
    std::vector<ItemDisplayRow> itemDisplays;
    std::vector<AppearanceRow> appearances;
    std::vector<SectionRow> sections;
    std::vector<ComponentTextureRow> componentTextures;
};

inline void AddBeast(RetailWorld& world, WorldRows& rows) {
    world.files[BEAST_MODEL] = RetailModel({ { BEAST_SKIN }, { BEAST_BODY_TEXTURE, BEAST_HORN_TEXTURE }, BEAST_ANIM });
    world.files[BEAST_SKIN] = RetailSkin();
    world.files[BEAST_ANIM] = WalkAnim();
    for (uint32_t texture : { BEAST_BODY_TEXTURE, BEAST_HORN_TEXTURE, BEAST_COAT, BEAST_MANE, BEAST_BELLY }) {
        world.files[texture] = Blob("BLP2", texture);
    }
    rows.models.push_back({ BEAST_MODEL_ROW, BEAST_MODEL });
    rows.displays.push_back({ BEAST_DISPLAY, BEAST_MODEL_ROW, { BEAST_COAT, BEAST_MANE, 0, BEAST_BELLY } });
}

inline void AddTable(RetailWorld& world, uint32_t id, const std::vector<uint8_t>& bytes) {
    world.files[id] = bytes;
    world.tables.insert(id);
}

// The wolf's skeleton walks with its own .anim and takes the run sequence, and its .anim, from the parent skeleton.
inline void AddWolf(RetailWorld& world, WorldRows& rows) {
    SkeletonParts child;
    child.parent = WOLF_PARENT_SKELETON;
    child.sequences = SkelSequences({ Sequence(0, 0x20), Sequence(4, 0) }, { 0 });
    child.bones = SkelBones({ { 1, -2 } });
    child.afid = AfidRow(4, 0, WOLF_ANIM);
    SkeletonParts parent;
    parent.sequences = SkelSequences({ Sequence(0, 0x20), Sequence(4, 0x20), Sequence(5, 0) }, {});
    parent.bones = SkelBones({ { 10, 20, -30 } });
    parent.afid = AfidRow(5, 0, WOLF_PARENT_ANIM);

    world.files[WOLF_MODEL] = RetailModel({ { WOLF_SKIN }, { WOLF_EYES }, 0, WOLF_SKELETON, true });
    world.files[WOLF_SKIN] = RetailSkin();
    world.files[WOLF_SKELETON] = SkeletonFile(child);
    world.files[WOLF_PARENT_SKELETON] = SkeletonFile(parent);
    world.files[WOLF_ANIM] = Chunk("AFSB", SkelAnimPart(2));
    world.files[WOLF_PARENT_ANIM] = Chunk("AFSB", SkelAnimPart(30));
    for (uint32_t texture : { WOLF_EYES, WOLF_PELT_A, WOLF_PELT_B }) {
        world.files[texture] = Blob("BLP2", texture);
    }
    rows.models.push_back({ WOLF_MODEL_ROW, WOLF_MODEL });
    rows.displays.push_back({ WOLF_DISPLAY, WOLF_MODEL_ROW, { WOLF_PELT_A } });
    rows.displays.push_back({ WOLF_ELDER_DISPLAY, WOLF_MODEL_ROW, { WOLF_PELT_B } });
    rows.displays.push_back({ WOLF_PUP_DISPLAY, WOLF_MODEL_ROW, { WOLF_PELT_B } });
}

// The retail body hangs on the wolf's skeleton, which the wolf's part of the world adds.
inline void AddRetailBodyNpc(RetailWorld& world, WorldRows& rows) {
    world.files[BODY_MODEL] = RetailModel({ { BODY_SKIN }, {}, 0, WOLF_SKELETON, false });
    world.files[BODY_SKIN] = RetailSkin();
    world.files[BODY_BAKE] = Blob("BLP2", BODY_BAKE);
    world.files[BODY_LOOK] = Blob("BLP2", BODY_LOOK);
    rows.models.push_back({ BODY_MODEL_ROW, BODY_MODEL });
    rows.displays.push_back({ BODY_DISPLAY, BODY_MODEL_ROW, {}, 1, 255, 0, BODY_EXTRA });
    rows.npc.extras.push_back({ BODY_EXTRA, WORGEN, MALE, 1, 0, BODY_BAKE_MATERIAL, 0 });
    rows.textures.push_back({ BODY_BAKE, 0, BODY_BAKE_MATERIAL });
    // One chosen option whose element shows a material of target 25, a file of the resource BODY_LOOK_MATERIAL.
    rows.npc.options.push_back({ 1, BODY_EXTRA, 9, 100 });
    rows.npc.choices.push_back({ 100, 0 });
    rows.npc.elements.push_back({ 1000, 100, 0, 0, 600 });
    rows.npc.materials.push_back({ 600, 25, BODY_LOOK_MATERIAL });
    rows.textures.push_back({ BODY_LOOK, 0, BODY_LOOK_MATERIAL });
}

// One file per stock race and gender, each with the retail body the client fits a helm to the stock head from.
inline void AddHelm(RetailWorld& world, WorldRows& rows) {
    world.files[HELM_MODEL_SKIN] = RetailSkin();
    world.files[HELM_MODEL_TEXTURE] = Blob("BLP2", HELM_MODEL_TEXTURE);
    for (uint32_t race = 1; race <= LAST_STOCK_RACE; race++) {
        for (uint32_t gender = 0; gender <= 1; gender++) {
            uint32_t file = HelmFile(race, gender);
            world.files[file] = RetailModel({ { HELM_MODEL_SKIN }, { HELM_MODEL_TEXTURE }, 0 });
            rows.modelFiles.push_back({ file, HELM_RESOURCE });
            rows.componentModels.push_back({ file, gender, race, 0 });
            if (uint32_t body = Retail::RetailBodyFileDataId(race, gender)) {
                world.files[body] = Blob("BODY", body);
            }
        }
    }
}

inline void AddArmour(RetailWorld& world, WorldRows& rows) {
    AddHelm(world, rows);
    for (uint32_t texture : { HELM_SKIN, CHEST_MALE, CHEST_FEMALE, SLEEVE_BOTH, ARMOUR_ICON, SHOULDER_SKIN }) {
        world.files[texture] = Blob("BLP2", texture);
    }
    rows.itemDisplays.push_back({ ARMOUR_DISPLAY, { HELM_RESOURCE }, { HELM_SKIN_MATERIAL }, {}, { 7, 8 } });
    rows.textures.push_back({ HELM_SKIN, 0, HELM_SKIN_MATERIAL });
    rows.textures.push_back({ CHEST_MALE, 0, CHEST_MATERIAL });
    rows.textures.push_back({ CHEST_FEMALE, 0, CHEST_MATERIAL });
    rows.textures.push_back({ SLEEVE_BOTH, 0, SLEEVE_MATERIAL });
    rows.componentTextures.push_back({ CHEST_MALE, 0, 0 });
    rows.componentTextures.push_back({ CHEST_FEMALE, 1, 0 });
    rows.sections.push_back({ ARMOUR_DISPLAY, TORSO_UPPER, CHEST_MATERIAL });
    rows.sections.push_back({ ARMOUR_DISPLAY, ARM_UPPER, SLEEVE_MATERIAL });
    rows.appearances.push_back({ 1, ARMOUR_DISPLAY, ARMOUR_ICON });

    world.files[SHOULDER_MODEL_SKIN] = RetailSkin();
    for (uint32_t shoulder : { SHOULDER_LEFT, SHOULDER_RIGHT }) {
        world.files[shoulder] = RetailModel({ { SHOULDER_MODEL_SKIN }, {}, 0 });
        rows.modelFiles.push_back({ shoulder, SHOULDER_RESOURCE });
    }
    rows.componentModels.push_back({ SHOULDER_LEFT, 2, 0, 0 });
    rows.componentModels.push_back({ SHOULDER_RIGHT, 2, 0, 1 });
    rows.itemDisplays.push_back({ SHOULDER_DISPLAY, { SHOULDER_RESOURCE, SHOULDER_RESOURCE }, { SHOULDER_SKIN_MATERIAL, SHOULDER_SKIN_MATERIAL }, {}, {} });
    rows.textures.push_back({ SHOULDER_SKIN, 0, SHOULDER_SKIN_MATERIAL });
}

inline void AddNpc(RetailWorld& world, WorldRows& rows) {
    world.files[NPC_MODEL] = Blob("BODY", NPC_MODEL);
    world.files[NPC_BAKE] = Blob("BLP2", NPC_BAKE);
    rows.models.push_back({ NPC_MODEL_ROW, NPC_MODEL });
    rows.displays.push_back({ NPC_DISPLAY, NPC_MODEL_ROW, {}, 1, 255, 0, NPC_EXTRA });
    rows.npc.extras.push_back({ NPC_EXTRA, HUMAN, MALE, 1, 0, NPC_BAKE_MATERIAL, 0 });
    rows.npc.items.push_back({ NPC_EXTRA, ARMOUR_DISPLAY, HELM_SLOT });
    rows.npc.items.push_back({ NPC_EXTRA, SHOULDER_DISPLAY, SHOULDER_SLOT });
    rows.textures.push_back({ NPC_BAKE, 0, NPC_BAKE_MATERIAL });
}

inline std::vector<BodyVertex> BodySquare(float uScale) {
    return { { 0, 0, 0, 0, 0 }, { 1, 0, 0, uScale, 0 }, { 0, 1, 0, 0, 1 }, { 1, 1, 0, uScale, 1 } };
}

inline Retail::Image BakePattern() {
    Retail::Image image;
    image.width = 1024;
    image.height = 512;
    for (uint32_t y = 0; y < image.height; y++) {
        for (uint32_t x = 0; x < image.width; x++) {
            image.texels.push_back({ static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(x >> 8), 255 });
        }
    }
    return image;
}

inline void AddWarpedNpc(RetailWorld& world, WorldRows& rows) {
    world.files[Retail::RetailHdBodyFileDataId(HUMAN, MALE)] = Concat({ BodyModelMd21(BodySquare(0.5f)), Chunk("SFID", U32s({ HD_BODY_SKIN })) });
    world.files[HD_BODY_SKIN] = BodySkin({ { { 0, 1, 2 }, { 1, 3, 2 } } });
    world.files[HD_BAKE] = Retail::EncodeRawBlp(BakePattern());
    rows.displays.push_back({ WARP_DISPLAY, NPC_MODEL_ROW, {}, 1, 255, 0, WARP_EXTRA });
    rows.npc.extras.push_back({ WARP_EXTRA, HUMAN, MALE, 1, 0, 0, HD_BAKE_MATERIAL });
    rows.textures.push_back({ HD_BAKE, 0, HD_BAKE_MATERIAL });
}

// The client's installed human male body, which the HD bake is matched against and warped onto.
inline void InstallHumanMale(Retail::CascSource& source) {
    source.SetStockReader([](const std::string& path, std::vector<uint8_t>& bytes) {
        if (path == Retail::InstalledBodyPath(HUMAN, MALE)) {
            bytes = BodyModel(BodySquare(1.0f), { 1 });
        } else if (path == Retail::InstalledBodySkinPath(HUMAN, MALE)) {
            bytes = BodySkin({ { { 0, 1, 2 }, { 1, 3, 2 } } });
        }
        return !bytes.empty();
    });
}

// The tables the display loaders read, empty where no fixture display needs a row (their layouts still load).
inline void AddTables(RetailWorld& world, const WorldRows& rows) {
    AddTable(world, Retail::CREATURE_MODEL_DATA_FDID, CreatureModels(rows.models).bytes);
    AddTable(world, Retail::CREATURE_DISPLAY_INFO_FDID, CreatureDisplays(rows.displays).bytes);
    AddTable(world, Retail::CREATURE_DISPLAY_INFO_GEOSET_DATA_FDID, DisplayGeosets({}).bytes);
    AddTable(world, Retail::ANIMATION_DATA_FDID, PlainTable(Retail::ANIMATION_DATA_LAYOUT, { 1 }, { { 4, { 0 } } }).bytes);
    AddTable(world, Retail::MODEL_FILE_DATA_FDID, ModelFiles(rows.modelFiles).bytes);
    AddTable(world, Retail::ITEM_DISPLAY_INFO_FDID, ItemDisplays(rows.itemDisplays).bytes);
    AddTable(world, Retail::ITEM_DISPLAY_INFO_MODEL_MAT_RES_FDID, PlainTable(Retail::ITEM_DISPLAY_INFO_MODEL_MAT_RES_LAYOUT, { 1, 1, 1 }, {}).bytes);
    AddTable(world, Retail::TEXTURE_FILE_DATA_FDID, TextureFiles(rows.textures).bytes);
    AddTable(world, Retail::COMPONENT_MODEL_FILE_DATA_FDID, ComponentModels(rows.componentModels).bytes);
    AddTable(world, Retail::ITEM_APPEARANCE_FDID, Appearances(rows.appearances).bytes);
    AddTable(world, Retail::ITEM_DISPLAY_INFO_MATERIAL_RES_FDID, Sections(rows.sections).bytes);
    AddTable(world, Retail::COMPONENT_TEXTURE_FILE_DATA_FDID, ComponentTextures(rows.componentTextures).bytes);
    AddTable(world, Retail::HELMET_GEOSET_DATA_FDID, HelmetGeosets({}).bytes);

    NpcTableSet npc = BuildNpcTables(rows.npc);
    AddTable(world, Retail::CREATURE_DISPLAY_INFO_EXTRA_FDID, npc.extra.bytes);
    AddTable(world, Retail::NPC_MODEL_ITEM_SLOT_DISPLAY_INFO_FDID, npc.itemSlots.bytes);
    AddTable(world, Retail::CREATURE_DISPLAY_INFO_OPTION_FDID, npc.options.bytes);
    AddTable(world, Retail::CHR_CUSTOMIZATION_CHOICE_FDID, npc.choices.bytes);
    AddTable(world, Retail::CHR_CUSTOMIZATION_ELEMENT_FDID, npc.elements.bytes);
    AddTable(world, Retail::CHR_CUSTOMIZATION_GEOSET_FDID, npc.geosets.bytes);
    AddTable(world, Retail::CHR_CUSTOMIZATION_MATERIAL_FDID, npc.materials.bytes);

    AddTable(world, Retail::CHR_RACES_FDID, PlainTable(Retail::CHR_RACES_LAYOUT, { 1, 1, 1 }, {}).bytes);
    AddTable(world, Retail::CHR_RACE_X_CHR_MODEL_FDID, PlainTable(Retail::CHR_RACE_X_CHR_MODEL_LAYOUT, { 1, 1, 1, 1 }, {}).bytes);
    AddTable(world, Retail::CHR_MODEL_FDID, PlainTable(Retail::CHR_MODEL_LAYOUT, { 1, 1, 1, 1, 1, 1 }, {}).bytes);
    AddTable(world, Retail::CHR_MODEL_TEXTURE_LAYER_FDID, PlainTable(Retail::CHR_MODEL_TEXTURE_LAYER_LAYOUT, { 1, 1, 1, 1, 1, 1, 1, 1 }, {}).bytes);
}

inline RetailWorld BuildWorld() {
    RetailWorld world;
    WorldRows rows;
    AddBeast(world, rows);
    AddWolf(world, rows);
    AddArmour(world, rows);
    AddNpc(world, rows);
    AddWarpedNpc(world, rows);
    AddRetailBodyNpc(world, rows);
    AddTables(world, rows);
    return world;
}

// Puts every original in the source's decoded cache.
inline bool Install(const LocalSource& local, const RetailWorld& world) {
    for (const auto& [id, bytes] : world.files) {
        if (!Cache(local, id, bytes)) {
            return false;
        }
    }
    return true;
}

#endif
