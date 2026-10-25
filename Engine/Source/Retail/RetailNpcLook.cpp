#include "Engine/Source/Retail/RetailNpcLook.h"

namespace Retail {

constexpr uint32_t HAIR = 0;
constexpr uint32_t BEARD = 1;
constexpr uint32_t SIDEBURNS = 2;
constexpr uint32_t MOUSTACHE = 3;
constexpr uint32_t HORNS = 24;
// The installed facial hair columns: beard, moustache, sideburns.
constexpr uint32_t FACIAL_GROUPS[3] = { BEARD, MOUSTACHE, SIDEBURNS };

static bool FindGeoset(const NpcLook& look, uint32_t type, uint32_t& id) {
    for (const CustomizationGeoset& geoset : look.geosets) {
        if (geoset.type == type) {
            id = geoset.id;
            return true;
        }
    }
    return false;
}

// The installed CharHairGeosets id the look's hair style shows: the Tauren horns one above, else the hair geoset.
static bool HairGeoset(const NpcLook& look, uint32_t& geoset) {
    if (FindGeoset(look, HORNS, geoset)) {
        geoset++;
        return true;
    }
    return FindGeoset(look, HAIR, geoset);
}

static void MapHairStyle(uint32_t race, uint32_t sex, const NpcLook& look, const InstalledLook& installed, NpcAppearance& appearance) {
    uint32_t geoset = 0;
    if (!HairGeoset(look, geoset)) {
        return;
    }
    for (const InstalledHairStyle& style : installed.hairStyles) {
        if (style.race == race && style.sex == sex && style.geoset == geoset) {
            appearance.hairStyle = style.variation;
            return;
        }
    }
    appearance.notes.push_back("race " + std::to_string(race) + " sex " + std::to_string(sex) + ": hair style geoset " + std::to_string(geoset)
        + " has no installed style, style 0 drawn");
}

// Retail 0 is no mesh, which the installed rows write as 0 or as 1 (the empty x01 mesh).
static bool SameFacialGeoset(uint32_t retail, uint32_t installed) {
    return retail == installed || (retail <= 1 && installed <= 1);
}

static void MapFacialHair(uint32_t race, uint32_t sex, const NpcLook& look, const InstalledLook& installed, NpcAppearance& appearance) {
    uint32_t wanted[3] = {};
    for (size_t group = 0; group < 3; group++) {
        FindGeoset(look, FACIAL_GROUPS[group], wanted[group]);
    }

    int best = -1;
    for (const InstalledFacialHair& row : installed.facialHair) {
        if (row.race != race || row.sex != sex) {
            continue;
        }
        int same = 0;
        for (size_t group = 0; group < 3; group++) {
            same += SameFacialGeoset(wanted[group], row.geosets[group]) ? 1 : 0;
        }
        if (same > best || (same == best && row.variation < appearance.facialHair)) {
            best = same;
            appearance.facialHair = row.variation;
        }
    }
    if (best >= 0 && best < 3) {
        appearance.notes.push_back("race " + std::to_string(race) + " sex " + std::to_string(sex) + ": facial hair (beard " + std::to_string(wanted[0])
            + ", moustache " + std::to_string(wanted[1]) + ", sideburns " + std::to_string(wanted[2]) + ") has no installed style, closest "
            + std::to_string(appearance.facialHair) + " drawn");
    }
}

NpcAppearance MapNpcLook(uint32_t race, uint32_t sex, const NpcLook& look, const InstalledLook& installed) {
    NpcAppearance appearance;
    MapHairStyle(race, sex, look, installed, appearance);
    MapFacialHair(race, sex, look, installed, appearance);
    appearance.hairColor = look.hasHairColor ? look.hairColorOrder : 0;
    return appearance;
}

NpcExtraFields ServedNpcExtra(const NpcExtra& extra, const NpcPlan& plan, const NpcAppearance& appearance, const ItemServable& servable,
    std::vector<std::string>& notes) {
    NpcExtraFields fields;
    fields.id = static_cast<int32_t>(SyntheticExtraId(extra.id));
    fields.race = static_cast<int32_t>(extra.race);
    fields.sex = static_cast<int32_t>(extra.sex);
    fields.hairStyle = static_cast<int32_t>(appearance.hairStyle);
    fields.hairColor = static_cast<int32_t>(appearance.hairColor);
    fields.facialHair = static_cast<int32_t>(appearance.facialHair);
    fields.bakeName = plan.bakeName;
    for (size_t slot = 0; slot < NPC_ITEM_SLOTS; slot++) {
        uint32_t item = extra.items[slot];
        if (item && servable(item)) {
            fields.items[slot] = static_cast<int32_t>(item);
        } else if (item) {
            notes.push_back("extra " + std::to_string(extra.id) + ": item display " + std::to_string(item) + " in slot " + std::to_string(slot)
                + " is not served, slot left empty");
        }
    }
    return fields;
}

}
