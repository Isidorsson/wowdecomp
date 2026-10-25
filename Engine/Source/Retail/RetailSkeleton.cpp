#include "Engine/Source/Retail/RetailSkeleton.h"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

constexpr size_t SKS1_SIZE = 32;
constexpr size_t TWO_ARRAYS_SIZE = 16;
constexpr size_t SKPD_SIZE = 16;
constexpr size_t SKPD_PARENT = 8;
constexpr size_t AFID_ROW_SIZE = 8;
// A longer chain is a loop or a corrupt file; retail skeletons have one parent at most.
constexpr size_t MAX_SKELETONS = 4;
constexpr size_t SKELETON_ATTACHMENT_SIZE = 0x28;
constexpr size_t EVENT_SIZE = 0x24;
constexpr size_t EVENT_ENABLED = 0x18;
constexpr size_t PEDC_TRACK_SIZE = 12;
constexpr uint16_t NO_LOOP = 0xFFFF;

using SequenceKey = std::pair<uint16_t, uint16_t>;

bool ParseSkeleton(const std::vector<uint8_t>& file, SkeletonFile& skeleton, std::string& error) {
    SkeletonFile parsed;
    std::set<std::string> seen;

    for (size_t at = 0; at + 8 <= file.size();) {
        std::string tag(reinterpret_cast<const char*>(file.data() + at), 4);
        size_t size = Get<uint32_t>(file, at + 4);
        if (size > file.size() - at - 8) {
            return Fail(error, "skeleton chunk " + tag + " runs past the end of the file");
        }

        if (!seen.insert(tag).second) {
            return Fail(error, "skeleton chunk " + tag + " appears twice");
        }

        std::vector<uint8_t> payload(file.begin() + at + 8, file.begin() + at + 8 + size);
        at += 8 + size;

        if (tag == "SKS1") {
            parsed.sequences = std::move(payload);
        } else if (tag == "SKB1") {
            parsed.bones = std::move(payload);
        } else if (tag == "SKA1") {
            parsed.attachments = std::move(payload);
        } else if (tag == "SKPD") {
            if (payload.size() < SKPD_SIZE) {
                return Fail(error, "the skeleton's SKPD header is truncated");
            }
            parsed.parent = Get<uint32_t>(payload, SKPD_PARENT);
        } else if (tag == "AFID") {
            if (payload.size() % AFID_ROW_SIZE) {
                return Fail(error, "the skeleton's AFID is not whole 8-byte rows");
            }
            parsed.afid = std::move(payload);
        }
    }

    for (const char* required : { "SKS1", "SKB1" }) {
        if (!seen.count(required)) {
            return Fail(error, std::string("the skeleton has no ") + required + " chunk");
        }
    }

    const char* truncated = parsed.sequences.size() < SKS1_SIZE ? "SKS1" : parsed.bones.size() < TWO_ARRAYS_SIZE ? "SKB1"
        : seen.count("SKA1") && parsed.attachments.size() < TWO_ARRAYS_SIZE ? "SKA1" : nullptr;
    if (truncated) {
        return Fail(error, std::string("the skeleton's ") + truncated + " header is truncated");
    }

    skeleton = std::move(parsed);
    return true;
}

bool LoadSkeletonChain(uint32_t fileDataId, const FileReader& read, std::vector<SkeletonFile>& chain, std::string& error) {
    std::vector<SkeletonFile> loaded;
    std::vector<uint32_t> ids;

    for (uint32_t id = fileDataId; id;) {
        std::string name = ids.empty() ? "skeleton " + std::to_string(id) : "parent skeleton " + std::to_string(id) + " of skeleton " + std::to_string(ids.back());
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
            return Fail(error, name + ": its chain of parent skeletons leads back to itself");
        }

        if (ids.size() >= MAX_SKELETONS) {
            return Fail(error, name + ": more than " + std::to_string(MAX_SKELETONS) + " skeletons in the chain of parents");
        }

        std::vector<uint8_t> bytes;
        std::string readError;
        if (!read || !read(id, bytes, readError)) {
            return Fail(error, name + " not read (" + readError + ")");
        }

        SkeletonFile file;
        if (!ParseSkeleton(bytes, file, error)) {
            error += " (" + name + ")";
            return false;
        }

        ids.push_back(id);
        id = file.parent;
        loaded.push_back(std::move(file));
    }

    chain = std::move(loaded);
    return true;
}

static SequenceKey KeyOf(const std::vector<uint8_t>& records, size_t s) {
    return { Get<uint16_t>(records, s * SEQUENCE_SIZE + SequenceAt::ID), Get<uint16_t>(records, s * SEQUENCE_SIZE + SequenceAt::VARIATION) };
}

static uint32_t FlagsOf(const std::vector<uint8_t>& records, size_t s) {
    return Get<uint32_t>(records, s * SEQUENCE_SIZE + SequenceAt::FLAGS);
}

