#include <map>
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RetailSkeleton.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ModelSources;
using Retail::SkeletonFile;

constexpr size_t BONE_TRANSLATION = 0x10;
constexpr size_t ATTACHMENT_SIZE = 0x28;
constexpr size_t ATTACHMENT_ANIMATED = 0x14;
constexpr size_t EVENT_SIZE = 0x24;
constexpr size_t EVENT_ENABLED = 0x18;
constexpr size_t COLOR_SIZE = 0x28;

static size_t Append(std::vector<uint8_t>& blob, const std::vector<uint8_t>& bytes) {
    blob.resize((blob.size() + 15) & ~size_t(15), 0);
    size_t at = blob.size();
    blob.insert(blob.end(), bytes.begin(), bytes.end());
    return at;
}

static std::vector<uint8_t> Floats(float x) {
    std::vector<uint8_t> bytes(12);
    for (size_t i = 0; i < 3; i++) {
        std::memcpy(bytes.data() + i * 4, &x, 4);
    }
    return bytes;
}

static std::vector<uint8_t> U16s(const std::vector<uint16_t>& values) {
    std::vector<uint8_t> bytes;
    for (uint16_t value : values) {
        bytes.push_back(static_cast<uint8_t>(value));
        bytes.push_back(static_cast<uint8_t>(value >> 8));
    }
    return bytes;
}

static Retail::FileReader Files(std::map<uint32_t, std::vector<uint8_t>> files) {
    return [files](uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
        auto file = files.find(id);
        if (file == files.end()) {
            error = "no such file";
            return false;
        }
        bytes = file->second;
        return true;
    };
}

static ModelSources Sources(std::map<uint32_t, std::vector<uint8_t>> files) {
    ModelSources sources;
    sources.read = Files(std::move(files));
    return sources;
}

// An SKS1 payload: the header, then the sequences and the id lookup.
static std::vector<uint8_t> Sks1(std::vector<std::vector<uint8_t>> sequences, std::vector<uint16_t> lookup = {}) {
    std::vector<uint8_t> payload(32, 0);
    AddArray(payload, 8, static_cast<uint32_t>(sequences.size()), Concat(sequences));
    AddArray(payload, 16, static_cast<uint32_t>(lookup.size()), U16s(lookup));
    return payload;
}

// The keys of one sequence of an .anim part: timestamp 0 at offset 0, then one 12-byte translation key of x.
static std::vector<uint8_t> AnimPart(float x) {
    return Concat({ U32s({ 0 }), Floats(x) });
}

// Per-sequence runs of one key: 0 is an empty run, a positive x a key kept in the chunk, a negative x a key
// of -x in the sequence's .anim part (AnimPart).
static TrackEntries Runs(std::vector<uint8_t>& payload, const std::vector<float>& runs, size_t keySize) {
    TrackEntries entries;
    for (float x : runs) {
        if (x == 0) {
            entries.times.push_back({ 0, 0 });
            entries.keys.push_back({ 0, 0 });
        } else if (x < 0) {
            entries.times.push_back({ 1, 0 });
            entries.keys.push_back({ 1, 4 });
        } else {
            uint32_t times = static_cast<uint32_t>(Append(payload, U32s({ 0 })));
            std::vector<uint8_t> key = keySize == 12 ? Floats(x) : std::vector<uint8_t>(keySize, static_cast<uint8_t>(x));
            entries.times.push_back({ 1, times });
            entries.keys.push_back({ 1, static_cast<uint32_t>(Append(payload, key)) });
        }
    }
    return entries;
}

// An SKB1 payload: one bone per run list, its translation track interpolated by interpolation; key bone lookup [0].
static std::vector<uint8_t> Skb1(std::vector<std::vector<float>> bones, uint16_t interpolation = 1) {
    std::vector<uint8_t> payload(16, 0);
    size_t at = AddArray(payload, 0, static_cast<uint32_t>(bones.size()), Concat(std::vector<std::vector<uint8_t>>(bones.size(), Bone(0))));
    AddArray(payload, 8, 1, U16s({ 0 }));

    for (size_t b = 0; b < bones.size(); b++) {
        size_t track = at + b * 0x58 + BONE_TRANSLATION;
        PutU16(payload, track, interpolation);
        TrackEntries entries = Runs(payload, bones[b], 12);
        SetTrack(payload, track, entries);
    }
    return payload;
}

