#include "Engine/Source/Retail/RetailRaces.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace Retail {

// Physical columns of the layouts (WoWDBDefs).
constexpr size_t RACE_CLIENT_PREFIX = 0;
constexpr size_t RACE_NAME = 2;
constexpr size_t RACE_MODEL_RACE = 0;
constexpr size_t RACE_MODEL_MODEL = 1;
constexpr size_t RACE_MODEL_SEX = 2;
constexpr size_t MODEL_DISPLAY = 4;
constexpr size_t MODEL_LAYOUT = 5;
constexpr size_t LAYER_TYPE = 0;
constexpr size_t LAYER_LAYER = 1;
constexpr size_t LAYER_TARGETS = 7;

static std::string Lowercase(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// A ChrModel's display and texture layout.
using ModelBody = std::pair<uint32_t, uint32_t>;

static std::map<uint32_t, ModelBody> BodiesByModel(const Db2Table& models) {
    std::map<uint32_t, ModelBody> bodies;
    for (size_t row = 0; row < models.RowCount(); row++) {
        bodies[models.RowId(row)] = { models.Value(row, MODEL_DISPLAY), models.Value(row, MODEL_LAYOUT) };
    }
    return bodies;
}

// Per race, the body of each sex.
static std::map<uint32_t, std::array<ModelBody, 2>> BodiesByRace(const RaceTables& tables) {
    std::map<uint32_t, ModelBody> modelBodies = BodiesByModel(tables.models);
    std::map<uint32_t, std::array<ModelBody, 2>> bodies;
    for (size_t row = 0; row < tables.raceModels.RowCount(); row++) {
        uint32_t sex = tables.raceModels.Value(row, RACE_MODEL_SEX);
        auto body = modelBodies.find(tables.raceModels.Value(row, RACE_MODEL_MODEL));
        if (sex <= 1 && body != modelBodies.end()) {
            bodies[tables.raceModels.Value(row, RACE_MODEL_RACE)][sex] = body->second;
        }
    }
    return bodies;
}

static std::map<uint32_t, std::vector<TextureLayer>> LayersByLayout(const Db2Table& layers) {
    std::map<uint32_t, std::vector<TextureLayer>> byLayout;
    for (size_t row = 0; row < layers.RowCount(); row++) {
        byLayout[layers.Relation(row)].push_back({ layers.Value(row, LAYER_TYPE), layers.Value(row, LAYER_LAYER), layers.Value(row, LAYER_TARGETS) });
    }
    return byLayout;
}

bool RetailRaces::Load(const RaceTables& tables, std::string& error) {
    if (!KnownLayout(tables.races, CHR_RACES_LAYOUT, "ChrRaces", error) || !KnownLayout(tables.raceModels, CHR_RACE_X_CHR_MODEL_LAYOUT, "ChrRaceXChrModel", error)
        || !KnownLayout(tables.models, CHR_MODEL_LAYOUT, "ChrModel", error)
        || !KnownLayout(tables.layers, CHR_MODEL_TEXTURE_LAYER_LAYOUT, "ChrModelTextureLayer", error)) {
        return false;
    }

    std::map<uint32_t, std::array<ModelBody, 2>> bodies = BodiesByRace(tables);
    this->m_layers = LayersByLayout(tables.layers);
    this->m_races.clear();
    for (size_t row = 0; row < tables.races.RowCount(); row++) {
        RetailRace race;
        race.id = tables.races.RowId(row);
        race.prefix = tables.races.String(row, RACE_CLIENT_PREFIX);
        race.name = tables.races.String(row, RACE_NAME);
        auto found = bodies.find(race.id);
        for (size_t sex = 0; sex < 2 && found != bodies.end(); sex++) {
            race.displays[sex] = found->second[sex].first;
            race.layouts[sex] = found->second[sex].second;
        }
        this->m_races.push_back(std::move(race));
    }
    std::sort(this->m_races.begin(), this->m_races.end(), [](const RetailRace& a, const RetailRace& b) { return a.id < b.id; });
    return true;
}

const RetailRace* RetailRaces::Race(uint32_t id) const {
    auto race = std::lower_bound(this->m_races.begin(), this->m_races.end(), id, [](const RetailRace& r, uint32_t key) { return r.id < key; });
    return race != this->m_races.end() && race->id == id ? &*race : nullptr;
}

uint32_t RetailRaces::MaxId() const {
    return this->m_races.empty() ? 0 : this->m_races.back().id;
}

uint32_t RetailRaces::RaceOfPrefix(const std::string& prefix) const {
    std::string wanted = Lowercase(prefix);
    for (const RetailRace& race : this->m_races) {
        if (Lowercase(race.prefix) == wanted) {
            return race.id;
        }
    }
    return 0;
}

bool ServeRace(const RetailRace& race, uint32_t installedMaxId, ServedRace& served) {
    if (race.id <= installedMaxId) {
        return false;
    }
    served.id = race.id;
    served.flags = RACE_HIDDEN_FROM_CREATION;
    served.prefix = race.prefix;
    served.name = race.name;
    served.displays = race.displays;
    return true;
}

uint32_t RaceCapacity(uint32_t installedMaxId, uint32_t servedMaxId) {
    return std::max(installedMaxId, servedMaxId);
}

bool RaceFitsTables(uint32_t race, uint32_t sex, uint32_t capacity) {
    return race <= capacity && sex <= 1;
}

bool NeedsCharSections(bool installedRace, bool baked) {
    return installedRace || !baked;
}

std::vector<TextureLayer> RetailRaces::TextureLayers(uint32_t layoutId) const {
    auto layers = this->m_layers.find(layoutId);
    return layers == this->m_layers.end() ? std::vector<TextureLayer>() : layers->second;
}

}