// The sequence whose keys s plays: s, or the end of its alias chain.
static bool ResolveAlias(const std::vector<uint8_t>& records, uint32_t s, uint32_t& owner, std::string& error) {
    uint32_t count = static_cast<uint32_t>(records.size() / SEQUENCE_SIZE);
    owner = s;
    for (uint32_t steps = 0; FlagsOf(records, owner) & SEQUENCE_ALIAS; steps++) {
        owner = Get<uint16_t>(records, owner * SEQUENCE_SIZE + SequenceAt::ALIAS_NEXT);
        if (steps >= count || owner >= count) {
            return Fail(error, "the alias chain of skeleton sequence " + std::to_string(s) + " loops or leaves the sequence table");
        }
    }
    return true;
}

// The first AFID row of key; 0 when there is none, or it names no file.
static uint32_t AnimFileOf(const std::vector<uint8_t>& afid, SequenceKey key) {
    for (size_t at = 0; at + AFID_ROW_SIZE <= afid.size(); at += AFID_ROW_SIZE) {
        if (Get<uint16_t>(afid, at) == key.first && Get<uint16_t>(afid, at + 2) == key.second) {
            return Get<uint32_t>(afid, at + 4);
        }
    }
    return 0;
}

// A skeleton owns a sequence's keys when they are in its own chunks or its AFID names a file for them.
static bool OwnedSequences(const std::vector<uint8_t>& records, const std::vector<uint8_t>& afid, std::vector<bool>& owned, std::string& error) {
    owned.assign(records.size() / SEQUENCE_SIZE, false);
    for (uint32_t s = 0; s < owned.size(); s++) {
        uint32_t owner;
        if (!ResolveAlias(records, s, owner, error)) {
            return false;
        }
        owned[s] = (FlagsOf(records, owner) & SEQUENCE_LOCAL_BITS) || AnimFileOf(afid, KeyOf(records, owner));
    }
    return true;
}

static bool OwnSequences(const SkeletonFile& file, uint32_t level, MergedSequences& own, std::string& error) {
    Span sequences;
    Span lookup;
    if (!ReadSpan(file.sequences, 8, SEQUENCE_SIZE, "skeleton sequences", sequences, error) || !ReadSpan(file.sequences, 16, 2, "skeleton sequence lookup", lookup, error)) {
        return false;
    }

    own.records.assign(file.sequences.begin() + sequences.offset, file.sequences.begin() + sequences.offset + sequences.count * SEQUENCE_SIZE);
    own.lookup.resize(lookup.count);
    for (uint32_t i = 0; i < lookup.count; i++) {
        own.lookup[i] = Get<uint16_t>(file.sequences, lookup.At(i, 2));
    }

    own.own = sequences.count;
    for (uint32_t i = 0; i < sequences.count; i++) {
        own.sources.push_back({ level, i });
    }
    own.parentIndex.assign(sequences.count, -1);
    return true;
}

// The appended ids become findable. A lookup that listed nothing lists everything once something is appended, as
// the client would otherwise find only the appended ids.
static bool CompleteLookup(MergedSequences& merged) {
    uint32_t count = static_cast<uint32_t>(merged.sources.size());
    if (count == merged.own) {
        return true;
    }

    if (count > 0x7FFF) {
        return false;
    }

    for (uint32_t s = merged.lookup.empty() ? 0 : merged.own; s < count; s++) {
        uint16_t id = Get<uint16_t>(merged.records, s * SEQUENCE_SIZE + SequenceAt::ID);
        if (merged.lookup.size() <= id) {
            merged.lookup.resize(id + 1u, NO_INDEX);
        }
        if (merged.lookup[id] == NO_INDEX) {
            merged.lookup[id] = static_cast<uint16_t>(s);
        }
    }
    return true;
}