// An SKA1 payload: attachments by id, each with an animate-attached track of 1-byte keys; lookup [0].
static std::vector<uint8_t> Ska1(std::vector<std::pair<uint32_t, std::vector<float>>> attachments) {
    std::vector<uint8_t> payload(16, 0);
    size_t at = AddArray(payload, 0, static_cast<uint32_t>(attachments.size()), std::vector<uint8_t>(attachments.size() * ATTACHMENT_SIZE, 0));
    AddArray(payload, 8, 1, U16s({ 0 }));

    for (size_t a = 0; a < attachments.size(); a++) {
        PutU32(payload, at + a * ATTACHMENT_SIZE, attachments[a].first);
        TrackEntries entries = Runs(payload, attachments[a].second, 1);
        SetTrack(payload, at + a * ATTACHMENT_SIZE + ATTACHMENT_ANIMATED, entries);
    }
    return payload;
}

struct Skel {
    std::vector<uint8_t> sks1 = Sks1({});
    std::vector<uint8_t> skb1 = Skb1({});
    std::vector<uint8_t> ska1;
    uint32_t parent = 0;
    std::vector<uint8_t> afid;
};

static std::vector<uint8_t> SkelFile(const Skel& skel) {
    std::vector<uint8_t> file = Concat({ Chunk("SKL1", std::vector<uint8_t>(16, 0)), Chunk("SKS1", skel.sks1), Chunk("SKB1", skel.skb1) });
    if (!skel.ska1.empty()) {
        file = Concat({ file, Chunk("SKA1", skel.ska1) });
    }
    if (skel.parent) {
        std::vector<uint8_t> skpd(16, 0);
        PutU32(skpd, 8, skel.parent);
        file = Concat({ file, Chunk("SKPD", skpd) });
    }
    if (!skel.afid.empty()) {
        file = Concat({ file, Chunk("AFID", skel.afid) });
    }
    return file;
}

static std::vector<uint8_t> SkeletalMd20() {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::VERTICES, 1, Vertex(255, 0, 0, 0));
    return md20;
}

static std::vector<uint8_t> SkeletalModel(const std::vector<uint8_t>& md20, std::vector<std::vector<uint8_t>> chunks = {}) {
    chunks.push_back(Chunk("SKID", U32s({ 100 })));
    return Md21File(md20, chunks);
}

static float FloatAt(const std::vector<uint8_t>& blob, size_t at) {
    float value;
    std::memcpy(&value, blob.data() + at, 4);
    return value;
}

// The first key of sequence s in the track at track, 0 for an empty run; timestamps must match it.
static float KeyOf(const std::vector<uint8_t>& m2, size_t track, size_t s, size_t keySize = 12) {
    auto times = TrackEntry(m2, track, s, false);
    auto keys = TrackEntry(m2, track, s, true);
    if (!keys.first || times.first != keys.first || GetU32(m2, times.second) != 0) {
        return 0;
    }
    return keySize == 12 ? FloatAt(m2, keys.second) : m2[keys.second];
}

static size_t BoneTrack(const std::vector<uint8_t>& m2, size_t bone) {
    return RecordAt(m2, M2At::BONES, bone, 0x58) + BONE_TRANSLATION;
}

static size_t AttachmentTrack(const std::vector<uint8_t>& m2, size_t attachment) {
    return RecordAt(m2, M2At::ATTACHMENTS, attachment, ATTACHMENT_SIZE) + ATTACHMENT_ANIMATED;
}

static bool Contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

static bool Parsed(const Skel& skel, SkeletonFile& file) {
    std::string error;
    return Retail::ParseSkeleton(SkelFile(skel), file, error);
}

