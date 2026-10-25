#include "Engine/Source/Retail/RetailSkinLayout.h"
#include "Engine/Source/Retail/Decimate.h"
#include "Engine/Source/Retail/RetailModelData.h"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <optional>

namespace Retail {

constexpr uint32_t MAX_INDEX_START = 0xFFFF;
constexpr size_t OTHERS_BUDGET = MAX_INDEX_START / 3;   // Triangles the submeshes other than the last may hold together.
constexpr size_t BATCH_SECTION = 0x04;

namespace SectionAt {
    constexpr size_t LEVEL = 0x02;
    constexpr size_t INDEX_START = 0x08;
    constexpr size_t INDEX_COUNT = 0x0A;
}

namespace VertexAt {
    constexpr size_t POSITION = 0x00;
    constexpr size_t WEIGHTS = 0x0C;
    constexpr size_t BONES = 0x10;
    constexpr size_t NORMAL = 0x14;
    constexpr size_t UV = 0x20;
}

uint32_t SectionIndexStart(const std::vector<uint8_t>& skin, size_t at) {
    return uint32_t(Get<uint16_t>(skin, at + SectionAt::LEVEL)) << 16 | Get<uint16_t>(skin, at + SectionAt::INDEX_START);
}

std::vector<uint32_t> SectionCounts(const std::vector<uint8_t>& skin) {
    Span sections;
    std::string error;
    if (skin.size() < SkinAt::HEADER || !ReadSpan(skin, SkinAt::SECTIONS, SECTION_SIZE, "submeshes", sections, error)) {
        return {};
    }

    std::vector<uint32_t> counts;
    for (uint32_t s = 0; s < sections.count; s++) {
        counts.push_back(Get<uint16_t>(skin, sections.At(s, SECTION_SIZE) + SectionAt::INDEX_COUNT));
    }
    return counts;
}

// Whether every submesh with corners starts within 16 bits, placed in order (the downporter's starts_fit).
static bool StartsFit(const std::vector<uint32_t>& counts, const std::vector<uint32_t>& order) {
    uint32_t start = 0;
    for (uint32_t s : order) {
        if (!counts[s]) {
            continue;
        }
        if (start > MAX_INDEX_START) {
            return false;
        }
        start += counts[s];
    }
    return true;
}

// Submesh order when it fits, else the smallest first and the largest last; none when neither does.
static std::optional<std::vector<uint32_t>> LayoutOrder(const std::vector<uint32_t>& counts) {
    std::vector<uint32_t> order(counts.size());
    std::iota(order.begin(), order.end(), 0);
    if (StartsFit(counts, order)) {
        return order;
    }

    std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return counts[a] < counts[b]; });
    return StartsFit(counts, order) ? std::optional<std::vector<uint32_t>>(order) : std::nullopt;
}

bool SectionsFit(const std::vector<uint32_t>& counts) {
    return LayoutOrder(counts).has_value();
}

// The corners of every submesh, Level included; false when one runs past the skin's indices.
static bool ReadWindows(const std::vector<uint8_t>& skin, const Span& indices, const Span& sections, std::vector<std::vector<uint16_t>>& windows, uint32_t& leveled,
    std::string& error) {
    for (uint32_t s = 0; s < sections.count; s++) {
        size_t at = sections.At(s, SECTION_SIZE);
        uint32_t start = SectionIndexStart(skin, at);
        uint32_t count = Get<uint16_t>(skin, at + SectionAt::INDEX_COUNT);
        if (uint64_t(start) + count > indices.count) {
            return Fail(error, "a submesh runs past the skin's indices");
        }

        leveled += start > MAX_INDEX_START;
        std::vector<uint16_t> window(count);
        if (count) {
            std::memcpy(window.data(), skin.data() + indices.At(start, 2), count * 2);
        }
        windows.push_back(std::move(window));
    }
    return true;
}

static std::vector<uint32_t> Lengths(const std::vector<std::vector<uint16_t>>& windows) {
    std::vector<uint32_t> lengths;
    for (const std::vector<uint16_t>& window : windows) {
        lengths.push_back(static_cast<uint32_t>(window.size()));
    }
    return lengths;
}

