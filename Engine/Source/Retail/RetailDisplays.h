#ifndef ENGINE_SOURCE_RETAIL_RETAILDISPLAYS_H
#define ENGINE_SOURCE_RETAIL_RETAILDISPLAYS_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/CreatureTables.h"

namespace Retail {

// Retail creature displays, for the client to serve the display ids its own CreatureDisplayInfo lacks (stock rows
// win). Only what a 3.3.5 row can mean is kept: sound, blood, footprint, particle colour and extended display ids
// index retail tables the client does not have, so its rows leave them 0.

// Retail model data ids overlap stock ones, so a served display names its model by an id above every stock id.
constexpr uint32_t SYNTHETIC_MODEL_ID_BASE = 0x01000000;

uint32_t SyntheticModelId(uint32_t retailModelId);

// False for an id below the synthetic range.
bool RetailModelIdOf(uint32_t syntheticModelId, uint32_t& retailModelId);

// The name a served model row gives the client: "retail\<fdid>.m2". The backslash matters: ReplaceMonsterSkin
// finds the folder for the skin files after the last one.
std::string CreatureModelName(uint32_t fileDataId);

// A texture variation's name, which the client puts in the model's folder: "<fdid>", "" for none.
std::string VariationName(uint32_t fileDataId);

struct RetailCreatureModel {
    uint32_t id = 0;
    uint32_t fileDataId = 0;
    std::array<float, 6> geoBox = {};   // Min x, y, z, then max.
    float collisionWidth = 0;
    float collisionHeight = 0;
    float modelScale = 1;
    float mountHeight = 0;
    int32_t sizeClass = 0;
};

struct RetailDisplay {
    uint32_t id = 0;
    uint32_t modelId = 0;   // The retail CreatureModelData id.
    float scale = 1;
    uint32_t alpha = 255;
    int32_t sizeClass = 0;
    std::array<uint32_t, 3> variations = {};   // The first three texture variations; 3.3.5 has no fourth.
    uint32_t geosetData = 0;
};

// One nibble per geoset group 1 to 8, group g at bits 4 * (g - 1): the variant shown, 0xF (hidden) for a group
// retail does not list or lists past variant 15. 0, every variant drawn, for a display without geosets.
uint32_t PackGeosetData(const std::vector<Geoset>& geosets);

class RetailDisplays {
    public:
    bool Load(const CreatureTables& tables, std::string& error);

    // Null for an unknown id, and for a display whose model the tables lack.
    const RetailDisplay* Display(uint32_t id) const;
    const RetailCreatureModel* Model(uint32_t id) const;
    // Every served display, by id.
    const std::vector<RetailDisplay>& All() const { return this->m_displays; }

    private:
    std::vector<RetailDisplay> m_displays;      // By id.
    std::vector<RetailCreatureModel> m_models;  // By id.
};

}

#endif