TEST(SkeletonFilesNameTheirParentAndAnimFiles) {
    Skel skel;
    skel.parent = 42;
    skel.afid = AfidRow(4, 1, 99);
    SkeletonFile file;
    std::string error;

    CHECK(Retail::ParseSkeleton(SkelFile(skel), file, error));
    CHECK(file.parent == 42 && file.afid == AfidRow(4, 1, 99) && file.attachments.empty());
    CHECK(file.sequences.size() >= 32 && file.bones.size() >= 16);

    CHECK(!Retail::ParseSkeleton(Chunk("SKS1", Sks1({})), file, error) && Contains(error, "SKB1"));
    CHECK(!Retail::ParseSkeleton(Chunk("SKB1", Skb1({})), file, error) && Contains(error, "SKS1"));
    CHECK(!Retail::ParseSkeleton(Concat({ SkelFile(Skel()), Chunk("SKB1", Skb1({})) }), file, error) && Contains(error, "twice"));
    CHECK(!Retail::ParseSkeleton(Concat({ Chunk("SKS1", std::vector<uint8_t>(8, 0)), Chunk("SKB1", Skb1({})) }), file, error) && Contains(error, "SKS1"));
    CHECK(!Retail::ParseSkeleton(Concat({ SkelFile(Skel()), Chunk("AFID", std::vector<uint8_t>(12, 0)) }), file, error) && Contains(error, "AFID"));
}

static Skel Parent(uint32_t parent) {
    Skel skel;
    skel.parent = parent;
    return skel;
}

TEST(SkeletonChainsAreReadNearestFirst) {
    std::vector<SkeletonFile> chain;
    std::string error;

    CHECK(Retail::LoadSkeletonChain(1, Files({ { 1, SkelFile(Parent(2)) }, { 2, SkelFile(Parent(3)) }, { 3, SkelFile(Parent(0)) } }), chain, error));
    CHECK(chain.size() == 3 && chain[0].parent == 2 && chain[1].parent == 3 && chain[2].parent == 0);

    CHECK(!Retail::LoadSkeletonChain(7, Files({}), chain, error) && Contains(error, "skeleton 7"));
    CHECK(!Retail::LoadSkeletonChain(1, Files({ { 1, SkelFile(Parent(99)) } }), chain, error) && Contains(error, "parent skeleton 99"));
    CHECK(!Retail::LoadSkeletonChain(1, Files({ { 1, SkelFile(Parent(2)) }, { 2, SkelFile(Parent(1)) } }), chain, error) && Contains(error, "leads back"));

    std::map<uint32_t, std::vector<uint8_t>> longChain;
    for (uint32_t id = 1; id <= 8; id++) {
        longChain[id] = SkelFile(Parent(id + 1));
    }
    CHECK(!Retail::LoadSkeletonChain(1, Files(longChain), chain, error) && Contains(error, "chain"));
}

static std::vector<uint16_t> Ids(const std::vector<uint8_t>& records) {
    std::vector<uint16_t> ids;
    for (size_t at = 0; at + 0x40 <= records.size(); at += 0x40) {
        ids.push_back(GetU16(records, at));
    }
    return ids;
}

TEST(ParentsSupplyTheSequencesTheChildDoesNotOwn) {
    // Child: stand (in its chunks), walk (no file of its own), run (its AFID names one). Parent: stand, walk, swim.
    Skel child = Parent(2);
    child.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0), Sequence(5, 0) }, { 0 });
    child.afid = AfidRow(5, 0, 900);
    Skel parent;
    parent.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0x20), Sequence(42, 0x20) });
    std::vector<SkeletonFile> chain(2);
    Retail::MergedSequences merged;
    std::string error;

    CHECK(Parsed(child, chain[0]) && Parsed(parent, chain[1]));
    CHECK(Retail::MergeSequences(chain, merged, error));
    CHECK(Ids(merged.records) == std::vector<uint16_t>({ 0, 4, 5, 42 }));
    CHECK(merged.sources.size() == 4 && merged.sources[0].level == 0 && merged.sources[1].level == 1 && merged.sources[1].index == 1);
    CHECK(merged.sources.size() == 4 && merged.sources[2].level == 0 && merged.sources[3].level == 1 && merged.sources[3].index == 2);
    CHECK(merged.parentIndex == std::vector<int32_t>({ -1, 1, -1, 2 }));
    CHECK(merged.own == 3 && merged.parentCount == 3);

    // The child's lookup lists its own; the appended swim joins it.
    CHECK(merged.lookup.size() == 43 && merged.lookup[0] == 0 && merged.lookup[42] == 3 && merged.lookup[4] == 0xFFFF);
}