// The windows in order, one after another, with the batches renumbered to follow their submeshes.
static void Rebuild(std::vector<uint8_t>& skin, const Span& sections, const Span& batches, const std::vector<std::vector<uint16_t>>& windows, const std::vector<uint32_t>& order) {
    std::vector<uint8_t> records;
    std::vector<uint8_t> indices;
    std::vector<uint16_t> moved(order.size());

    for (uint32_t i = 0; i < order.size(); i++) {
        const std::vector<uint16_t>& window = windows[order[i]];
        size_t at = sections.At(order[i], SECTION_SIZE);
        std::vector<uint8_t> record(skin.begin() + at, skin.begin() + at + SECTION_SIZE);
        Put<uint16_t>(record, SectionAt::LEVEL, 0);
        Put<uint16_t>(record, SectionAt::INDEX_START, static_cast<uint16_t>(window.empty() ? 0 : indices.size() / 2));
        Put<uint16_t>(record, SectionAt::INDEX_COUNT, static_cast<uint16_t>(window.size()));
        size_t end = indices.size();
        indices.resize(end + window.size() * 2);
        if (!window.empty()) {
            std::memcpy(indices.data() + end, window.data(), window.size() * 2);
        }
        records.insert(records.end(), record.begin(), record.end());
        moved[order[i]] = static_cast<uint16_t>(i);
    }

    for (uint32_t b = 0; b < batches.count; b++) {
        size_t at = batches.At(b, BATCH_SIZE) + BATCH_SECTION;
        uint16_t section = Get<uint16_t>(skin, at);
        if (section < moved.size()) {
            Put<uint16_t>(skin, at, moved[section]);
        }
    }

    Put<uint32_t>(skin, SkinAt::INDICES, static_cast<uint32_t>(indices.size() / 2));
    Put<uint32_t>(skin, SkinAt::INDICES + 4, AppendAligned(skin, indices));
    Put<uint32_t>(skin, SkinAt::SECTIONS + 4, AppendAligned(skin, records));
}

// How many triangles each submesh keeps: exempt (the largest, which may run past 65,535) keeps all, the others are
// capped at the one count that lets them hold budget together, so small submeshes keep theirs (cap_to_budget).
static std::vector<size_t> CapToBudget(const std::vector<size_t>& triangles, size_t budget, size_t exempt) {
    auto held = [&](size_t cap) {
        size_t sum = 0;
        for (size_t i = 0; i < triangles.size(); i++) {
            sum += i == exempt ? 0 : std::min(triangles[i], cap);
        }
        return sum;
    };

    size_t low = 0;
    size_t high = triangles.empty() ? 0 : *std::max_element(triangles.begin(), triangles.end());
    while (low < high) {
        size_t middle = (low + high + 1) / 2;
        if (held(middle) <= budget) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }

    std::vector<size_t> targets;
    for (size_t i = 0; i < triangles.size(); i++) {
        targets.push_back(i == exempt ? triangles[i] : std::min(triangles[i], low));
    }
    return targets;
}

// What the decimator weighs of a skin vertex (the downporter's vertex_data); one past the model's is blank.
static DecimationVertex VertexOf(const std::vector<uint8_t>& skin, const Span& lookup, const std::vector<uint8_t>& m2, const Span& vertices, uint16_t local) {
    DecimationVertex data;
    uint16_t vertex = local < lookup.count ? Get<uint16_t>(skin, lookup.At(local, 2)) : NO_INDEX;
    if (vertex >= vertices.count) {
        return data;
    }

    size_t at = vertices.At(vertex, VERTEX_SIZE);
    for (size_t axis = 0; axis < 3; axis++) {
        data.position[axis] = Get<float>(m2, at + VertexAt::POSITION + axis * 4);
        data.normal[axis] = Get<float>(m2, at + VertexAt::NORMAL + axis * 4);
    }
    for (size_t set = 0; set < 2; set++) {
        data.uv[set] = { Get<float>(m2, at + VertexAt::UV + set * 8), Get<float>(m2, at + VertexAt::UV + set * 8 + 4) };
    }

    // The heaviest slot, the first among equals.
    size_t heaviest = 0;
    for (size_t slot = 1; slot < 4; slot++) {
        heaviest = m2[at + VertexAt::WEIGHTS + slot] > m2[at + VertexAt::WEIGHTS + heaviest] ? slot : heaviest;
    }
    data.mainBone = m2[at + VertexAt::BONES + heaviest];
    return data;
}

// The window cut to at most target triangles by edge collapse, over the skin vertices it uses.
static DecimationStats DecimateWindow(const std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, std::vector<uint16_t>& window, size_t target) {
    Span lookup;
    Span vertices;
    std::string error;
    if (!ReadSpan(skin, SkinAt::VERTICES, 2, "skin vertices", lookup, error) || !ReadSpan(m2, M2At::VERTICES, VERTEX_SIZE, "vertices", vertices, error)) {
        lookup = {};
        vertices = {};
    }

    std::vector<uint16_t> used(window.begin(), window.begin() + window.size() / 3 * 3);
    std::sort(used.begin(), used.end());
    used.erase(std::unique(used.begin(), used.end()), used.end());
    std::vector<DecimationVertex> data;
    for (uint16_t local : used) {
        data.push_back(VertexOf(skin, lookup, m2, vertices, local));
    }

    auto compact = [&](uint16_t corner) { return static_cast<uint32_t>(std::lower_bound(used.begin(), used.end(), corner) - used.begin()); };
    std::vector<IndexTriangle> triangles;
    for (size_t c = 0; c + 2 < window.size(); c += 3) {
        triangles.push_back({ compact(window[c]), compact(window[c + 1]), compact(window[c + 2]) });
    }

    Decimated result = Decimate(data, triangles, target);
    window.clear();
    for (const IndexTriangle& triangle : result.triangles) {
        for (uint32_t corner : triangle) {
            window.push_back(used[corner]);
        }
    }
    return result.stats;
}

