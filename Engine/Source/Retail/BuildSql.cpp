#include "Engine/Source/Retail/BuildSql.h"
#include <cstdio>
#include <set>

namespace Retail {

constexpr uint32_t INVENTORY_CHEST = 5;
constexpr uint32_t INVENTORY_ROBE = 20;

std::string SqlQuote(const std::string& text) {
    std::string quoted = "'";
    for (char c : text) {
        switch (c) {
            case '\'': quoted += "''"; break;
            case '\\': quoted += "\\\\"; break;
            case '\n': quoted += "\\n"; break;
            case '\r': quoted += "\\r"; break;
            case '\t': quoted += "\\t"; break;
            case '\0': quoted += "\\0"; break;
            case '\x1A': quoted += "\\Z"; break;
            default: quoted += c; break;
        }
    }
    return quoted + "'";
}

// SHOW COLUMNS FROM item_template / creature_template of the TrinityCore 3.3.5 world DB, in order.
const std::vector<std::string>& ItemTemplateColumns() {
    static const std::vector<std::string> columns = { "entry", "class", "subclass", "SoundOverrideSubclass", "name", "displayid", "Quality",
        "Flags", "FlagsExtra", "BuyCount", "BuyPrice", "SellPrice", "InventoryType", "AllowableClass", "AllowableRace", "ItemLevel",
        "RequiredLevel", "RequiredSkill", "RequiredSkillRank", "requiredspell", "requiredhonorrank", "RequiredCityRank",
        "RequiredReputationFaction", "RequiredReputationRank", "maxcount", "stackable", "ContainerSlots", "StatsCount", "stat_type1",
        "stat_value1", "stat_type2", "stat_value2", "stat_type3", "stat_value3", "stat_type4", "stat_value4", "stat_type5", "stat_value5",
        "stat_type6", "stat_value6", "stat_type7", "stat_value7", "stat_type8", "stat_value8", "stat_type9", "stat_value9", "stat_type10",
        "stat_value10", "ScalingStatDistribution", "ScalingStatValue", "dmg_min1", "dmg_max1", "dmg_type1", "dmg_min2", "dmg_max2",
        "dmg_type2", "armor", "holy_res", "fire_res", "nature_res", "frost_res", "shadow_res", "arcane_res", "delay", "ammo_type",
        "RangedModRange", "spellid_1", "spelltrigger_1", "spellcharges_1", "spellppmRate_1", "spellcooldown_1", "spellcategory_1",
        "spellcategorycooldown_1", "spellid_2", "spelltrigger_2", "spellcharges_2", "spellppmRate_2", "spellcooldown_2", "spellcategory_2",
        "spellcategorycooldown_2", "spellid_3", "spelltrigger_3", "spellcharges_3", "spellppmRate_3", "spellcooldown_3", "spellcategory_3",
        "spellcategorycooldown_3", "spellid_4", "spelltrigger_4", "spellcharges_4", "spellppmRate_4", "spellcooldown_4", "spellcategory_4",
        "spellcategorycooldown_4", "spellid_5", "spelltrigger_5", "spellcharges_5", "spellppmRate_5", "spellcooldown_5", "spellcategory_5",
        "spellcategorycooldown_5", "bonding", "description", "PageText", "LanguageID", "PageMaterial", "startquest", "lockid", "Material",
        "sheath", "RandomProperty", "RandomSuffix", "block", "itemset", "MaxDurability", "area", "Map", "BagFamily", "TotemCategory",
        "socketColor_1", "socketContent_1", "socketColor_2", "socketContent_2", "socketColor_3", "socketContent_3", "socketBonus",
        "GemProperties", "RequiredDisenchantSkill", "ArmorDamageModifier", "duration", "ItemLimitCategory", "HolidayId", "ScriptName",
        "DisenchantID", "FoodType", "minMoneyLoot", "maxMoneyLoot", "flagsCustom", "VerifiedBuild" };
    return columns;
}

const std::vector<std::string>& CreatureTemplateColumns() {
    static const std::vector<std::string> columns = { "entry", "difficulty_entry_1", "difficulty_entry_2", "difficulty_entry_3",
        "KillCredit1", "KillCredit2", "modelid1", "modelid2", "modelid3", "modelid4", "name", "subname", "IconName", "gossip_menu_id",
        "minlevel", "maxlevel", "exp", "faction", "npcflag", "speed_walk", "speed_run", "scale", "rank", "dmgschool", "BaseAttackTime",
        "RangeAttackTime", "BaseVariance", "RangeVariance", "unit_class", "unit_flags", "unit_flags2", "dynamicflags", "family", "type",
        "type_flags", "lootid", "pickpocketloot", "skinloot", "PetSpellDataId", "VehicleId", "mingold", "maxgold", "AIName",
        "MovementType", "HoverHeight", "HealthModifier", "ManaModifier", "ArmorModifier", "DamageModifier", "ExperienceModifier",
        "RacialLeader", "movementId", "RegenHealth", "mechanic_immune_mask", "spell_school_immune_mask", "flags_extra", "ScriptName",
        "StringId", "VerifiedBuild" };
    return columns;
}

static std::string Joined(const std::vector<std::string>& parts) {
    std::string joined;
    for (const std::string& part : parts) {
        joined += (joined.empty() ? "" : ", ") + part;
    }
    return joined;
}

std::string InsertFromBase(const std::string& table, const std::vector<std::string>& columns, uint32_t base,
    const std::map<std::string, std::string>& overrides, std::string& error) {
    std::set<std::string> known(columns.begin(), columns.end());
    for (const auto& [column, value] : overrides) {
        if (!known.count(column)) {
            error = table + " has no column " + column;
            return std::string();
        }
    }
    std::vector<std::string> names, values;
    for (const std::string& column : columns) {
        auto value = overrides.find(column);
        names.push_back("`" + column + "`");
        values.push_back(value != overrides.end() ? value->second : "`" + column + "`");
    }
    // Deleting the new entry first lets an export run again in place of the row it wrote before.
    auto entry = overrides.find("entry");
    std::string replace = entry != overrides.end() ? "DELETE FROM `" + table + "` WHERE `entry` = " + entry->second + ";\n" : "";
    return replace + "INSERT INTO `" + table + "` (" + Joined(names) + ")\nSELECT " + Joined(values) + "\nFROM `" + table +
        "` WHERE `entry` = " + std::to_string(base) + ";\n";
}

static std::string CommentLine(const std::string& comment) {
    std::string line = "-- ";
    for (char c : comment) {
        line += c == '\n' || c == '\r' ? ' ' : c;
    }
    return line + "\n";
}

static std::string Number(uint32_t value) {
    return std::to_string(value);
}

// A clean item: the base lends its shape (durability, material, sheath, sound, price), never a requirement, flag,
// binding, spell, set, quest, lock, script or limit, so a player of any level, class or race can use it as it is.
static void ClearBaseExtras(std::map<std::string, std::string>& set) {
    for (const char* column : { "Flags", "FlagsExtra", "flagsCustom", "RequiredSkill", "RequiredSkillRank", "requiredspell",
             "requiredhonorrank", "RequiredCityRank", "RequiredReputationFaction", "RequiredReputationRank", "maxcount", "bonding",
             "PageText", "startquest", "lockid", "RandomProperty", "RandomSuffix", "itemset", "area", "Map", "ItemLimitCategory",
             "duration", "HolidayId", "minMoneyLoot", "maxMoneyLoot" }) {
        set[column] = "0";
    }
    set["AllowableClass"] = set["AllowableRace"] = "-1";
    set["description"] = set["ScriptName"] = "''";
    for (int slot = 1; slot <= 5; slot++) {
        std::string n = std::to_string(slot);
        set["spellid_" + n] = set["spelltrigger_" + n] = set["spellcharges_" + n] = set["spellppmRate_" + n] = set["spellcategory_" + n] = "0";
        set["spellcooldown_" + n] = set["spellcategorycooldown_" + n] = "-1";
    }
}

std::string BuildItemSql(const ItemSql& item, std::string& error) {
    std::map<std::string, std::string> set = { { "entry", Number(item.entry) }, { "class", Number(item.classId) },
        { "subclass", Number(item.subclass) }, { "name", SqlQuote(item.name) }, { "displayid", Number(item.displayId) },
        { "Quality", Number(item.quality) }, { "InventoryType", Number(item.inventoryType) }, { "ItemLevel", Number(item.itemLevel) },
        { "StatsCount", Number(static_cast<uint32_t>(item.stats.stats.size())) }, { "ScalingStatDistribution", "0" },
        { "ScalingStatValue", "0" }, { "socketBonus", "0" }, { "RequiredLevel", Number(item.requiredLevel) } };
    ClearBaseExtras(set);
    for (size_t slot = 0; slot < CLASSIC_STAT_SLOTS; slot++) {
        bool filled = slot < item.stats.stats.size();
        set["stat_type" + std::to_string(slot + 1)] = filled ? Number(item.stats.stats[slot].first) : "0";
        set["stat_value" + std::to_string(slot + 1)] = filled ? std::to_string(item.stats.stats[slot].second) : "0";
    }
    for (size_t r = 0; r < item.stats.resistances.size(); r++) {
        set[ResistanceColumn(static_cast<Resistance>(r))] = std::to_string(item.stats.resistances[r]);
    }
    for (size_t socket = 0; socket < item.socketColors.size(); socket++) {
        set["socketColor_" + std::to_string(socket + 1)] = Number(item.socketColors[socket]);
    }
    if (item.armor >= 0) {
        set["armor"] = std::to_string(item.armor);
    }
    if (item.weapon) {
        set["dmg_min1"] = Number(item.damageMin);
        set["dmg_max1"] = Number(item.damageMax);
        set["dmg_type1"] = Number(item.damageType);
        set["dmg_min2"] = set["dmg_max2"] = set["dmg_type2"] = "0";
        set["delay"] = Number(item.delay);
    }
    std::string insert = InsertFromBase("item_template", ItemTemplateColumns(), item.base, set, error);
    return insert.empty() ? insert : CommentLine(item.comment) + insert;
}

std::string BuildCreatureSql(const CreatureSql& creature, std::string& error) {
    std::map<std::string, std::string> set = { { "entry", Number(creature.entry) }, { "difficulty_entry_1", "0" },
        { "difficulty_entry_2", "0" }, { "difficulty_entry_3", "0" }, { "name", SqlQuote(creature.name) },
        { "subname", SqlQuote(creature.subname) } };
    for (size_t slot = 0; slot < creature.models.size(); slot++) {
        set["modelid" + std::to_string(slot + 1)] = Number(creature.models[slot]);
    }
    if (creature.scale > 0) {
        char scale[32];
        std::snprintf(scale, sizeof(scale), "%g", creature.scale);
        set["scale"] = scale;
    }
    std::string insert = InsertFromBase("creature_template", CreatureTemplateColumns(), creature.base, set, error);
    return insert.empty() ? insert : CommentLine(creature.comment) + insert;
}

// From the world DB: per class, subclass and inventory type, the first epic of item level 200 to 245 without a
// spell, set, socket, scaling or reputation requirement, else the highest-level uncommon to epic without a spell.
struct BaseItem {
    uint32_t classId;
    uint32_t subclass;
    uint32_t inventoryType;
    uint32_t entry;
};

static const BaseItem BASE_ITEMS[] = {
    { 2, 0, 13, 40402 }, { 2, 0, 21, 43897 }, { 2, 0, 22, 42226 }, { 2, 1, 17, 37852 }, { 2, 2, 15, 42489 }, { 2, 3, 26, 42484 },
    { 2, 4, 13, 42274 }, { 2, 4, 21, 41383 }, { 2, 4, 22, 42279 }, { 2, 5, 17, 39245 }, { 2, 6, 17, 42326 }, { 2, 7, 13, 37401 },
    { 2, 7, 21, 45129 }, { 2, 7, 22, 42289 }, { 2, 8, 17, 40343 }, { 2, 10, 17, 37883 }, { 2, 13, 13, 36570 }, { 2, 13, 21, 42259 },
    { 2, 13, 22, 42264 }, { 2, 14, 13, 12866 }, { 2, 14, 17, 41755 }, { 2, 15, 13, 42241 }, { 2, 15, 21, 42345 }, { 2, 15, 22, 42247 },
    { 2, 16, 25, 42449 }, { 2, 18, 26, 42494 }, { 2, 19, 26, 34146 },
    { 4, 0, 1, 20591 }, { 4, 0, 2, 39246 }, { 4, 0, 11, 37784 }, { 4, 0, 12, 42128 }, { 4, 0, 19, 45983 }, { 4, 0, 23, 38322 },
    { 4, 1, 1, 45435 }, { 4, 1, 3, 45253 }, { 4, 1, 5, 24687 }, { 4, 1, 6, 40561 }, { 4, 1, 7, 40560 }, { 4, 1, 8, 45258 },
    { 4, 1, 9, 41878 }, { 4, 1, 10, 42103 }, { 4, 1, 16, 39225 }, { 4, 1, 20, 42100 },
    { 4, 2, 1, 50679 }, { 4, 2, 3, 43481 }, { 4, 2, 5, 50656 }, { 4, 2, 6, 43484 }, { 4, 2, 7, 43495 }, { 4, 2, 8, 40519 },
    { 4, 2, 9, 41332 }, { 4, 2, 10, 40541 }, { 4, 2, 20, 49786 },
    { 4, 3, 1, 50605 }, { 4, 3, 3, 40438 }, { 4, 3, 5, 43461 }, { 4, 3, 6, 40275 }, { 4, 3, 7, 43458 }, { 4, 3, 8, 43469 },
    { 4, 3, 9, 41047 }, { 4, 3, 10, 45109 },
    { 4, 4, 1, 39723 }, { 4, 4, 3, 40334 }, { 4, 4, 5, 39239 }, { 4, 4, 6, 37241 }, { 4, 4, 7, 40240 }, { 4, 4, 8, 39139 },
    { 4, 4, 9, 37620 }, { 4, 4, 10, 37363 },
    { 4, 6, 14, 40266 }, { 4, 7, 28, 42612 }, { 4, 10, 28, 39715 },
};

static uint32_t FindBaseItem(uint32_t classId, uint32_t subclass, uint32_t inventoryType) {
    for (const BaseItem& item : BASE_ITEMS) {
        if (item.classId == classId && item.subclass == subclass && item.inventoryType == inventoryType) {
            return item.entry;
        }
    }
    return 0;
}

uint32_t SuggestedBaseItem(uint32_t classId, uint32_t subclass, uint32_t inventoryType) {
    uint32_t entry = FindBaseItem(classId, subclass, inventoryType);
    if (!entry && (inventoryType == INVENTORY_CHEST || inventoryType == INVENTORY_ROBE)) {
        entry = FindBaseItem(classId, subclass, inventoryType == INVENTORY_CHEST ? INVENTORY_ROBE : INVENTORY_CHEST);
    }
    return entry;
}

void ClassicItemClass(uint32_t& classId, uint32_t& subclass, std::string& note) {
    if (classId == 2 && subclass == 9) {
        subclass = 7;
        note = "warglaives are not on 3.3.5: one-hand sword";
    } else if (classId == 4 && (subclass == 5 || subclass == 11)) {
        note = std::string(subclass == 5 ? "cosmetic" : "relic") + " armour is not on 3.3.5: miscellaneous";
        subclass = 0;
    }
}

}