TEST(AppendedSequencesLinkIntoTheMergedList) {
    Skel child = Parent(2);
    child.sks1 = Sks1({ Sequence(9, 0x20), Sequence(4, 0x20, 1) });
    std::vector<uint8_t> stand = Sequence(0, 0x20);
    PutU16(stand, SeqAt::VARIATION_NEXT, 1);
    std::vector<uint8_t> walk = Sequence(4, 0x20);
    PutU16(walk, SeqAt::VARIATION_NEXT, 3);
    Skel parent;
    parent.sks1 = Sks1({ stand, Sequence(0, 0x40, 1, 0, 0), walk, Sequence(4, 0x20, 1) });
    std::vector<SkeletonFile> chain(2);
    Retail::MergedSequences merged;
    std::string error;

    CHECK(Parsed(child, chain[0]) && Parsed(parent, chain[1]));
    CHECK(Retail::MergeSequences(chain, merged, error));
    CHECK(Ids(merged.records) == std::vector<uint16_t>({ 9, 4, 0, 0, 4 }));
    CHECK(GetU16(merged.records, 2 * 0x40 + SeqAt::VARIATION_NEXT) == 3);
    CHECK(GetU16(merged.records, 3 * 0x40 + 0x3E) == 2);
    CHECK(GetU16(merged.records, 4 * 0x40 + SeqAt::VARIATION_NEXT) == 1);

    // Nothing appended, no parent at all: the lookup stays the skeleton's, empty here.
    CHECK(Retail::MergeSequences({ chain[1] }, merged, error) && merged.lookup.empty() && merged.parentCount == 0);
}

TEST(SkeletonsReplaceTheModelsSequencesBonesAndAttachments) {
    Skel skel;
    skel.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0) }, { 0 });
    skel.skb1 = Skb1({ { 1.5f, -2.5f } });
    skel.ska1 = Ska1({ { 7, { 3, 0 } } });
    skel.afid = AfidRow(4, 0, 900);
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(SkeletalModel(SkeletalMd20()), Sources({ { 100, SkelFile(skel) }, { 900, Chunk("AFSB", AnimPart(2.5f)) } }), model, error));
    CHECK(GetU32(model.m2, M2At::SEQUENCES) == 2 && GetU32(model.m2, M2At::BONES) == 1 && GetU32(model.m2, M2At::ATTACHMENTS) == 1);
    CHECK(KeyOf(model.m2, BoneTrack(model.m2, 0), 0) == 1.5f && KeyOf(model.m2, BoneTrack(model.m2, 0), 1) == 2.5f);
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::ATTACHMENTS, 0, ATTACHMENT_SIZE)) == 7 && KeyOf(model.m2, AttachmentTrack(model.m2, 0), 0, 1) == 3);
    CHECK(U16sAt(model.m2, M2At::KEY_BONE_LOOKUP) == std::vector<uint16_t>({ 0 }) && U16sAt(model.m2, M2At::ATTACHMENT_LOOKUP) == std::vector<uint16_t>({ 0 }));
    CHECK(U16sAt(model.m2, M2At::SEQUENCE_LOOKUP) == std::vector<uint16_t>({ 0 }));
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::SEQUENCES, 1, 0x40) + SeqAt::FLAGS) & 0x20);
    CHECK(model.skeleton.fileDataId == 100 && model.skeleton.own == 2 && model.skeleton.parents.empty());
    CHECK(model.warnings.empty());
}

