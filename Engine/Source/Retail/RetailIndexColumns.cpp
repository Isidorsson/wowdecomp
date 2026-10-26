#include "Engine/Source/Retail/RetailIndex.h"
#include <algorithm>

// The retail browser grid's column filters and sort values (Interface\AddOns\RetailBrowser\Columns.lua): what every
// kind has (its id, its displays and how many, whether the installed DBC has one of them) and the few kind-only
// columns the older filters (RetailIndexSearch.cpp) do not cover.

namespace Retail {

namespace {

bool InRange(int32_t min, int32_t max, uint32_t value) {
    return (min < 0 || value >= static_cast<uint32_t>(min)) && (max < 0 || value <= static_cast<uint32_t>(max));
}

bool Matches(int32_t filter, uint32_t value) {
    return filter < 0 || static_cast<uint32_t>(filter) == value;
}

// An entry's displays (or appearances, pieces, users) as the column filters and sorts read them.
class DisplayScan {
    public:
    DisplayScan(const BrowseFilters& filters, bool wantInstalled) : m_filters(filters), m_wantInstalled(wantInstalled) {}

    void Add(uint32_t display, const std::function<bool(uint32_t)>& installed) {
        if (this->m_count++ == 0) {
            this->m_first = display;
        }
        this->m_found = this->m_found || (this->m_filters.displayId >= 0 && display == static_cast<uint32_t>(this->m_filters.displayId));
        if (this->m_wantInstalled && !this->m_installed && display && installed) {
            this->m_installed = installed(display);
        }
    }

    uint32_t Count() const { return this->m_count; }
    uint32_t First() const { return this->m_first; }
    bool Found() const { return this->m_found; }
    bool Installed() const { return this->m_installed; }