// The parent's sequences child lacks, appended with their variation and alias links pointing into the merged list.
static bool AppendParentSequences(MergedSequences& child, const MergedSequences& parent, std::string& error) {
    std::map<SequenceKey, uint32_t> childKeys;
    for (uint32_t s = 0; s < child.own; s++) {
        childKeys.emplace(KeyOf(child.records, s), s);
    }

    std::vector<uint32_t> appended;
    for (uint32_t p = 0; p < parent.sources.size(); p++) {
        if (!childKeys.count(KeyOf(parent.records, p))) {
            appended.push_back(p);
        }
    }

    auto mergedIndex = [&](uint32_t p) -> int32_t {
        if (p >= parent.sources.size()) {
            return -1;
        }
        auto own = childKeys.find(KeyOf(parent.records, p));
        if (own != childKeys.end()) {
            return static_cast<int32_t>(own->second);
        }
        auto position = std::find(appended.begin(), appended.end(), p);
        return position == appended.end() ? -1 : static_cast<int32_t>(child.own + (position - appended.begin()));
    };

    for (uint32_t p : appended) {
        std::vector<uint8_t> record(parent.records.begin() + p * SEQUENCE_SIZE, parent.records.begin() + (p + 1) * SEQUENCE_SIZE);
        int16_t next = static_cast<int16_t>(Get<uint16_t>(record, SequenceAt::VARIATION_NEXT));
        int32_t nextIndex = next >= 0 ? mergedIndex(static_cast<uint32_t>(next)) : -1;
        int32_t aliasIndex = mergedIndex(Get<uint16_t>(record, SequenceAt::ALIAS_NEXT));
        bool alias = Get<uint32_t>(record, SequenceAt::FLAGS) & SEQUENCE_ALIAS;

        if ((next >= 0 && nextIndex < 0) || (alias && aliasIndex < 0)) {
            return Fail(error, "parent skeleton sequence " + std::to_string(p) + " links to a sequence the parent skeleton does not have");
        }

        if (next >= 0) {
            Put<uint16_t>(record, SequenceAt::VARIATION_NEXT, static_cast<uint16_t>(nextIndex));
        }
        Put<uint16_t>(record, SequenceAt::ALIAS_NEXT, static_cast<uint16_t>(aliasIndex >= 0 ? aliasIndex : int32_t(child.sources.size())));
        child.records.insert(child.records.end(), record.begin(), record.end());
        child.sources.push_back(parent.sources[p]);
        child.parentIndex.push_back(static_cast<int32_t>(p));
    }

    if (!CompleteLookup(child)) {
        return Fail(error, std::to_string(child.sources.size()) + " merged sequences do not fit the id lookup");
    }
    return true;
}

// Sequences the child does not own take the keys of the parent's sequence of the same id and variation.
static bool Inherit(const SkeletonFile& file, MergedSequences& child, const MergedSequences& parent, std::string& error) {
    std::vector<bool> owned;
    if (!OwnedSequences(child.records, file.afid, owned, error)) {
        return false;
    }

    std::map<SequenceKey, uint32_t> parentKeys;
    for (uint32_t p = 0; p < parent.sources.size(); p++) {
        parentKeys.emplace(KeyOf(parent.records, p), p);
    }

    for (uint32_t s = 0; s < child.own; s++) {
        auto inherited = parentKeys.find(KeyOf(child.records, s));
        if (!owned[s] && inherited != parentKeys.end()) {
            child.sources[s] = parent.sources[inherited->second];
            child.parentIndex[s] = static_cast<int32_t>(inherited->second);
        }
    }

    child.parentCount = static_cast<uint32_t>(parent.sources.size());
    return AppendParentSequences(child, parent, error);
}

bool MergeSequences(const std::vector<SkeletonFile>& chain, MergedSequences& merged, std::string& error) {
    if (chain.empty()) {
        return Fail(error, "no skeleton to merge");
    }

    MergedSequences result;
    for (size_t level = chain.size(); level-- > 0;) {
        MergedSequences own;
        if (!OwnSequences(chain[level], static_cast<uint32_t>(level), own, error) || (level + 1 < chain.size() && !Inherit(chain[level], own, result, error))) {
            return false;
        }
        result = std::move(own);
    }

    merged = std::move(result);
    return true;
}