TEST(AnimFilesTheSkeletonCannotReadLeaveItsBonesInBindPose) {
    Skel skel;
    skel.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0) });
    skel.skb1 = Skb1({ { 1.5f, -2.5f } });
    skel.afid = AfidRow(4, 0, 900);
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(SkeletalModel(SkeletalMd20()), Sources({ { 100, SkelFile(skel) } }), model, error));
    CHECK(KeyOf(model.m2, BoneTrack(model.m2, 0), 1) == 0 && TrackEntry(model.m2, BoneTrack(model.m2, 0), 1, false).first == 0);
    CHECK(model.warnings.size() == 1 && Contains(model.warnings[0], "900"));

    // An .anim with bone keys but no AFSB falls back to AFM2; attachment keys need AFSA.
    CHECK(ConvertModel(SkeletalModel(SkeletalMd20()), Sources({ { 100, SkelFile(skel) }, { 900, Chunk("AFM2", AnimPart(2.5f)) } }), model, error));
    CHECK(KeyOf(model.m2, BoneTrack(model.m2, 0), 1) == 2.5f);
    skel.ska1 = Ska1({ { 7, { 0, -1 } } });
    CHECK(!ConvertModel(SkeletalModel(SkeletalMd20()), Sources({ { 100, SkelFile(skel) }, { 900, Chunk("AFM2", AnimPart(2.5f)) } }), model, error) && Contains(error, "AFSA"));
}

// Child: stand (own keys), walk (keys from the parent). Parent: stand, walk, run. Two child bones, one parent bone.
static std::map<uint32_t, std::vector<uint8_t>> ParentChain(uint16_t parentInterpolation = 1) {
    Skel child = Parent(200);
    child.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0) }, { 0 });
    child.skb1 = Skb1({ { 1, 0 }, { 2, 0 } });
    child.ska1 = Ska1({ { 11, { 3, 0 } } });
    Skel parent;
    parent.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0x20), Sequence(5, 0x20) });
    parent.skb1 = Skb1({ { 10, 20, 30 } }, parentInterpolation);
    parent.ska1 = Ska1({ { 15, { 9, 9, 9 } }, { 11, { 4, 5, 6 } } });
    return { { 100, SkelFile(child) }, { 200, SkelFile(parent) } };
}

TEST(ParentSkeletonsKeyTheSequencesTheyFill) {
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(SkeletalModel(SkeletalMd20()), Sources(ParentChain()), model, error));
    CHECK(GetU32(model.m2, M2At::SEQUENCES) == 3 && GetU32(model.m2, M2At::BONES) == 2);
    size_t first = BoneTrack(model.m2, 0);
    size_t second = BoneTrack(model.m2, 1);
    CHECK(KeyOf(model.m2, first, 0) == 1 && KeyOf(model.m2, first, 1) == 20 && KeyOf(model.m2, first, 2) == 30);
    CHECK(KeyOf(model.m2, second, 0) == 2 && KeyOf(model.m2, second, 1) == 0 && KeyOf(model.m2, second, 2) == 0);
    CHECK(GetU32(model.m2, second + 4) == 3);

    // Attachments match by id, not position.
    size_t attachment = AttachmentTrack(model.m2, 0);
    CHECK(KeyOf(model.m2, attachment, 0, 1) == 3 && KeyOf(model.m2, attachment, 1, 1) == 5 && KeyOf(model.m2, attachment, 2, 1) == 6);

    CHECK(U16sAt(model.m2, M2At::SEQUENCE_LOOKUP) == std::vector<uint16_t>({ 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 2 }));
    CHECK(model.skeleton.parents == std::vector<uint32_t>({ 200 }) && model.skeleton.parentIndex == std::vector<int32_t>({ -1, 1, 2 }));
    CHECK(model.warnings.empty());
}

