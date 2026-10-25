#include "Engine/Source/Retail/RetailViews.h"
#include "Engine/Source/Retail/RetailBoneWindows.h"
#include "Engine/Source/Retail/RetailModelData.h"
#include "Engine/Source/Retail/RetailSkinLayout.h"
#include <algorithm>
#include <map>

namespace Retail {

constexpr uint32_t MAX_SKIN_VERTEX = 0xFFFF;   // What a 16-bit skin lookup names.

// A candidate view: a view's skin or a LOD skin, as SFID lists it.
struct Candidate {
    uint32_t fileDataId;
    uint32_t view;      // Its view, or its LOD band (1 is <model>_lod01.skin).
    bool lod;
    std::vector<uint8_t> skin;
};

static std::string Name(const Candidate& candidate) {
    return (candidate.lod ? "LOD skin " : "view ") + std::to_string(candidate.view) + " (" + std::to_string(candidate.fileDataId) + ")";
}

// The LOD skin with its lookup counted from the model's first vertex and boneCountMax cleared. False with why when
// it cannot be: a broken skin, or one naming a vertex past 16 bits.
static bool RebaseLod(std::vector<uint8_t>& skin, std::string& why) {
    Span lookup;
    if (skin.size() < SkinAt::HEADER || !ReadSpan(skin, SkinAt::VERTICES, 2, "skin vertices", lookup, why)) {
        why = why.empty() ? "is not a skin" : why;
        return false;
    }

    uint32_t first = Get<uint32_t>(skin, SkinAt::BONE_COUNT_MAX);
    uint32_t highest = 0;
    for (uint32_t i = 0; i < lookup.count; i++) {
        highest = std::max<uint32_t>(highest, first + Get<uint16_t>(skin, lookup.At(i, 2)));
    }

    if (highest > MAX_SKIN_VERTEX) {
        why = "names model vertex " + std::to_string(highest) + ", past the 65,535 a 3.3.5 skin looks up";
        return false;
    }

    for (uint32_t i = 0; i < lookup.count; i++) {
        Put<uint16_t>(skin, lookup.At(i, 2), static_cast<uint16_t>(first + Get<uint16_t>(skin, lookup.At(i, 2))));
    }
    Put<uint32_t>(skin, SkinAt::BONE_COUNT_MAX, 0);
    return true;
}

// A candidate by its number among the candidates (a view's is its view), and its skin.
using Member = std::pair<size_t, const std::vector<uint8_t>*>;

// The pieces of each candidate at each palette a set tries it with, planned once: a set that loses a view keeps the
// palettes of the views after it.
class Planner {
    public:
    explicit Planner(const std::vector<uint8_t>& m2) : m_m2(m2) {
        Span combos;
        std::string error;
        this->m_readable = ReadSpan(m2, M2At::BONE_COMBOS, 2, "bone combos", combos, error);
        this->m_combos = combos.count;
    }

    // The first of set whose submeshes, split by bone for its place in it, the client cannot draw; set.size() when it
    // can draw all of them, or when the model's bone combos cannot be read.
    size_t FirstUnfit(const std::vector<Member>& set) {
        uint32_t views = static_cast<uint32_t>(set.size());
        for (uint32_t view = 0; this->m_readable && view < views; view++) {
            const std::vector<SectionPiece>& pieces = this->Pieces(set[view], view, views);
            std::vector<uint32_t> counts = SectionCounts(*set[view].second);
            if (!pieces.empty()) {
                counts.clear();
                for (const SectionPiece& piece : pieces) {
                    counts.push_back(static_cast<uint32_t>(piece.triangles.size() * 3));
                }
            }
            if (!SectionsFit(counts)) {
                return view;
            }
        }
        return set.size();
    }

    // The set's plan, as PlanBoneWindows makes it; none when the model's bone combos cannot be read.
    void Plan(const std::vector<Member>& set, ViewSkins& chosen) {
        chosen.planned = this->m_readable;
        chosen.plan = {};
        chosen.plan.combos = this->m_combos;
        uint32_t views = static_cast<uint32_t>(set.size());
        for (uint32_t view = 0; this->m_readable && view < views; view++) {
            chosen.plan.views.push_back(this->Pieces(set[view], view, views));
        }
    }

    private:
    const std::vector<SectionPiece>& Pieces(const Member& member, uint32_t view, uint32_t views) {
        uint32_t palette = ViewBonePalette(view, views);
        auto key = std::make_pair(member.first, palette);
        auto found = this->m_pieces.find(key);
        if (found == this->m_pieces.end()) {
            found = this->m_pieces.emplace(key, PlanPieces(this->m_m2, *member.second, palette)).first;
        }
        return found->second;
    }

