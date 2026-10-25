#include "WoW/Source/Ui/RetailModelFrame_Scripts.h"

// Not in the original. Model frame methods the retail browser previews with, added to the PlayerModel and DressUpModel
// method tables when retail assets are configured. 3.3.5 model frames show a unit, a cached creature or an item from
// the item cache; a retail id the server does not know reaches none of them.
//   model:SetDisplayInfo(displayId): a CreatureDisplayInfo id, as SetCreature shows a creature's first display. On a
//     DressUpModel a display with an Extra row is dressed and baked as the client draws that NPC.
//   dressUpModel:TryOnDisplay(displayId, inventoryType [, sheatheType [, classId [, subclassId]]]): an ItemDisplayInfo
//     id worn in the slot of inventoryType, as TryOn wears an item.

#if defined(WOWDECOMP_RETAIL_ASSETS)
#include <map>
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/CreatureStats_C.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Ui/DressUpModelFrame.h"

// The stats each frame shows a display through. A frame keeps the pointer for ShowThis, so they live for the session.
static std::map<CGCharacterModelBase*, CreatureStats_C*> s_displayStats;

static int32_t Script_SetDisplayInfo(lua_State* L) {
    CGCharacterModelBase* model = static_cast<CGCharacterModelBase*>(FrameScript_GetObjectThis(L, CGCharacterModelBase::GetObjectType()));
    if (!lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: SetDisplayInfo(displayID)");
        return 0;
    }
    CreatureStats_C*& stats = s_displayStats[model];
    if (!stats) {
        stats = new CreatureStats_C();
    }
    stats->m_displayId[0] = static_cast<uint32_t>(lua_tointeger(L, 2));
    model->SetCreature(stats);
    return 0;
}

// The component slot of an armour inventory type, false for a hand item or a slot nothing is drawn in.
static bool ArmourSlot(int32_t inventoryType, ITEM_SLOT& slot) {
    static const std::map<int32_t, int32_t> slots = { { 1, 0 }, { 3, 1 }, { 4, 2 }, { 5, 3 }, { 20, 3 }, { 6, 4 }, { 7, 5 }, { 8, 6 },
        { 9, 7 }, { 10, 8 }, { 19, 9 }, { 16, 10 } };
    auto found = slots.find(inventoryType);
    if (found == slots.end()) {
        return false;
    }
    slot = static_cast<ITEM_SLOT>(found->second);
    return true;
}

// Main hand 15, off hand 16 (shields, off-hand weapons and held items); 0 for no hand item.
static int32_t HandOf(int32_t inventoryType) {
    switch (inventoryType) {
        case 13: case 15: case 17: case 21: case 25: case 26:
            return 15;
        case 14: case 22: case 23:
            return 16;
        default:
            return 0;
    }
}

static void TryOnHandDisplay(CGDressUpModelFrame* frame, const ItemDisplayInfoRec& display, int32_t hand, const CGUnitVirtualItem& item) {
    CGUnitVirtualItem& worn = hand == 15 ? frame->m_mainHand : frame->m_offHand;
    if (worn.m_classID) {
        CCharacterComponent::RemoveHandItem(frame->m_model, static_cast<INVENTORY_SLOTS>(hand), static_cast<SHEATHE_TYPE>(worn.m_sheatheType),
            worn.m_inventoryType == 14);
    }
    worn = item;
    CCharacterComponent::AddHandItem(frame->m_model, &display, static_cast<INVENTORY_SLOTS>(hand), static_cast<SHEATHE_TYPE>(item.m_sheatheType), false,
        item.m_inventoryType == 14, item.m_inventoryType == 26, 0);
}

static int32_t Script_TryOnDisplay(lua_State* L) {
    CGDressUpModelFrame* frame = static_cast<CGDressUpModelFrame*>(FrameScript_GetObjectThis(L, CGDressUpModelFrame::GetObjectType()));
    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        luaL_error(L, "Usage: TryOnDisplay(displayID, inventoryType [, sheatheType [, classID [, subclassID]]])");
        return 0;
    }
    int32_t displayId = static_cast<int32_t>(lua_tointeger(L, 2));
    int32_t inventoryType = static_cast<int32_t>(lua_tointeger(L, 3));
    ItemDisplayInfoRec display;
    if (!frame->m_model || !g_itemDisplayInfoDB.GetRecord(displayId, &display)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    ITEM_SLOT slot;
    if (ArmourSlot(inventoryType, slot) && frame->m_component) {
        frame->m_component->RemoveItem(slot);
        frame->m_component->AddItem(slot, displayId, 0);
        lua_pushboolean(L, 1);
        return 1;
    }
    int32_t hand = HandOf(inventoryType);
    if (!hand) {
        lua_pushboolean(L, 0);
        return 1;
    }
    CGUnitVirtualItem item = {};
    item.m_inventoryType = static_cast<uint8_t>(inventoryType);
    item.m_sheatheType = static_cast<uint8_t>(lua_isnumber(L, 4) ? lua_tointeger(L, 4) : 0);
    item.m_classID = static_cast<uint8_t>(lua_isnumber(L, 5) ? lua_tointeger(L, 5) : 2);
    item.m_subclassID = static_cast<uint8_t>(lua_isnumber(L, 6) ? lua_tointeger(L, 6) : 0);
    TryOnHandDisplay(frame, display, hand, item);
    lua_pushboolean(L, 1);
    return 1;
}

static FrameScript_Method s_modelMethods[] = {
    { "SetDisplayInfo", &Script_SetDisplayInfo },
};

static FrameScript_Method s_dressUpMethods[] = {
    { "SetDisplayInfo", &Script_SetDisplayInfo },
    { "TryOnDisplay", &Script_TryOnDisplay },
};

// Adds methods to the __index table of a frame type's metatable (FrameScript_Object::CreateScriptMetaTable).
static void AddMethods(int32_t metatable, FrameScript_Method* methods, int32_t count) {
    lua_State* L = FrameScript_GetContext();
    lua_rawgeti(L, LUA_REGISTRYINDEX, metatable);
    if (lua_istable(L, -1)) {
        lua_pushstring(L, "__index");
        lua_rawget(L, -2);
        if (lua_istable(L, -1)) {
            FrameScript_Object::FillScriptMethodTable(L, methods, count);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void RetailModelFramesRegister() {
    AddMethods(CGCharacterModelBase::s_metatable, s_modelMethods, sizeof(s_modelMethods) / sizeof(s_modelMethods[0]));
    AddMethods(CGDressUpModelFrame::s_metatable, s_dressUpMethods, sizeof(s_dressUpMethods) / sizeof(s_dressUpMethods[0]));
}

#else

void RetailModelFramesRegister() {
}

#endif