TEST(ParentKeysOfAnotherInterpolationAreRefused) {
    ConvertedModel model;
    std::string error;

    CHECK(!ConvertModel(SkeletalModel(SkeletalMd20()), Sources(ParentChain(2)), model, error) && Contains(error, "interpolat") && Contains(error, "bone 0"));

    // A child track without runs takes the parent's interpolation.
    std::map<uint32_t, std::vector<uint8_t>> files = ParentChain(2);
    Skel child = Parent(200);
    child.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0) });
    child.skb1 = Skb1({ {} }, 0);
    files[100] = SkelFile(child);
    CHECK(ConvertModel(SkeletalModel(SkeletalMd20()), Sources(files), model, error));
    CHECK(GetU16(model.m2, BoneTrack(model.m2, 0)) == 2 && KeyOf(model.m2, BoneTrack(model.m2, 0), 2) == 30);
}

TEST(ModelsOnASkeletonAreRefusedWhenTheyCannotBeMerged) {
    ConvertedModel model;
    std::string error;

    CHECK(!ConvertModel(SkeletalModel(SkeletalMd20()), Sources({}), model, error) && Contains(error, "skeleton 100"));

    std::vector<uint8_t> ownBones = SkeletalMd20();
    AddArray(ownBones, M2At::BONES, 1, Bone(0));
    CHECK(!ConvertModel(SkeletalModel(ownBones), Sources(ParentChain()), model, error) && Contains(error, "SKID") && Contains(error, "bones"));

    for (uint32_t type : { 1u, 6u, 8u, 19u }) {
        std::vector<uint8_t> character = SkeletalMd20();
        AddArray(character, M2At::TEXTURES, 2, Concat({ Texture(0), Texture(type) }));
        CHECK(!ConvertModel(SkeletalModel(character), Sources(ParentChain()), model, error) && Contains(error, ("type " + std::to_string(type)).c_str()));
    }

    std::vector<uint8_t> creature = SkeletalMd20();
    AddArray(creature, M2At::TEXTURES, 2, Concat({ Texture(11), Texture(2) }));
    CHECK(ConvertModel(SkeletalModel(creature), Sources(ParentChain()), model, error));
}

// Two color records: the first with a run per child sequence, the second with one run every sequence shares.
static std::vector<uint8_t> ColoredMd20(uint32_t firstRuns) {
    std::vector<uint8_t> md20 = SkeletalMd20();
    size_t colors = AddArray(md20, M2At::COLORS, 2, std::vector<uint8_t>(2 * COLOR_SIZE, 0));
    TrackEntries first = Runs(md20, std::vector<float>(firstRuns, 7.0f), 12);
    SetTrack(md20, colors, first);
    TrackEntries shared = Runs(md20, { 8.0f }, 12);
    SetTrack(md20, colors + COLOR_SIZE, shared);
    return md20;
}

TEST(TheModelsOwnTracksGetARunPerMergedSequence) {
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(SkeletalModel(ColoredMd20(2)), Sources(ParentChain()), model, error));
    size_t first = RecordAt(model.m2, M2At::COLORS, 0, COLOR_SIZE);
    size_t shared = RecordAt(model.m2, M2At::COLORS, 1, COLOR_SIZE);
    CHECK(GetU32(model.m2, first + 4) == 3 && GetU32(model.m2, first + 0x0C) == 3);
    CHECK(KeyOf(model.m2, first, 0) == 7 && KeyOf(model.m2, first, 1) == 7 && TrackEntry(model.m2, first, 2, false).first == 0);
    CHECK(GetU32(model.m2, shared + 4) == 1 && KeyOf(model.m2, shared, 0) == 8);

    CHECK(!ConvertModel(SkeletalModel(ColoredMd20(5)), Sources(ParentChain()), model, error) && Contains(error, "5 runs"));
}