    const std::vector<uint8_t>& m_m2;
    bool m_readable = false;
    uint32_t m_combos = 0;
    std::map<std::pair<size_t, uint32_t>, std::vector<SectionPiece>> m_pieces;
};

static std::vector<Member> MembersOf(const std::vector<Candidate>& candidates, const std::vector<const Candidate*>& kept) {
    std::vector<Member> members;
    for (const Candidate* candidate : kept) {
        members.emplace_back(static_cast<size_t>(candidate - candidates.data()), &candidate->skin);
    }
    return members;
}

static std::vector<std::vector<uint8_t>> SkinsOf(const std::vector<const Candidate*>& kept) {
    std::vector<std::vector<uint8_t>> skins;
    for (const Candidate* candidate : kept) {
        skins.push_back(candidate->skin);
    }
    return skins;
}

// The views and the LOD skins that can be read and looked up, in SFID order.
static std::vector<Candidate> ReadCandidates(const ViewSkins& views, const std::vector<uint32_t>& sfid, const FileReader& read, std::vector<std::string>& warnings) {
    std::vector<Candidate> candidates;
    for (size_t view = 0; view < views.fileDataIds.size(); view++) {
        candidates.push_back({ views.fileDataIds[view], static_cast<uint32_t>(view), false, views.originals[view] });
    }

    for (size_t at = views.fileDataIds.size(); at < sfid.size(); at++) {
        Candidate lod{ sfid[at], static_cast<uint32_t>(at - views.fileDataIds.size() + 1), true, {} };
        std::string why;
        if (!read(lod.fileDataId, lod.skin, why)) {
            warnings.push_back(Name(lod) + " is left out: it was not read (" + why + ")");
            continue;
        }
        if (!RebaseLod(lod.skin, why)) {
            warnings.push_back(Name(lod) + " is left out: it " + why);
            continue;
        }
        candidates.push_back(std::move(lod));
    }
    return candidates;
}

// The candidates the client can draw, at most four: each view of a set is split for its place in it, so a set that
// loses one is planned again.
static std::vector<const Candidate*> Drawable(Planner& planner, const std::vector<Candidate>& candidates) {
    std::vector<const Candidate*> fitting;
    for (const Candidate& candidate : candidates) {
        if (SectionsFit(SectionCounts(candidate.skin))) {
            fitting.push_back(&candidate);
        }
    }

    while (true) {
        std::vector<const Candidate*> kept(fitting.begin(), fitting.begin() + std::min<size_t>(fitting.size(), MAX_VIEWS));
        size_t unfit = planner.FirstUnfit(MembersOf(candidates, kept));
        if (unfit == kept.size()) {
            return kept;
        }
        fitting.erase(fitting.begin() + unfit);
    }
}

// "LOD skins 1, 2 (7001, 7002)", or each by name when views are among them.
static std::string Describe(const std::vector<const Candidate*>& kept) {
    bool lods = std::all_of(kept.begin(), kept.end(), [](const Candidate* candidate) { return candidate->lod; });
    std::string bands;
    std::string ids;
    std::string names;
    for (const Candidate* candidate : kept) {
        std::string separator = names.empty() ? "" : ", ";
        bands += separator + std::to_string(candidate->view);
        ids += separator + std::to_string(candidate->fileDataId);
        names += separator + Name(*candidate);
    }
    return lods ? (kept.size() == 1 ? "LOD skin " : "LOD skins ") + bands + " (" + ids + ")" : names;
}

bool ChooseViews(const std::vector<uint8_t>& m2, const std::vector<uint32_t>& sfid, const FileReader& read, ViewSkins& chosen, std::string& readError) {
    uint32_t views = sfid.empty() ? 0 : std::min<uint32_t>(Get<uint32_t>(m2, M2At::VIEWS), static_cast<uint32_t>(sfid.size()));
    chosen = {};
    chosen.fileDataIds.assign(sfid.begin(), sfid.begin() + views);
    chosen.originals.resize(views);

    for (uint32_t view = 0; view < views; view++) {
        std::string error;
        if (!read || !read(sfid[view], chosen.originals[view], error)) {
            readError = "skin " + std::to_string(view) + " (" + std::to_string(sfid[view]) + ") not read (" + error + ")";
            return false;
        }
    }

    Planner planner(m2);
    std::vector<Member> members;
    for (uint32_t view = 0; view < views; view++) {
        members.emplace_back(view, &chosen.originals[view]);
    }
    size_t unfit = planner.FirstUnfit(members);
    if (unfit == views) {
        planner.Plan(members, chosen);
        return true;
    }

    std::vector<Candidate> candidates = ReadCandidates(chosen, sfid, read, chosen.warnings);
    std::vector<const Candidate*> kept = Drawable(planner, candidates);
    std::string why = Name(candidates[unfit]) + " has " + std::to_string(Get<uint32_t>(candidates[unfit].skin, SkinAt::INDICES))
        + " corners, and its submeshes (split by bone where they must be) other than the largest pass 65,535 together: the 3.3.5 client keeps each "
        "submesh's start in 16 bits and draws such a view garbled";

    std::string reduced;
    if (kept.empty()) {
        // As the downporter, which reads the views and then LOD skins up to four, and reduces every one.
        for (size_t c = 0; c < std::min<size_t>(candidates.size(), MAX_VIEWS); c++) {
            kept.push_back(&candidates[c]);
        }
        reduced = ", whose submeshes other than the largest lose triangles by edge collapse until they fit";
        why += "; no view or LOD skin of the model fits";
    }

    chosen.warnings.push_back(why + "; the model keeps " + Describe(kept) + " as its views" + reduced);
    planner.Plan(MembersOf(candidates, kept), chosen);
    chosen.fileDataIds.clear();
    chosen.originals = SkinsOf(kept);
    for (const Candidate* candidate : kept) {
        chosen.fileDataIds.push_back(candidate->fileDataId);
    }
    return true;
}

}
