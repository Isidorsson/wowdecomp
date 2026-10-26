#include "Engine/Source/Retail/DisplayAudit.h"
#include <algorithm>

namespace Retail {

// The NPC item slots whose models the client attaches: helm (HelmFit), shoulders and belt buckles (BeltBuckle.h).
constexpr size_t HELM_SLOT = 0;
constexpr size_t SHOULDER_SLOT = 1;
constexpr size_t BELT_SLOT = 4;
constexpr uint32_t SECTION_BITS = 32;

static bool AttachedSlot(size_t slot) {
    return slot == HELM_SLOT || slot == SHOULDER_SLOT || slot == BELT_SLOT;
}

static void AuditItems(const NpcExtra& extra, const ItemDisplayLookup& items, std::vector<AuditFinding>& findings) {
    uint32_t droppedSections = 0;
    for (size_t slot = 0; slot < extra.items.size(); slot++) {
        if (!extra.items[slot]) {
            continue;
        }
        const RetailItemDisplay* item = items(extra.items[slot]);
        if (!item) {
            findings.push_back({ AuditFlag::MissingItem, static_cast<uint32_t>(slot) });
            continue;
        }
        if (!AttachedSlot(slot) && (item->models[0] || item->models[1])) {
            findings.push_back({ AuditFlag::ArmourModelDropped, static_cast<uint32_t>(slot) });
        }
        droppedSections |= item->droppedSections;
    }
    for (uint32_t section = 0; section < SECTION_BITS; section++) {
        if (droppedSections & (1u << section)) {
            findings.push_back({ AuditFlag::BodySectionDropped, section });
        }
    }
}

std::vector<AuditFinding> AuditDisplay(const RetailDisplay& display, const NpcExtra* extra, const ItemDisplayLookup& items) {
    std::vector<AuditFinding> findings;
    if (display.extendedDisplayId && !extra) {
        findings.push_back({ AuditFlag::MissingExtra, display.extendedDisplayId });
    }
    if (extra && !extra->sdBake && !extra->hdBake) {
        findings.push_back({ AuditFlag::NoBake, 0 });
    }
    if (extra) {
        AuditItems(*extra, items, findings);
    }
    for (const Geoset& geoset : display.droppedGeosets) {
        findings.push_back({ AuditFlag::GeosetDropped, geoset.index });
    }

    auto byFlagThenDetail = [](const AuditFinding& a, const AuditFinding& b) {
        return a.flag != b.flag ? a.flag < b.flag : a.detail < b.detail;
    };
    std::stable_sort(findings.begin(), findings.end(), byFlagThenDetail);
    findings.erase(std::unique(findings.begin(), findings.end()), findings.end());
    return findings;
}

const char* AuditFlagName(AuditFlag flag) {
    switch (flag) {
        case AuditFlag::MissingExtra: return "MissingExtra";
        case AuditFlag::NoBake: return "NoBake";
        case AuditFlag::MissingItem: return "MissingItem";
        case AuditFlag::ArmourModelDropped: return "ArmourModelDropped";
        case AuditFlag::BodySectionDropped: return "BodySectionDropped";
        case AuditFlag::GeosetDropped: return "GeosetDropped";
    }
    return "Unknown";
}

}
