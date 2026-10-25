#include "Engine/Source/Retail/RetailBoneWindows.h"
#include "Engine/Source/Retail/RetailModelData.h"
#include "Engine/Source/Retail/RetailSkinLayout.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <set>

namespace Retail {

constexpr uint32_t VIEW_BONE_PALETTES[] = { 256, 64, 53, 21 };
constexpr uint32_t CPU_SKINNED_PALETTE = 256;
constexpr size_t PROPERTY_SIZE = 4;
constexpr uint32_t MAX_SKIN_VERTICES = 0x10000;   // What 16-bit indices name.
const char* const TOO_MANY_VERTICES = "the view needs more than 65,536 vertices once its pieces copy the vertices they share";

namespace SectionAt {
    constexpr size_t LEVEL = 0x02;
    constexpr size_t VERTEX_START = 0x04;
    constexpr size_t VERTEX_COUNT = 0x06;
    constexpr size_t INDEX_START = 0x08;
    constexpr size_t INDEX_COUNT = 0x0A;
    constexpr size_t BONE_COUNT = 0x0C;
    constexpr size_t BONE_COMBO = 0x0E;
}

namespace BatchAt {
    constexpr size_t SECTION = 0x04;
    constexpr size_t GEOSET = 0x06;
}

uint32_t ViewBonePalette(uint32_t view, uint32_t views) {
    uint32_t first = MAX_VIEWS - std::min(views, MAX_VIEWS);
    return VIEW_BONE_PALETTES[std::min(first + view, MAX_VIEWS - 1)];
}

static std::set<uint16_t> GrowWindow(const std::vector<std::vector<uint16_t>>& triangles, const std::vector<uint32_t>& remaining, uint32_t palette) {
    std::set<uint16_t> window;

    while (window.size() < palette) {
        std::map<uint16_t, double> score;
        for (uint32_t t : remaining) {
            std::vector<uint16_t> missing;
            std::copy_if(triangles[t].begin(), triangles[t].end(), std::back_inserter(missing), [&](uint16_t bone) { return !window.count(bone); });
            if (missing.empty() || window.size() + missing.size() > palette) {
                continue;
            }

            for (uint16_t bone : missing) {
                score[bone] += 1.0 / missing.size();
            }
        }

        if (score.empty()) {
            break;
        }

        // The highest score, the lowest bone among equals.
        auto best = std::max_element(score.begin(), score.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        window.insert(best->first);
    }

    return window;
}

std::vector<TriangleWindow> PartitionTriangles(const std::vector<std::vector<uint16_t>>& triangles, uint32_t palette) {
    std::vector<uint32_t> remaining(triangles.size());
    for (uint32_t t = 0; t < remaining.size(); t++) {
        remaining[t] = t;
    }

    std::vector<TriangleWindow> windows;
    while (!remaining.empty()) {
        std::set<uint16_t> window = GrowWindow(triangles, remaining, palette);
        auto covered = [&](uint32_t t) { return std::all_of(triangles[t].begin(), triangles[t].end(), [&](uint16_t bone) { return window.count(bone) != 0; }); };

        if (!covered(remaining.front()) && std::none_of(remaining.begin(), remaining.end(), covered)) {
            window = std::set<uint16_t>(triangles[remaining.front()].begin(), triangles[remaining.front()].end());
        }

        TriangleWindow piece;
        std::vector<uint32_t> rest;
        for (uint32_t t : remaining) {
            (covered(t) ? piece.triangles : rest).push_back(t);
        }

        piece.bones.assign(window.begin(), window.end());
        windows.push_back(std::move(piece));
        remaining = std::move(rest);
    }

    return windows;
}

// The arrays of a skin and its model the split reads.
struct SkinData {
    const std::vector<uint8_t>& skin;
    const std::vector<uint8_t>& m2;
    Span lookup;
    Span indices;
    Span properties;
    Span sections;
    Span batches;
    Span vertices;
    Span combos;

    SkinData(const std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2) : skin(skin), m2(m2) {}

    bool Read(std::string& error) {
        return ReadSpan(skin, SkinAt::VERTICES, 2, "skin vertices", lookup, error) && ReadSpan(skin, SkinAt::INDICES, 2, "skin indices", indices, error)
            && ReadSpan(skin, SkinAt::BONE_PROPERTIES, PROPERTY_SIZE, "skin bone properties", properties, error)
            && ReadSpan(skin, SkinAt::SECTIONS, SECTION_SIZE, "submeshes", sections, error) && ReadSpan(skin, SkinAt::BATCHES, BATCH_SIZE, "batches", batches, error)
            && ReadSpan(m2, M2At::VERTICES, VERTEX_SIZE, "vertices", vertices, error) && ReadSpan(m2, M2At::BONE_COMBOS, 2, "bone combos", combos, error);
    }

    uint16_t Field(uint32_t section, size_t field) const { return Get<uint16_t>(skin, sections.At(section, SECTION_SIZE) + field); }

    // The source vertex numbers of a submesh's triangle corners, refused when they run past the indices.
    bool Corners(uint32_t section, std::vector<uint16_t>& corners, std::string& error) const {
        uint32_t start = SectionIndexStart(skin, sections.At(section, SECTION_SIZE));
        uint32_t count = Field(section, SectionAt::INDEX_COUNT) / 3 * 3;
        if (start + count > indices.count) {
            return Fail(error, "a submesh runs past the skin's indices");
        }

        corners.clear();
        for (uint32_t i = start; i < start + count; i++) {
            corners.push_back(Get<uint16_t>(skin, indices.At(i, 2)));
        }
        return true;
    }

    std::array<uint8_t, 4> Properties(uint16_t local) const {
        std::array<uint8_t, 4> slots = {};
        if (local < properties.count) {
            std::memcpy(slots.data(), skin.data() + properties.At(local, PROPERTY_SIZE), 4);
        }
        return slots;
    }

    // The model bones local weighs, with the slot of each. A vertex with no weight at all draws with its first slot.
    std::vector<std::pair<size_t, uint16_t>> Weighted(uint32_t section, uint16_t local) const {
        std::array<uint8_t, 4> slots = Properties(local);
        std::array<uint8_t, 4> weights = {};
        if (local < lookup.count) {
            uint16_t vertex = Get<uint16_t>(skin, lookup.At(local, 2));
            if (vertex < vertices.count) {
                std::memcpy(weights.data(), m2.data() + vertices.At(vertex, VERTEX_SIZE) + 0x0C, 4);
            }
        }

        bool unweighted = std::all_of(weights.begin(), weights.end(), [](uint8_t w) { return w == 0; });
        std::vector<std::pair<size_t, uint16_t>> bones;
        for (size_t slot = 0; slot < 4; slot++) {
            uint32_t at = Field(section, SectionAt::BONE_COMBO) + slots[slot];
            if ((weights[slot] || (unweighted && slot == 0)) && at < combos.count) {
                bones.emplace_back(slot, Get<uint16_t>(m2, combos.At(at, 2)));
            }
        }
        return bones;
    }
};

static std::vector<std::vector<uint16_t>> TriangleBones(const SkinData& data, uint32_t section, const std::vector<uint16_t>& corners) {
    std::vector<std::vector<uint16_t>> triangles;
    for (size_t c = 0; c < corners.size(); c += 3) {
        std::set<uint16_t> bones;
        for (size_t k = c; k < c + 3; k++) {
            for (const auto& weighted : data.Weighted(section, corners[k])) {
                bones.insert(weighted.second);
            }
        }
        triangles.emplace_back(bones.begin(), bones.end());
    }
    return triangles;
}

static bool PlanView(const SkinData& data, uint32_t palette, std::vector<SectionPiece>& pieces, std::string& error) {
    bool wide = false;
    for (uint32_t s = 0; s < data.sections.count; s++) {
        wide |= data.Field(s, SectionAt::BONE_COUNT) > palette;
    }

    for (uint32_t s = 0; wide && s < data.sections.count; s++) {
        std::vector<uint16_t> corners;
        if (!data.Corners(s, corners, error)) {
            return false;
        }

        if (data.Field(s, SectionAt::BONE_COUNT) <= palette) {
            SectionPiece whole{ s, {}, {} };
            for (uint32_t t = 0; t < corners.size() / 3; t++) {
                whole.triangles.push_back(t);
            }
            pieces.push_back(std::move(whole));
            continue;
        }

        for (TriangleWindow& window : PartitionTriangles(TriangleBones(data, s, corners), palette)) {
            pieces.push_back({ s, std::move(window.bones), std::move(window.triangles) });
        }
    }

    return true;
}

bool PlanBoneWindows(const std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, BoneWindowPlan& plan, std::string& error) {
    Span combos;
    if (!ReadSpan(m2, M2At::BONE_COMBOS, 2, "bone combos", combos, error)) {
        return false;
    }

    plan = {};
    plan.combos = combos.count;
    plan.views.resize(skins.size());
    uint32_t views = static_cast<uint32_t>(skins.size());

    for (uint32_t view = 0; view < views; view++) {
        uint32_t palette = ViewBonePalette(view, views);
        SkinData data(skins[view], m2);
        std::string broken;
        if (palette >= CPU_SKINNED_PALETTE || skins[view].size() < SkinAt::HEADER || !data.Read(broken) || !PlanView(data, palette, plan.views[view], broken)) {
            plan.views[view].clear();
        }
    }

    return true;
}

static uint32_t WindowStart(const BoneWindowPlan& plan, uint32_t view) {
    uint32_t start = plan.combos;
    for (uint32_t v = 0; v < view; v++) {
        for (const SectionPiece& piece : plan.views[v]) {
            start += static_cast<uint32_t>(piece.window.size());
        }
    }
    return start;
}

static void AppendU16(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}

bool AppendBoneWindows(std::vector<uint8_t>& m2, const BoneWindowPlan& plan, std::string& error) {
    uint32_t total = WindowStart(plan, static_cast<uint32_t>(plan.views.size()));
    if (total == plan.combos) {
        return true;
    }

    if (total > NO_INDEX) {
        return Fail(error, "the bone combos would need " + std::to_string(total) + " entries, past the 16-bit window starts");
    }

    uint32_t offset = Get<uint32_t>(m2, M2At::BONE_COMBOS + 4);
    std::vector<uint8_t> combos(m2.begin() + offset, m2.begin() + offset + plan.combos * 2);
    for (const auto& view : plan.views) {
        for (const SectionPiece& piece : view) {
            for (uint16_t bone : piece.window) {
                AppendU16(combos, bone);
            }
        }
    }

    Put<uint32_t>(m2, M2At::BONE_COMBOS, total);
    Put<uint32_t>(m2, M2At::BONE_COMBOS + 4, AppendAligned(m2, combos));
    return true;
}

// The rebuilt arrays of a split skin.
struct SplitSkin {
    std::vector<uint8_t> lookup;
    std::vector<uint8_t> properties;
    std::vector<uint8_t> indices;
    std::vector<uint8_t> sections;
    uint32_t vertices = 0;
    uint32_t corners = 0;
};

// The bone slots of local relative to window; unweighted slots point at the window's first bone.
static std::array<uint8_t, 4> SlotsIn(const SkinData& data, uint32_t section, uint16_t local, const std::vector<uint16_t>& window) {
    std::array<uint8_t, 4> slots = {};
    for (const auto& weighted : data.Weighted(section, local)) {
        auto at = std::lower_bound(window.begin(), window.end(), weighted.second);
        slots[weighted.first] = at != window.end() && *at == weighted.second ? static_cast<uint8_t>(at - window.begin()) : 0;
    }
    return slots;
}

// The piece's own vertices, numbered in the order its corners name them, and its corners into them.
static void AppendPieceVertices(const SkinData& data, const SectionPiece& piece, const std::vector<uint16_t>& corners, SplitSkin& out) {
    std::map<uint16_t, uint32_t> numbers;
    for (uint32_t t : piece.triangles) {
        for (size_t k = t * 3; k < t * 3 + 3; k++) {
            uint16_t corner = corners[k];
            auto number = numbers.find(corner);
            if (number == numbers.end()) {
                number = numbers.emplace(corner, out.vertices + static_cast<uint32_t>(numbers.size())).first;
                AppendU16(out.lookup, corner < data.lookup.count ? Get<uint16_t>(data.skin, data.lookup.At(corner, 2)) : 0);
                std::array<uint8_t, 4> slots = piece.window.empty() ? data.Properties(corner) : SlotsIn(data, piece.parent, corner, piece.window);
                out.properties.insert(out.properties.end(), slots.begin(), slots.end());
            }
            AppendU16(out.indices, number->second);
        }
    }
    out.vertices += static_cast<uint32_t>(numbers.size());
}

static bool AppendPiece(const SkinData& data, const SectionPiece& piece, uint32_t& combo, SplitSkin& out, std::string& error) {
    std::vector<uint16_t> corners;
    if (!data.Corners(piece.parent, corners, error)) {
        return false;
    }

    if (out.vertices > NO_INDEX) {
        return Fail(error, TOO_MANY_VERTICES);
    }

    uint32_t vertexStart = out.vertices;
    uint32_t indexStart = out.corners;
    AppendPieceVertices(data, piece, corners, out);
    out.corners += static_cast<uint32_t>(piece.triangles.size() * 3);

    size_t at = data.sections.At(piece.parent, SECTION_SIZE);
    std::vector<uint8_t> section(data.skin.begin() + at, data.skin.begin() + at + SECTION_SIZE);
    Put<uint16_t>(section, SectionAt::LEVEL, static_cast<uint16_t>(indexStart >> 16));
    Put<uint16_t>(section, SectionAt::VERTEX_START, static_cast<uint16_t>(vertexStart));
    Put<uint16_t>(section, SectionAt::VERTEX_COUNT, static_cast<uint16_t>(std::min<uint32_t>(out.vertices - vertexStart, NO_INDEX)));
    Put<uint16_t>(section, SectionAt::INDEX_START, static_cast<uint16_t>(indexStart));
    Put<uint16_t>(section, SectionAt::INDEX_COUNT, static_cast<uint16_t>(out.corners - indexStart));

    if (!piece.window.empty()) {
        if (combo > NO_INDEX) {
            return Fail(error, "a piece's bone window starts past the 16-bit window starts");
        }
        Put<uint16_t>(section, SectionAt::BONE_COMBO, static_cast<uint16_t>(combo));
        Put<uint16_t>(section, SectionAt::BONE_COUNT, static_cast<uint16_t>(piece.window.size()));
        combo += static_cast<uint32_t>(piece.window.size());
    }

    out.sections.insert(out.sections.end(), section.begin(), section.end());
    return true;
}

// Every batch once per piece of its submesh, in the original batch order. A batch whose geoset index repeats its
// submesh index keeps doing so.
static std::vector<uint8_t> RepeatBatches(const SkinData& data, const std::vector<SectionPiece>& pieces) {
    std::vector<uint8_t> batches;
    for (uint32_t b = 0; b < data.batches.count; b++) {
        size_t at = data.batches.At(b, BATCH_SIZE);
        std::vector<uint8_t> batch(data.skin.begin() + at, data.skin.begin() + at + BATCH_SIZE);
        uint16_t parent = Get<uint16_t>(batch, BatchAt::SECTION);
        bool geosetIsSection = Get<uint16_t>(batch, BatchAt::GEOSET) == parent;
        bool placed = false;

        for (uint16_t i = 0; i < pieces.size(); i++) {
            if (pieces[i].parent != parent) {
                continue;
            }
            Put<uint16_t>(batch, BatchAt::SECTION, i);
            if (geosetIsSection) {
                Put<uint16_t>(batch, BatchAt::GEOSET, i);
            }
            batches.insert(batches.end(), batch.begin(), batch.end());
            placed = true;
        }

        if (!placed) {
            batches.insert(batches.end(), batch.begin(), batch.end());
        }
    }
    return batches;
}

bool SplitSections(std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, const BoneWindowPlan& plan, uint32_t view, std::string& error) {
    if (view >= plan.views.size() || plan.views[view].empty()) {
        return true;
    }

    const std::vector<SectionPiece>& pieces = plan.views[view];
    SkinData data(skin, m2);
    if (!data.Read(error)) {
        return false;
    }

    SplitSkin out;
    uint32_t combo = WindowStart(plan, view);
    for (const SectionPiece& piece : pieces) {
        if (!AppendPiece(data, piece, combo, out, error)) {
            return false;
        }
    }

    if (out.vertices > MAX_SKIN_VERTICES) {
        return Fail(error, TOO_MANY_VERTICES);
    }

    std::vector<uint8_t> batches = RepeatBatches(data, pieces);
    std::vector<uint8_t> rebuilt = skin;
    auto point = [&](size_t header, uint32_t count, const std::vector<uint8_t>& bytes) {
        Put<uint32_t>(rebuilt, header, count);
        Put<uint32_t>(rebuilt, header + 4, AppendAligned(rebuilt, bytes));
    };
    point(SkinAt::VERTICES, out.vertices, out.lookup);
    point(SkinAt::BONE_PROPERTIES, out.vertices, out.properties);
    point(SkinAt::INDICES, out.corners, out.indices);
    point(SkinAt::SECTIONS, static_cast<uint32_t>(pieces.size()), out.sections);
    point(SkinAt::BATCHES, static_cast<uint32_t>(batches.size() / BATCH_SIZE), batches);
    skin = std::move(rebuilt);
    return true;
}

// "1 submesh (0)", "3 submeshes (0, 2, 5)": the downporter's describe_items.
static std::string DescribeSubmeshes(const std::vector<uint32_t>& indices) {
    constexpr size_t EXAMPLES = 5;
    std::string listed;
    for (size_t i = 0; i < std::min(indices.size(), EXAMPLES); i++) {
        listed += (i ? ", " : "") + std::to_string(indices[i]);
    }
    return std::to_string(indices.size()) + (indices.size() == 1 ? " submesh (" : " submeshes (") + listed + (indices.size() > EXAMPLES ? ", ...)" : ")");
}

void ReportBoneWindows(const std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, const BoneWindowPlan& plan, std::vector<std::string>& warnings) {
    uint32_t views = static_cast<uint32_t>(plan.views.size());
    for (uint32_t view = 0; view < views; view++) {
        std::vector<uint32_t> wide;
        size_t split = 0;
        for (const SectionPiece& piece : plan.views[view]) {
            if (piece.window.empty()) {
                continue;
            }
            split++;
            if (wide.empty() || wide.back() != piece.parent) {
                wide.push_back(piece.parent);
            }
        }

        if (wide.empty()) {
            continue;
        }

        std::string scope = "skin " + std::to_string(view) + ": " + DescribeSubmeshes(wide);
        std::string limit = std::to_string(ViewBonePalette(view, views)) + " bones view " + std::to_string(view) + " of a " + std::to_string(views) + "-view model allows";
        std::vector<uint8_t> skin = skins[view];
        std::string error;
        if (!SplitSections(skin, m2, plan, view, error)) {
            warnings.push_back(scope + " use more than the " + limit + ", and splitting them by bone failed: " + error);
            continue;
        }

        warnings.push_back(scope + " drew with more than the " + limit + " (the 3.3.5 client picks views by position, not by boneCountMax, and past the view's "
            "palette it overruns its shader constants). They became " + std::to_string(split) + " submeshes, each with a window of only the bones its "
            "triangles weigh, copying the vertices where pieces meet: the view has " + std::to_string(Get<uint32_t>(skin, SkinAt::VERTICES)) + " vertices now ("
            + std::to_string(Get<uint32_t>(skins[view], SkinAt::VERTICES)) + " before) and the same triangles");
    }
}

}