namespace {

enum Part { BONE_KEYS, ATTACHMENT_KEYS };

constexpr const char* PART_CHUNK[] = { "AFSB", "AFSA" };

struct SkeletonTrack {
    size_t offset;
    size_t keySize;
    const char* name;
};

const SkeletonTrack BONE_TRACKS[] = { { 0x10, 12, "translation" }, { 0x24, 8, "rotation" }, { 0x38, 12, "scale" } };
const SkeletonTrack ATTACHMENT_TRACK = { 0x14, 1, "animate-attached" };

// The two key parts of one .anim file a skeleton reads, and where each went in the model once appended.
struct AnimParts {
    bool read = false;
    std::vector<uint8_t> parts[2];
    bool has[2] = {};
    int64_t base[2] = { -1, -1 };
};

// Where the keys of one sequence of one level sit: the bytes its entries count from and their place in the model.
struct KeySource {
    const std::vector<uint8_t>* bytes = nullptr;   // None: the keys are missing, and the sequence plays the bind pose.
    uint32_t base = 0;
    const char* lacking = nullptr;                 // The chunk the .anim file would need for any keys.
};

// A track inside a level's chunk; bytes is null when the level has no counterpart record.
struct TrackRef {
    const std::vector<uint8_t>* bytes = nullptr;
    size_t at = 0;
};

struct Merge {
    std::vector<uint8_t>& m2;
    const std::vector<SkeletonFile>& chain;
    const FileReader& read;
    std::vector<std::string>& warnings;
    MergedSequences sequences;
    std::vector<uint32_t> ids;                          // The FileDataID of each level.
    std::vector<std::vector<uint8_t>> levelRecords;     // Each level's own sequences.
    std::vector<Span> levelBones;
    std::vector<Span> levelAttachments;
    std::map<std::pair<uint32_t, int>, uint32_t> chunkBases;
    std::map<uint32_t, AnimParts> anims;
    std::map<std::tuple<uint32_t, uint32_t, int>, KeySource> located;
    std::set<std::pair<uint32_t, uint32_t>> unnamed;
};

const std::vector<uint8_t>& ChunkOf(const SkeletonFile& file, int part) {
    return part == BONE_KEYS ? file.bones : file.attachments;
}

// Where the level's chunk went in the model, appending it the first time.
uint32_t ChunkBase(Merge& merge, uint32_t level, int part) {
    auto known = merge.chunkBases.find({ level, part });
    if (known != merge.chunkBases.end()) {
        return known->second;
    }
    uint32_t base = AppendAligned(merge.m2, ChunkOf(merge.chain[level], part));
    merge.chunkBases[{ level, part }] = base;
    return base;
}

std::string SequenceName(SequenceKey key) {
    return "animation " + std::to_string(key.first) + "." + std::to_string(key.second);
}

// Bone keys come from AFSB, else AFM2, else the whole of an unchunked file; attachment keys only from AFSA
// (downport/read/anim_file.rs).
void SplitAnim(const std::vector<uint8_t>& file, AnimParts& anim) {
    bool chunked = file.size() >= 4 && (!std::memcmp(file.data(), "AFM2", 4) || !std::memcmp(file.data(), "AFSA", 4) || !std::memcmp(file.data(), "AFSB", 4));
    if (!chunked) {
        anim.parts[BONE_KEYS] = file;
        anim.has[BONE_KEYS] = true;
        return;
    }

    auto chunks = ReadChunks(file);
    for (const char* tag : { "AFSB", "AFM2" }) {
        auto chunk = chunks.find(tag);
        if (chunk != chunks.end() && !anim.has[BONE_KEYS]) {
            anim.parts[BONE_KEYS] = chunk->second;
            anim.has[BONE_KEYS] = true;
        }
    }

    auto attachments = chunks.find("AFSA");
    if (attachments != chunks.end()) {
        anim.parts[ATTACHMENT_KEYS] = attachments->second;
        anim.has[ATTACHMENT_KEYS] = true;
    }
}

// The .anim parts of id, read the first time.
AnimParts& AnimOf(Merge& merge, uint32_t id, SequenceKey key, const std::string& skeleton) {
    AnimParts& anim = merge.anims[id];
    if (anim.read) {
        return anim;
    }

    std::vector<uint8_t> bytes;
    std::string readError;
    anim.read = true;
    if (!merge.read || !merge.read(id, bytes, readError)) {
        merge.warnings.push_back(SequenceName(key) + ": .anim " + std::to_string(id) + " of " + skeleton + " not read, so its bones play the bind pose (" + readError + ")");
    } else {
        SplitAnim(bytes, anim);
    }
    return anim;
}

bool LocateKeys(Merge& merge, SequenceSource source, int part, KeySource& keys, std::string& error) {
    auto known = merge.located.find({ source.level, source.index, part });
    if (known != merge.located.end()) {
        keys = known->second;
        return true;
    }

    const std::vector<uint8_t>& records = merge.levelRecords[source.level];
    uint32_t owner;
    if (!ResolveAlias(records, source.index, owner, error)) {
        return false;
    }

    keys = KeySource();
    SequenceKey key = KeyOf(records, owner);
    std::string skeleton = "skeleton " + std::to_string(merge.ids[source.level]);
    uint32_t id = AnimFileOf(merge.chain[source.level].afid, key);

    if (FlagsOf(records, owner) & SEQUENCE_LOCAL_BITS) {
        keys.bytes = &ChunkOf(merge.chain[source.level], part);
        keys.base = ChunkBase(merge, source.level, part);
    } else if (!id) {
        if (merge.unnamed.insert({ source.level, owner }).second) {
            merge.warnings.push_back(SequenceName(key) + ": " + skeleton + " names no .anim file for it, so its bones play the bind pose");
        }
    } else {
        AnimParts& anim = AnimOf(merge, id, key, skeleton);
        if (anim.has[part]) {
            if (anim.base[part] < 0) {
                anim.base[part] = AppendAligned(merge.m2, anim.parts[part]);
            }
            keys.bytes = &anim.parts[part];
            keys.base = static_cast<uint32_t>(anim.base[part]);
        } else if (anim.has[BONE_KEYS] || anim.has[ATTACHMENT_KEYS]) {
            keys.lacking = PART_CHUNK[part];
        }
    }

    merge.located[{ source.level, source.index, part }] = keys;
    return true;
}

// The per-sequence runs of one track at one level.
struct LevelTrack {
    bool animated = false;
    uint16_t interpolation = 0;
    Span times;
    Span keys;
};

bool ReadLevelTrack(const Merge& merge, uint32_t level, const TrackRef& ref, const SkeletonTrack& field, const std::string& what, LevelTrack& track, std::string& error) {
    if (!ref.bytes || Get<uint16_t>(*ref.bytes, ref.at + TrackAt::GLOBAL_SEQUENCE) != NO_LOOP) {
        return true;
    }

    if (!ReadSpan(*ref.bytes, ref.at + TrackAt::TIMES, 8, "skeleton track timestamps", track.times, error)
        || (field.keySize && !ReadSpan(*ref.bytes, ref.at + TrackAt::KEYS, 8, "skeleton track keys", track.keys, error))) {
        return false;
    }

    size_t sequences = merge.levelRecords[level].size() / SEQUENCE_SIZE;
    if ((track.times.count && track.times.count != sequences) || (field.keySize && track.keys.count != track.times.count)) {
        return Fail(error, what + ": " + std::to_string(track.times.count) + " runs of keys but skeleton " + std::to_string(merge.ids[level]) + " has "
            + std::to_string(sequences) + " sequences");
    }

    track.animated = track.times.count > 0;
    track.interpolation = Get<uint16_t>(*ref.bytes, ref.at + TrackAt::INTERPOLATION);
    return true;
}

// One (count, offset) entry rebased onto its keys' place in the model, refused when it runs past their bytes.
bool RebaseEntry(uint32_t& count, uint32_t& offset, size_t size, const KeySource& keys, const std::string& what, std::string& error) {
    if (!count) {
        offset = 0;
        return true;
    }

    if (keys.lacking) {
        return Fail(error, what + ": its keys are in an .anim file that has no " + keys.lacking + " chunk");
    }

    if (uint64_t(offset) + uint64_t(count) * size > keys.bytes->size()) {
        return Fail(error, what + ": a track's keys run past the end of their .anim file or skeleton chunk");
    }

    offset += keys.base;
    return true;
}

// A track looping on a global sequence keeps its own runs, whose keys are in the chunk of its record.
bool RebaseLoopTrack(Merge& merge, size_t out, const TrackRef& child, const SkeletonTrack& field, int part, const std::string& what, std::string& error) {
    KeySource keys { child.bytes, ChunkBase(merge, 0, part), nullptr };
    for (size_t table : { TrackAt::TIMES, TrackAt::KEYS }) {
        Span span;
        if (table == TrackAt::KEYS && !field.keySize) {
            continue;
        }

        if (!ReadSpan(*child.bytes, child.at + table, 8, "skeleton track runs", span, error)) {
            return false;
        }

        size_t size = table == TrackAt::TIMES ? 4 : field.keySize;
        for (uint32_t r = 0; r < span.count; r++) {
            uint32_t count = Get<uint32_t>(*child.bytes, span.At(r, 8));
            uint32_t offset = Get<uint32_t>(*child.bytes, span.At(r, 8) + 4);
            if (!RebaseEntry(count, offset, size, keys, what, error)) {
                return false;
            }
            Put<uint32_t>(merge.m2, keys.base + span.At(r, 8) + 4, offset);
        }
        Put<uint32_t>(merge.m2, out + table + 4, span.count ? span.offset + keys.base : 0);
    }
    return true;
}

// The keys sequence s plays in one track: the entries of the level that supplies it, rebased.
bool MergedEntry(Merge& merge, const LevelTrack& track, const TrackRef& ref, SequenceSource source, const SkeletonTrack& field, int part, const std::string& what,
    uint32_t (&entry)[4], bool& keyed, std::string& error) {
    const std::vector<uint8_t>& bytes = *ref.bytes;
    entry[0] = Get<uint32_t>(bytes, track.times.At(source.index, 8));
    entry[1] = Get<uint32_t>(bytes, track.times.At(source.index, 8) + 4);
    entry[2] = field.keySize ? Get<uint32_t>(bytes, track.keys.At(source.index, 8)) : 0;
    entry[3] = field.keySize ? Get<uint32_t>(bytes, track.keys.At(source.index, 8) + 4) : 0;

    KeySource keys;
    keyed = false;
    if (!entry[0] && !entry[2]) {
        return true;
    }

    if (!LocateKeys(merge, source, part, keys, error)) {
        return false;
    }

    if (!keys.bytes && !keys.lacking) {
        return true;
    }

    keyed = true;
    return RebaseEntry(entry[0], entry[1], 4, keys, what, error) && RebaseEntry(entry[2], entry[3], field.keySize, keys, what, error);
}

// The track at out (the child's, in the model) with one run per merged sequence, each from the level that supplies
// the sequence; levels[l] is that level's counterpart track. The nearest animated level sets the interpolation,
// which every level whose keys are taken must share (downport/read/skeleton/merge.rs).
bool MergeTrack(Merge& merge, size_t out, const std::vector<TrackRef>& levels, const SkeletonTrack& field, int part, const std::string& what, std::string& error) {
    if (Get<uint16_t>(*levels[0].bytes, levels[0].at + TrackAt::GLOBAL_SEQUENCE) != NO_LOOP) {
        return RebaseLoopTrack(merge, out, levels[0], field, part, what, error);
    }

    std::vector<LevelTrack> tracks(levels.size());
    for (uint32_t level = 0; level < levels.size(); level++) {
        if (!ReadLevelTrack(merge, level, levels[level], field, what, tracks[level], error)) {
            return false;
        }
    }

    auto first = std::find_if(tracks.begin(), tracks.end(), [](const LevelTrack& track) { return track.animated; });
    if (first == tracks.end()) {
        return true;
    }

    uint16_t interpolation = first->interpolation;
    uint32_t count = static_cast<uint32_t>(merge.sequences.sources.size());
    std::vector<uint8_t> times(count * 8, 0);
    std::vector<uint8_t> keys(count * 8, 0);

    for (uint32_t s = 0; s < count; s++) {
        SequenceSource source = merge.sequences.sources[s];
        const LevelTrack& track = tracks[source.level];
        uint32_t entry[4];
        bool keyed;
        if (!track.animated) {
            continue;
        }

        if (!MergedEntry(merge, track, levels[source.level], source, field, part, what, entry, keyed, error)) {
            return false;
        }

        if (keyed && track.interpolation != interpolation) {
            return Fail(error, what + ": skeleton " + std::to_string(merge.ids[first - tracks.begin()]) + " interpolates it with type " + std::to_string(interpolation)
                + " but skeleton " + std::to_string(merge.ids[source.level]) + " with type " + std::to_string(track.interpolation) + ", so their keys cannot be mixed");
        }

        if (keyed) {
            for (size_t i = 0; i < 2; i++) {
                Put<uint32_t>(times, s * 8 + i * 4, entry[i]);
                Put<uint32_t>(keys, s * 8 + i * 4, entry[2 + i]);
            }
        }
    }

    Put<uint16_t>(merge.m2, out + TrackAt::INTERPOLATION, interpolation);
    Put<uint32_t>(merge.m2, out + TrackAt::TIMES, count);
    Put<uint32_t>(merge.m2, out + TrackAt::TIMES + 4, AppendAligned(merge.m2, times));
    if (field.keySize) {
        Put<uint32_t>(merge.m2, out + TrackAt::KEYS, count);
        Put<uint32_t>(merge.m2, out + TrackAt::KEYS + 4, AppendAligned(merge.m2, keys));
    }
    return true;
}

bool ReadLevels(Merge& merge, std::string& error) {
    for (uint32_t level = 0; level < merge.chain.size(); level++) {
        const SkeletonFile& file = merge.chain[level];
        Span sequences;
        Span bones;
        Span attachments;
        if (!ReadSpan(file.sequences, 8, SEQUENCE_SIZE, "skeleton sequences", sequences, error) || !ReadSpan(file.bones, 0, BONE_SIZE, "skeleton bones", bones, error)
            || (!file.attachments.empty() && !ReadSpan(file.attachments, 0, SKELETON_ATTACHMENT_SIZE, "skeleton attachments", attachments, error))) {
            error += " (skeleton " + std::to_string(merge.ids[level]) + ")";
            return false;
        }

        merge.levelRecords.emplace_back(file.sequences.begin() + sequences.offset, file.sequences.begin() + sequences.offset + sequences.count * SEQUENCE_SIZE);
        merge.levelBones.push_back(bones);
        merge.levelAttachments.push_back(attachments);
    }
    return true;
}

// Points the M2Array at header to the lookup at its offset in the child chunk appended at base.
bool PutLookup(Merge& merge, const std::vector<uint8_t>& chunk, uint32_t base, size_t header, const char* name, std::string& error) {
    Span lookup;
    if (!ReadSpan(chunk, 8, 2, name, lookup, error)) {
        return false;
    }
    Put<uint32_t>(merge.m2, header, lookup.count);
    Put<uint32_t>(merge.m2, header + 4, lookup.count ? base + lookup.offset : 0);
    return true;
}

// Bones match their counterparts by index.
bool MergeBones(Merge& merge, std::string& error) {
    const Span& bones = merge.levelBones[0];
    uint32_t base = ChunkBase(merge, 0, BONE_KEYS);

    for (uint32_t b = 0; b < bones.count; b++) {
        for (const SkeletonTrack& field : BONE_TRACKS) {
            std::vector<TrackRef> levels(merge.chain.size());
            for (uint32_t level = 0; level < merge.chain.size(); level++) {
                if (b < merge.levelBones[level].count) {
                    levels[level] = { &merge.chain[level].bones, merge.levelBones[level].At(b, BONE_SIZE) + field.offset };
                }
            }

            if (!MergeTrack(merge, base + levels[0].at, levels, field, BONE_KEYS, "bone " + std::to_string(b) + " " + field.name, error)) {
                return false;
            }
        }
    }

    Put<uint32_t>(merge.m2, M2At::BONES, bones.count);
    Put<uint32_t>(merge.m2, M2At::BONES + 4, bones.count ? base + bones.offset : 0);
    return PutLookup(merge, merge.chain[0].bones, base, M2At::KEY_BONE_LOOKUP, "skeleton key bone lookup", error);
}

// The record of the attachment with id at level; bytes null when the level has none.
TrackRef AttachmentOf(const Merge& merge, uint32_t level, uint32_t id) {
    const Span& attachments = merge.levelAttachments[level];
    const std::vector<uint8_t>& chunk = merge.chain[level].attachments;
    for (uint32_t a = 0; a < attachments.count; a++) {
        if (Get<uint32_t>(chunk, attachments.At(a, SKELETON_ATTACHMENT_SIZE)) == id) {
            return { &chunk, attachments.At(a, SKELETON_ATTACHMENT_SIZE) + ATTACHMENT_TRACK.offset };
        }
    }
    return {};
}

// Attachments match their counterparts by attachment id.
bool MergeAttachments(Merge& merge, std::string& error) {
    const SkeletonFile& child = merge.chain[0];
    const Span& attachments = merge.levelAttachments[0];
    if (child.attachments.empty()) {
        return true;
    }

    uint32_t base = ChunkBase(merge, 0, ATTACHMENT_KEYS);
    for (uint32_t a = 0; a < attachments.count; a++) {
        uint32_t id = Get<uint32_t>(child.attachments, attachments.At(a, SKELETON_ATTACHMENT_SIZE));
        std::vector<TrackRef> levels(merge.chain.size());
        for (uint32_t level = 1; level < merge.chain.size(); level++) {
            levels[level] = AttachmentOf(merge, level, id);
        }

        levels[0] = { &child.attachments, attachments.At(a, SKELETON_ATTACHMENT_SIZE) + ATTACHMENT_TRACK.offset };
        if (!MergeTrack(merge, base + levels[0].at, levels, ATTACHMENT_TRACK, ATTACHMENT_KEYS, "attachment " + std::to_string(id), error)) {
            return false;
        }
    }

    Put<uint32_t>(merge.m2, M2At::ATTACHMENTS, attachments.count);
    Put<uint32_t>(merge.m2, M2At::ATTACHMENTS + 4, attachments.count ? base + attachments.offset : 0);
    return PutLookup(merge, child.attachments, base, M2At::ATTACHMENT_LOOKUP, "skeleton attachment lookup", error);
}

void PutArray(std::vector<uint8_t>& m2, size_t header, uint32_t count, const std::vector<uint8_t>& bytes) {
    Put<uint32_t>(m2, header, count);
    Put<uint32_t>(m2, header + 4, count ? AppendAligned(m2, bytes) : 0);
}

bool PutSequences(Merge& merge, std::string& error) {
    const std::vector<uint8_t>& sks1 = merge.chain[0].sequences;
    Span loops;
    if (!ReadSpan(sks1, 0, 4, "skeleton global loops", loops, error)) {
        return false;
    }

    std::vector<uint8_t> lookup;
    for (uint16_t value : merge.sequences.lookup) {
        lookup.push_back(static_cast<uint8_t>(value));
        lookup.push_back(static_cast<uint8_t>(value >> 8));
    }

    PutArray(merge.m2, M2At::GLOBAL_LOOPS, loops.count, std::vector<uint8_t>(sks1.begin() + loops.offset, sks1.begin() + loops.offset + loops.count * 4));
    PutArray(merge.m2, M2At::SEQUENCES, static_cast<uint32_t>(merge.sequences.sources.size()), merge.sequences.records);
    PutArray(merge.m2, M2At::SEQUENCE_LOOKUP, static_cast<uint32_t>(merge.sequences.lookup.size()), lookup);
    return true;
}

}