// The submeshes other than the largest decimated until they hold 65,535 corners together (the downporter's fit).
static void Reduce(const std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, std::vector<std::vector<uint16_t>>& windows, SkinLayout& layout) {
    std::vector<size_t> triangles;
    for (const std::vector<uint16_t>& window : windows) {
        triangles.push_back(window.size() / 3);
    }
    size_t largest = std::max_element(triangles.begin(), triangles.end()) - triangles.begin();
    std::vector<size_t> targets = CapToBudget(triangles, OTHERS_BUDGET, largest);

    for (size_t s = 0; s < windows.size(); s++) {
        if (targets[s] >= triangles[s]) {
            continue;
        }
        DecimationStats stats = DecimateWindow(skin, m2, windows[s], targets[s]);
        layout.reduced.push_back({ static_cast<uint32_t>(s), static_cast<uint32_t>(triangles[s]), static_cast<uint32_t>(windows[s].size() / 3) });
        layout.stats.collapsed += stats.collapsed;
        layout.stats.borderCollapsed += stats.borderCollapsed;
        layout.stats.dropped += stats.dropped;
        layout.stats.degenerate += stats.degenerate;
    }
}

bool LayOutSections(std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, SkinLayout& layout, std::string& error) {
    Span indices;
    Span sections;
    Span batches;
    if (!ReadSpan(skin, SkinAt::INDICES, 2, "skin indices", indices, error) || !ReadSpan(skin, SkinAt::SECTIONS, SECTION_SIZE, "submeshes", sections, error)
        || !ReadSpan(skin, SkinAt::BATCHES, BATCH_SIZE, "batches", batches, error)) {
        return false;
    }

    layout = {};
    std::vector<std::vector<uint16_t>> windows;
    if (!ReadWindows(skin, indices, sections, windows, layout.leveled, error)) {
        return false;
    }

    std::optional<std::vector<uint32_t>> order = LayoutOrder(Lengths(windows));
    if (!order) {
        Reduce(skin, m2, windows, layout);
        order = LayoutOrder(Lengths(windows));
    }
    if (!order) {
        return Fail(error, "the view's submeshes could not be laid out within the 16-bit starts of the 3.3.5 client");
    }

    layout.reordered = !std::is_sorted(order->begin(), order->end());
    if (!layout.leveled && !layout.reordered && layout.reduced.empty()) {
        return true;
    }

    layout.changed = true;
    Rebuild(skin, sections, batches, windows, *order);
    return true;
}

// "submesh 0 from 4142 to 3892, ...; 5390 collapses inside the surface, ...": the downporter's describe_reductions.
static std::string DescribeReductions(const SkinLayout& layout) {
    constexpr size_t EXAMPLES = 5;
    std::string changes;
    for (size_t i = 0; i < std::min(layout.reduced.size(), EXAMPLES); i++) {
        const SectionReduction& reduction = layout.reduced[i];
        changes += (i ? ", " : "") + std::string("submesh ") + std::to_string(reduction.section) + " from " + std::to_string(reduction.before) + " to "
            + std::to_string(reduction.after);
    }
    size_t count = layout.reduced.size();
    return std::to_string(count) + (count == 1 ? " submesh" : " submeshes") + " lost triangles by edge collapse (" + changes + (count > EXAMPLES ? ", ...)" : ")") + "; "
        + std::to_string(layout.stats.collapsed) + " collapses inside the surface, " + std::to_string(layout.stats.borderCollapsed) + " along borders, "
        + std::to_string(layout.stats.dropped) + " triangles dropped, " + std::to_string(layout.stats.degenerate) + " degenerate removed";
}

void ReportLayout(uint32_t view, const std::vector<uint8_t>& skin, const SkinLayout& layout, std::vector<std::string>& warnings) {
    if (!layout.changed) {
        return;
    }

    std::string scope = "skin " + std::to_string(view) + ": ";
    std::string corners = std::to_string(Get<uint32_t>(skin, SkinAt::INDICES));
    if (!layout.reduced.empty()) {
        warnings.push_back(scope + "the 3.3.5 client keeps each submesh's start in 16 bits, so the submeshes other than the largest can hold "
            + std::to_string(OTHERS_BUDGET) + " triangles together and this view needed more. " + DescribeReductions(layout) + ". The view has " + corners
            + " corners now, laid out from index 0 with the largest last");
        return;
    }

    std::string leveled = layout.leveled == 1 ? "1 submesh started" : std::to_string(layout.leveled) + " submeshes started";
    std::string how = layout.reordered ? "the largest last, as in submesh order one would start past the 65,535 the client keeps a start in" : "in submesh order";
    warnings.push_back(scope + (layout.leveled ? leveled + " past index 65,535 (submesh Level), which the 3.3.5 client ignores; " : "") + "the " + corners
        + " corners were laid out again from index 0, " + how + "; no triangle was removed");
}

}