TEST(ModelTrackKeysInSkeletonAnimFilesComeFromAfm2) {
    Skel skel;
    skel.sks1 = Sks1({ Sequence(0, 0x20), Sequence(4, 0) });
    skel.skb1 = Skb1({ { 1, -1 } });
    skel.afid = AfidRow(4, 0, 900);
    std::vector<uint8_t> md20 = SkeletalMd20();
    size_t colors = AddArray(md20, M2At::COLORS, 1, std::vector<uint8_t>(COLOR_SIZE, 0));
    TrackEntries entries = Runs(md20, { 7.0f, -1.0f }, 12);
    SetTrack(md20, colors, entries);
    ConvertedModel model;
    std::string error;

    std::vector<uint8_t> anim = Concat({ Chunk("AFM2", AnimPart(6.0f)), Chunk("AFSB", AnimPart(2.5f)) });
    CHECK(ConvertModel(SkeletalModel(md20), Sources({ { 100, SkelFile(skel) }, { 900, anim } }), model, error));
    CHECK(KeyOf(model.m2, BoneTrack(model.m2, 0), 1) == 2.5f && KeyOf(model.m2, RecordAt(model.m2, M2At::COLORS, 0, COLOR_SIZE), 1) == 6.0f);

    CHECK(!ConvertModel(SkeletalModel(md20), Sources({ { 100, SkelFile(skel) }, { 900, Chunk("AFSB", AnimPart(2.5f)) } }), model, error) && Contains(error, "AFM2"));
}

// PEDC: one event track per model event, with a run of timestamps per parent sequence.
static std::vector<uint8_t> Pedc(std::vector<std::vector<std::vector<uint32_t>>> tracks) {
    std::vector<uint8_t> payload(8, 0);
    size_t records = AddArray(payload, 0, static_cast<uint32_t>(tracks.size()), std::vector<uint8_t>(tracks.size() * 12, 0));
    for (size_t t = 0; t < tracks.size(); t++) {
        TrackEntries entries;
        for (const auto& run : tracks[t]) {
            entries.times.push_back({ static_cast<uint32_t>(run.size()), run.empty() ? 0 : static_cast<uint32_t>(Append(payload, U32s(run))) });
        }
        SetTrack(payload, records + t * 12, entries, true);
    }
    return payload;
}

static std::vector<uint8_t> EventMd20() {
    std::vector<uint8_t> md20 = SkeletalMd20();
    size_t event = AddArray(md20, M2At::EVENTS, 1, std::vector<uint8_t>(EVENT_SIZE, 0));
    uint32_t own = static_cast<uint32_t>(Append(md20, U32s({ 50 })));
    SetTrack(md20, event + EVENT_ENABLED, { { { 1, own }, { 0, 0 } }, {} }, true);
    return md20;
}

static size_t EventTrack(const std::vector<uint8_t>& m2) {
    return RecordAt(m2, M2At::EVENTS, 0, EVENT_SIZE) + EVENT_ENABLED;
}

TEST(ParentEventTimingsFillTheParentsSequences) {
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(SkeletalModel(EventMd20(), { Chunk("PEDC", Pedc({ { { 900 }, { 300, 600 }, { 700 } } })) }), Sources(ParentChain()), model, error));
    size_t track = EventTrack(model.m2);
    auto own = TrackEntry(model.m2, track, 0, false);
    auto walk = TrackEntry(model.m2, track, 1, false);
    auto run = TrackEntry(model.m2, track, 2, false);
    CHECK(GetU32(model.m2, track + 4) == 3);
    CHECK(own.first == 1 && GetU32(model.m2, own.second) == 50);
    CHECK(walk.first == 2 && GetU32(model.m2, walk.second) == 300 && GetU32(model.m2, walk.second + 4) == 600);
    CHECK(run.first == 1 && GetU32(model.m2, run.second) == 700);
    CHECK(model.warnings.empty());

    CHECK(ConvertModel(SkeletalModel(EventMd20(), { Chunk("PEDC", Pedc({ { { 1 }, { 2 }, { 3 } }, { {}, {}, {} } })) }), Sources(ParentChain()), model, error));
    CHECK(TrackEntry(model.m2, EventTrack(model.m2), 1, false).first == 0);
    CHECK(model.warnings.size() == 1 && Contains(model.warnings[0], "PEDC"));
}