bool MergeSkeleton(std::vector<uint8_t>& m2, const std::vector<SkeletonFile>& chain, const FileReader& read, ModelSkeleton& skeleton,
    std::vector<std::string>& warnings, std::string& error) {
    Merge merge { m2, chain, read, warnings };
    merge.ids.push_back(skeleton.fileDataId);
    for (size_t level = 0; level + 1 < chain.size(); level++) {
        merge.ids.push_back(chain[level].parent);
    }

    if (!MergeSequences(chain, merge.sequences, error) || !ReadLevels(merge, error) || !MergeBones(merge, error) || !MergeAttachments(merge, error)
        || !PutSequences(merge, error)) {
        return false;
    }

    skeleton.parents.assign(merge.ids.begin() + 1, merge.ids.end());
    skeleton.own = merge.sequences.own;
    skeleton.parentCount = merge.sequences.parentCount;
    skeleton.parentIndex = merge.sequences.parentIndex;
    skeleton.afid.clear();
    for (const SkeletonFile& file : chain) {
        skeleton.afid.insert(skeleton.afid.end(), file.afid.begin(), file.afid.end());
    }
    return true;
}

// Why the PEDC tracks cannot be matched to the events and the parent's sequences; empty when they can.
static std::string ParentEventsMisfit(const std::vector<uint8_t>& pedc, uint32_t events, uint32_t parentCount, Span& tracks) {
    std::string error;
    if (pedc.size() < 8 || !ReadSpan(pedc, 0, PEDC_TRACK_SIZE, "PEDC tracks", tracks, error)) {
        return "PEDC tracks run past the chunk";
    }

    if (tracks.count != events) {
        return "PEDC has " + std::to_string(tracks.count) + " event tracks but the model has " + std::to_string(events) + " events";
    }

    for (uint32_t t = 0; t < tracks.count; t++) {
        size_t track = tracks.At(t, PEDC_TRACK_SIZE);
        uint16_t loop = Get<uint16_t>(pedc, track + 2);
        Span runs;
        if (!ReadSpan(pedc, track + 4, 8, "PEDC runs", runs, error)) {
            return "PEDC track " + std::to_string(t) + " runs past the chunk";
        }

        if (loop != NO_LOOP || runs.count != parentCount) {
            return "PEDC track " + std::to_string(t) + " has " + std::to_string(runs.count) + " runs (global sequence " + std::to_string(int16_t(loop))
                + ") but the parent skeleton has " + std::to_string(parentCount) + " sequences";
        }

        for (uint32_t r = 0; r < runs.count; r++) {
            if (uint64_t(Get<uint32_t>(pedc, runs.At(r, 8) + 4)) + uint64_t(Get<uint32_t>(pedc, runs.At(r, 8))) * 4 > pedc.size()) {
                return "PEDC track " + std::to_string(t) + " has timestamps past the chunk";
            }
        }
    }

    return {};
}