    private:
    const BrowseFilters& m_filters;
    bool m_wantInstalled;
    uint32_t m_count = 0;
    uint32_t m_first = 0;
    bool m_found = false;
    bool m_installed = false;
};

void ScanDisplays(const std::vector<BrowseDisplay>& displays, const std::function<bool(uint32_t)>& installed, DisplayScan& scan) {
    for (const BrowseDisplay& display : displays) {
        scan.Add(display.id, installed);
    }
}

uint32_t RaceOf(const BrowseCreature& creature) {
    for (const BrowseDisplay& display : creature.displays) {
        if (display.facts.extraId) {
            return display.facts.race;
        }
    }
    return 0;
}

bool HasExtra(const BrowseCreature& creature) {
    return std::any_of(creature.displays.begin(), creature.displays.end(), [](const BrowseDisplay& d) { return d.facts.extraId != 0; });
}

bool AnyColumnFilter(const BrowseFilters& f) {
    for (int32_t value : { f.minId, f.maxId, f.displayId, f.minCount, f.maxCount, f.installed, f.setSource, f.modelKind, f.mountType }) {
        if (value >= 0) {
            return true;
        }
    }
    return false;
}

}

bool RetailIndex::PassesColumns(BrowseKind kind, size_t entry, const BrowseFilters& f) const {
    if (!AnyColumnFilter(f)) {
        return true;
    }
    DisplayScan scan(f, f.installed >= 0);
    uint32_t id = 0;
    bool own = true;   // The kind-only filters.
    switch (kind) {
        case BrowseKind::Creature:
            id = this->m_creatures[entry].id;
            ScanDisplays(this->m_creatures[entry].displays, f.installedCreature, scan);
            break;
        case BrowseKind::Item:
            id = this->m_items[entry].id;
            for (const BrowseAppearance& look : this->m_items[entry].appearances) {
                scan.Add(look.displayId, f.installedItem);
            }
            break;
        case BrowseKind::Set: {
            const BrowseSet& set = this->m_sets[entry];
            id = set.setId;
            own = Matches(f.setSource, set.source == SetSource::ItemSet ? 1 : 0);
            for (const SetPiece& piece : set.pieces) {
                scan.Add(piece.displayId, f.installedItem);
            }
            break;
        }
        case BrowseKind::Model: {
            const BrowseModel& model = this->m_models[entry];
            id = model.fdid;
            own = Matches(f.modelKind, model.creature ? 1 : 0);
            for (const ModelUser& user : model.users) {
                scan.Add(user.displayId, user.kind == BrowseKind::Item ? f.installedItem : f.installedCreature);
            }
            break;
        }
        default:
            id = this->m_mounts[entry].id;
            own = Matches(f.mountType, this->m_mounts[entry].mountType);
            ScanDisplays(this->m_mounts[entry].displays, f.installedCreature, scan);
            break;
    }
    return own && InRange(f.minId, f.maxId, id) && (f.displayId < 0 || scan.Found()) && InRange(f.minCount, f.maxCount, scan.Count()) &&
        Matches(f.installed, scan.Installed() ? 1 : 0);
}

static uint32_t CreatureValue(const BrowseCreature& c, SortColumn column) {
    switch (column) {
        case SortColumn::Id:
            return c.id;
        case SortColumn::Type:
            return c.type;
        case SortColumn::Rank:
            return static_cast<uint32_t>(GroupOfClassification(c.classification));
        case SortColumn::Race:
            return RaceOf(c);
        case SortColumn::Family:
            return c.family;
        case SortColumn::Kind:
            return HasExtra(c) ? 1 : 0;
        default:
            return 0;
    }
}

static uint32_t ItemValue(const BrowseItem& i, SortColumn column) {
    switch (column) {
        case SortColumn::Id:
            return i.id;
        case SortColumn::ItemLevel:
            return i.itemLevel;
        case SortColumn::Quality:
            return i.quality;
        case SortColumn::Slot:
            return i.inventoryType;
        case SortColumn::Subclass:
            return i.classId << 8 | (i.subclassId & 0xFF);
        case SortColumn::Expansion:
            return i.expansion;
        default:
            return 0;
    }
}

static uint32_t SetValue(const BrowseSet& s, SortColumn column) {
    switch (column) {
        case SortColumn::Id:
            return s.setId;
        case SortColumn::Subclass:
            return s.armour;
        case SortColumn::Expansion:
            return s.expansion;
        case SortColumn::Kind:
            return s.source == SetSource::ItemSet ? 1 : 0;
        default:
            return 0;
    }
}

uint32_t RetailIndex::SortValue(BrowseKind kind, size_t entry, const BrowseFilters& f) const {
    SortColumn column = static_cast<SortColumn>(f.sort);
    bool scanned = column == SortColumn::Display || column == SortColumn::Count || column == SortColumn::Installed;
    BrowseFilters wanted;
    DisplayScan scan(wanted, column == SortColumn::Installed);
    uint32_t value = 0;
    if (kind == BrowseKind::Creature) {
        if (scanned) {
            ScanDisplays(this->m_creatures[entry].displays, f.installedCreature, scan);
        }
        value = CreatureValue(this->m_creatures[entry], column);
    } else if (kind == BrowseKind::Item) {
        for (size_t i = 0; scanned && i < this->m_items[entry].appearances.size(); i++) {
            scan.Add(this->m_items[entry].appearances[i].displayId, f.installedItem);
        }
        value = ItemValue(this->m_items[entry], column);
    } else if (kind == BrowseKind::Set) {
        for (size_t i = 0; scanned && i < this->m_sets[entry].pieces.size(); i++) {
            scan.Add(this->m_sets[entry].pieces[i].displayId, f.installedItem);
        }
        value = SetValue(this->m_sets[entry], column);
    } else if (kind == BrowseKind::Model) {
        const BrowseModel& model = this->m_models[entry];
        for (size_t i = 0; scanned && i < model.users.size(); i++) {
            scan.Add(model.users[i].displayId, model.users[i].kind == BrowseKind::Item ? f.installedItem : f.installedCreature);
        }
        value = column == SortColumn::Id ? model.fdid : column == SortColumn::Kind ? (model.creature ? 1 : 0) : 0;
    } else {
        const BrowseMount& mount = this->m_mounts[entry];
        if (scanned) {
            ScanDisplays(mount.displays, f.installedCreature, scan);
        }
        value = column == SortColumn::Id ? mount.id : column == SortColumn::Kind ? mount.mountType : 0;
    }
    switch (column) {
        case SortColumn::Display:
            return scan.First();
        case SortColumn::Count:
            return scan.Count();
        case SortColumn::Installed:
            return scan.Installed() ? 1 : 0;
        default:
            return value;
    }
}

}
