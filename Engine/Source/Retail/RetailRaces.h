#ifndef ENGINE_SOURCE_RETAIL_RETAILRACES_H
#define ENGINE_SOURCE_RETAIL_RETAILRACES_H

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"

namespace Retail {

// Retail races the installed ChrRaces lacks (Worgen, Pandaren, the allied races, Earthen, ...): their NPCs draw on the
// retail body through the 3.3.5 character component (slice b), which needs a ChrRaces row for the race and tables
// sized for its id. A served row keeps the retail id and ClientPrefix (helm files), is hidden from character creation
// and names the race's retail body displays (ChrRaceXChrModel -> ChrModel.DisplayID). Installed rows always win.

constexpr uint32_t CHR_RACES_FDID = 1305311;
constexpr uint32_t CHR_RACES_LAYOUT = 0x4F44C796;
constexpr uint32_t CHR_RACE_X_CHR_MODEL_FDID = 3490304;
constexpr uint32_t CHR_RACE_X_CHR_MODEL_LAYOUT = 0xA203BC29;
constexpr uint32_t CHR_MODEL_FDID = 3384313;
constexpr uint32_t CHR_MODEL_LAYOUT = 0x03FAB755;
constexpr uint32_t CHR_MODEL_TEXTURE_LAYER_FDID = 3548976;
constexpr uint32_t CHR_MODEL_TEXTURE_LAYER_LAYOUT = 0xD0583FB4;

// ChrRaces flag 0x1: not a playable race (character creation and the race tooltips skip it).
constexpr int32_t RACE_HIDDEN_FROM_CREATION = 0x1;

struct RaceTables {
    const Db2Table& races;        // ChrRaces: ClientPrefix and Name.
    const Db2Table& raceModels;   // ChrRaceXChrModel: race, sex, ChrModel.
    const Db2Table& models;       // ChrModel: the body's display and texture layout.
    const Db2Table& layers;       // ChrModelTextureLayer: per layout, what each texture target draws on.
};

// A ChrModelTextureLayer row: a customization material of target is drawn on the model's textures of type, at layer.
struct TextureLayer {
    uint32_t type;
    uint32_t layer;
    uint32_t target;
};

struct RetailRace {
    uint32_t id = 0;
    std::string prefix;                     // ClientPrefix as retail writes it, "Kt".
    std::string name;
    std::array<uint32_t, 2> displays = {};  // The body's CreatureDisplayInfo id, male then female; 0 for none.
    std::array<uint32_t, 2> layouts = {};   // The body's CharComponentTextureLayout, male then female.
};

class RetailRaces {
    public:
    bool Load(const RaceTables& tables, std::string& error);

    // Null for an id retail lacks.
    const RetailRace* Race(uint32_t id) const;
    uint32_t MaxId() const;
    // The lowest race with that ClientPrefix, in any case (Earthen 84 and 85 share "Ed"); 0 for none.
    uint32_t RaceOfPrefix(const std::string& prefix) const;
    // A texture layout's layers, by id; none for an unknown layout.
    std::vector<TextureLayer> TextureLayers(uint32_t layoutId) const;

    private:
    std::vector<RetailRace> m_races;   // By id.
    std::map<uint32_t, std::vector<TextureLayer>> m_layers;   // By layout.
};

// What a served ChrRaces row holds.
struct ServedRace {
    uint32_t id = 0;
    int32_t flags = 0;
    std::string prefix;
    std::string name;
    std::array<uint32_t, 2> displays = {};
};

// False for a race the installed table already has (an id up to its highest).
bool ServeRace(const RetailRace& race, uint32_t installedMaxId, ServedRace& served);

// The highest race id the client's race-sized tables (CharSections variations, facial hair counts) must hold.
uint32_t RaceCapacity(uint32_t installedMaxId, uint32_t servedMaxId);
// Whether race * 2 + sex indexes tables of that capacity.
bool RaceFitsTables(uint32_t race, uint32_t sex, uint32_t capacity);

// Whether the character component needs the race's CharSections rows: an installed race always (its base skin and
// variations), a served race only without a bake, which then has no skin at all and is refused.
bool NeedsCharSections(bool installedRace, bool baked);

}

#endif