bool ApplyParentEvents(std::vector<uint8_t>& m2, const std::vector<uint8_t>& pedc, const ModelSkeleton& skeleton, std::vector<std::string>& warnings, std::string& error) {
    Span events;
    Span tracks;
    if (pedc.empty()) {
        return true;
    }

    if (!ReadSpan(m2, M2At::EVENTS, EVENT_SIZE, "events", events, error)) {
        return false;
    }

    std::string misfit = ParentEventsMisfit(pedc, events.count, skeleton.parentCount, tracks);
    if (!misfit.empty()) {
        warnings.push_back(misfit + ", so the sequences from the parent skeleton keep no event timings");
        return true;
    }

    uint32_t base = AppendAligned(m2, pedc);
    uint32_t count = static_cast<uint32_t>(skeleton.parentIndex.size());

    for (uint32_t e = 0; e < events.count; e++) {
        size_t track = events.At(e, EVENT_SIZE) + EVENT_ENABLED;
        Span times;
        if (Get<uint16_t>(m2, track + TrackAt::GLOBAL_SEQUENCE) != NO_LOOP) {
            continue;
        }

        if (!ReadSpan(m2, track + TrackAt::TIMES, 8, "event timestamps", times, error)) {
            return false;
        }

        if (!times.count) {
            times = { count, AppendAligned(m2, std::vector<uint8_t>(count * 8, 0)) };
            Put<uint32_t>(m2, track + TrackAt::TIMES, times.count);
            Put<uint32_t>(m2, track + TrackAt::TIMES + 4, times.offset);
        }

        if (times.count != count) {
            continue;
        }

        Span runs { Get<uint32_t>(pedc, tracks.At(e, PEDC_TRACK_SIZE) + 4), Get<uint32_t>(pedc, tracks.At(e, PEDC_TRACK_SIZE) + 8) };
        for (uint32_t s = 0; s < count; s++) {
            int32_t parent = skeleton.parentIndex[s];
            uint32_t keys = parent >= 0 ? Get<uint32_t>(pedc, runs.At(parent, 8)) : 0;
            if (keys && !Get<uint32_t>(m2, times.At(s, 8))) {
                Put<uint32_t>(m2, times.At(s, 8), keys);
                Put<uint32_t>(m2, times.At(s, 8) + 4, base + Get<uint32_t>(pedc, runs.At(parent, 8) + 4));
            }
        }
    }

    return true;
}

}
