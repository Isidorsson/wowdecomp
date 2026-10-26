#include <map>
#include "Engine/Source/Retail/DisplayAudit.h"
#include "Tools/Tests/Check.h"

using Retail::AuditDisplay;
using Retail::AuditFinding;
using Retail::AuditFlag;
using Retail::NpcExtra;
using Retail::RetailDisplay;
using Retail::RetailItemDisplay;

// The audit names what a served display uses that the client cannot draw, so a fix can be chosen by how many
// displays it repairs. It reads rows only: the same mesh in another colour is the same finding.

constexpr size_t HELM = 0;
constexpr size_t SHOULDERS = 1;
constexpr size_t CHEST = 3;
constexpr size_t BELT = 4;

using Findings = std::vector<AuditFinding>;

static RetailDisplay NpcDisplay(uint32_t extraId) {
    RetailDisplay display;
    display.id = 128688;
    display.extendedDisplayId = extraId;
    return display;
}

static NpcExtra BakedExtra() {
    NpcExtra extra;
    extra.id = 160609;
    extra.sdBake = 6795669;
    return extra;
}

static RetailItemDisplay ModelItem(uint32_t id, uint32_t model) {
    RetailItemDisplay item;
    item.id = id;
    item.models[0] = model;
    return item;
}

struct Items {
    std::map<uint32_t, RetailItemDisplay> byId;

    Retail::ItemDisplayLookup Lookup() const {
        return [this](uint32_t id) -> const RetailItemDisplay* {
            auto found = this->byId.find(id);
            return found == this->byId.end() ? nullptr : &found->second;
        };
    }
};

TEST(APlainCreatureWithNothingDroppedHasNoFindings) {
    RetailDisplay display;
    display.id = 144739;

    CHECK(AuditDisplay(display, nullptr, Items().Lookup()).empty());
}

// Umbric (128688): his chest, belt, legs and gloves name collection models the client never attaches.
TEST(ChestAndBeltModelsAreDroppedArmour) {
    Items items;
    items.byId[699535] = ModelItem(699535, 6758314);
    items.byId[699536] = ModelItem(699536, 6758400);
    NpcExtra extra = BakedExtra();
    extra.items[CHEST] = 699535;
    extra.items[BELT] = 699536;

    Findings findings = AuditDisplay(NpcDisplay(160609), &extra, items.Lookup());

    CHECK(findings == Findings({ { AuditFlag::ArmourModelDropped, CHEST }, { AuditFlag::ArmourModelDropped, BELT } }));
}

TEST(HelmAndShoulderModelsAreDrawnSoNotFlagged) {
    Items items;
    items.byId[1] = ModelItem(1, 100);
    items.byId[2] = ModelItem(2, 200);
    NpcExtra extra = BakedExtra();
    extra.items[HELM] = 1;
    extra.items[SHOULDERS] = 2;

    CHECK(AuditDisplay(NpcDisplay(160609), &extra, items.Lookup()).empty());
}

TEST(ATextureOnlyChestIsNotFlagged) {
    Items items;
    items.byId[5] = RetailItemDisplay();
    items.byId[5].id = 5;
    items.byId[5].skins[0] = 999;
    NpcExtra extra = BakedExtra();
    extra.items[CHEST] = 5;

    CHECK(AuditDisplay(NpcDisplay(160609), &extra, items.Lookup()).empty());
}

TEST(AnExtraWithoutAnyBakeIsFlagged) {
    NpcExtra extra;
    extra.id = 160609;

    CHECK(AuditDisplay(NpcDisplay(160609), &extra, Items().Lookup()) == Findings({ { AuditFlag::NoBake, 0 } }));
}

TEST(AnExtraTheTablesLackIsFlaggedWithItsId) {
    CHECK(AuditDisplay(NpcDisplay(160609), nullptr, Items().Lookup()) == Findings({ { AuditFlag::MissingExtra, 160609 } }));
}

TEST(AnItemTheTablesLackIsFlaggedWithItsSlot) {
    NpcExtra extra = BakedExtra();
    extra.items[CHEST] = 424242;

    CHECK(AuditDisplay(NpcDisplay(160609), &extra, Items().Lookup()) == Findings({ { AuditFlag::MissingItem, CHEST } }));
}

// Retail's later body sections (accessory, scalp) have no 3.3.5 column; each is reported once, whichever items use it.
TEST(DroppedBodySectionsAreReportedOncePerSection) {
    Items items;
    items.byId[5] = RetailItemDisplay();
    items.byId[5].droppedSections = (1u << 8) | (1u << 9);
    items.byId[6] = RetailItemDisplay();
    items.byId[6].droppedSections = 1u << 9;
    NpcExtra extra = BakedExtra();
    extra.items[CHEST] = 5;
    extra.items[BELT] = 6;

    Findings findings = AuditDisplay(NpcDisplay(160609), &extra, items.Lookup());

    CHECK(findings == Findings({ { AuditFlag::BodySectionDropped, 8 }, { AuditFlag::BodySectionDropped, 9 } }));
}

TEST(DroppedGeosetGroupsAreReportedOncePerGroup) {
    RetailDisplay display;
    display.droppedGeosets = { { 9, 1 }, { 9, 2 }, { 12, 1 } };

    Findings findings = AuditDisplay(display, nullptr, Items().Lookup());

    CHECK(findings == Findings({ { AuditFlag::GeosetDropped, 9 }, { AuditFlag::GeosetDropped, 12 } }));
}

TEST(EveryFlagHasAName) {
    CHECK(std::string(Retail::AuditFlagName(AuditFlag::ArmourModelDropped)) == "ArmourModelDropped");
    CHECK(std::string(Retail::AuditFlagName(AuditFlag::GeosetDropped)) == "GeosetDropped");
}
