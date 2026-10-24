#include <cmath>
#include <map>
#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"
#include "Tools/Tests/Wdc5Builder.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ModelSources;

static size_t Append(std::vector<uint8_t>& blob, const std::vector<uint8_t>& bytes) {
    blob.resize((blob.size() + 15) & ~size_t(15), 0);
    size_t at = blob.size();
    blob.insert(blob.end(), bytes.begin(), bytes.end());
    return at;
}

static ModelSources Reading(std::map<uint32_t, std::vector<uint8_t>> files) {
    ModelSources sources;
    sources.read = [files](uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
        auto file = files.find(id);
        if (file == files.end()) {
            error = "no such file";
            return false;
        }
        bytes = file->second;
        return true;
    };
    return sources;
}

// Timestamps 0 and 500, then two 12-byte translation keys: an .anim body for one sequence.
static std::vector<uint8_t> AnimBody() {
    std::vector<uint8_t> body = U32s({ 0, 500 });
    body.resize(8 + 24, 0x5A);
    return body;
}

// Stand (embedded, one key) and Walk (id 4, external: two keys at offsets 0 and 8 of its .anim) on one bone.
struct AnimatedModel {
    std::vector<uint8_t> md20;
    size_t track;
    size_t embeddedTimes;
};

static AnimatedModel WithExternalWalk(uint32_t walkFlags = 0) {
    AnimatedModel model { NewMd20(), 0, 0 };
    AddArray(model.md20, M2At::SEQUENCES, 2, Concat({ Sequence(0, 0x20), Sequence(4, walkFlags) }));
    size_t bone = AddArray(model.md20, M2At::BONES, 1, Bone(0));
    model.embeddedTimes = Append(model.md20, U32s({ 0 }));
    size_t embeddedKeys = Append(model.md20, std::vector<uint8_t>(12, 1));
    model.track = bone + 0x10;
    SetTrack(model.md20, model.track, { { { 1, uint32_t(model.embeddedTimes) }, { 2, 0 } }, { { 1, uint32_t(embeddedKeys) }, { 2, 8 } } });
    return model;
}

static uint32_t SequenceFlags(const std::vector<uint8_t>& m2, size_t s) {
    return GetU32(m2, RecordAt(m2, M2At::SEQUENCES, s, 0x40) + SeqAt::FLAGS);
}

TEST(ExternalAnimationsAreInlined) {
    AnimatedModel walk = WithExternalWalk();
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(walk.md20, { Chunk("AFID", AfidRow(4, 0, 777)) }), Reading({ { 777, AnimBody() } }), model, error));
    CHECK(SequenceFlags(model.m2, 1) & 0x20);
    auto times = TrackEntry(model.m2, walk.track, 1, false);
    auto keys = TrackEntry(model.m2, walk.track, 1, true);
    CHECK(times.first == 2 && keys.first == 2 && keys.second == times.second + 8);
    CHECK(GetU32(model.m2, times.second + 4) == 500 && model.m2[keys.second] == 0x5A);
    CHECK(TrackEntry(model.m2, walk.track, 0, false) == std::make_pair(1u, uint32_t(walk.embeddedTimes)));
    CHECK(model.warnings.empty());
}

TEST(ChunkedAnimFilesCountFromTheAfm2Payload) {
    AnimatedModel walk = WithExternalWalk();
    ConvertedModel model;
    std::string error;
    std::vector<uint8_t> chunked = Concat({ Chunk("AFM2", AnimBody()), Chunk("AFSB", { 9, 9, 9, 9 }) });

    CHECK(ConvertModel(Md21File(walk.md20, { Chunk("AFID", AfidRow(4, 0, 777)) }), Reading({ { 777, chunked } }), model, error));
    auto times = TrackEntry(model.m2, walk.track, 1, false);
    CHECK(GetU32(model.m2, times.second + 4) == 500);
}

TEST(SequencesWhoseAnimFileIsMissingAreEmptied) {
    AnimatedModel walk = WithExternalWalk();
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(walk.md20, { Chunk("AFID", AfidRow(4, 0, 777)) }), Reading({}), model, error));
    CHECK(SequenceFlags(model.m2, 1) & 0x20);
    CHECK(TrackEntry(model.m2, walk.track, 1, false).first == 0 && TrackEntry(model.m2, walk.track, 1, true).first == 0);
    CHECK(model.warnings.size() == 1);
}

TEST(SequencesWithoutAnAnimFileStayEmbedded) {
    AnimatedModel walk = WithExternalWalk();
    ConvertedModel model;
    std::string error;

    // FileDataID 0, and no AFID row at all, both mean the data is in the model (the retail sword).
    CHECK(ConvertModel(Md21File(walk.md20, { Chunk("AFID", AfidRow(4, 0, 0)) }), Reading({}), model, error));
    CHECK(SequenceFlags(model.m2, 1) & 0x20);
    CHECK(TrackEntry(model.m2, walk.track, 1, false) == std::make_pair(2u, 0u));
    CHECK(model.warnings.empty());
}

TEST(LocalSequencesNeedNoFile) {
    AnimatedModel walk = WithExternalWalk(0x10);
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(walk.md20, { Chunk("AFID", AfidRow(4, 0, 777)) }), Reading({}), model, error));
    CHECK(model.warnings.empty());
}

TEST(KeylessTracksAreRebasedToo) {
    AnimatedModel walk = WithExternalWalk();
    size_t event = AddArray(walk.md20, M2At::EVENTS, 1, std::vector<uint8_t>(0x24, 0));
    SetTrack(walk.md20, event + 0x18, { { { 0, 0 }, { 1, 4 } }, {} }, true);
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(walk.md20, { Chunk("AFID", AfidRow(4, 0, 777)) }), Reading({ { 777, AnimBody() } }), model, error));
    auto times = TrackEntry(model.m2, event + 0x18, 1, false);
    CHECK(times.first == 1 && GetU32(model.m2, times.second) == 500);
}

TEST(EntriesPastTheAnimFileAreRefused) {
    AnimatedModel walk = WithExternalWalk();
    SetTrack(walk.md20, walk.track, { { { 1, uint32_t(walk.embeddedTimes) }, { 2, 100 } }, { { 1, 0 }, { 2, 8 } } });
    ConvertedModel model;
    std::string error;

    CHECK(!ConvertModel(Md21File(walk.md20, { Chunk("AFID", AfidRow(4, 0, 777)) }), Reading({ { 777, AnimBody() } }), model, error));
    CHECK(error.find(".anim") != std::string::npos);
}

TEST(AliasesOfEmbeddedSequencesAreEmbedded) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 2, Concat({ Sequence(0, 0x20), Sequence(5, 0x40, 0, 0, 0) }));
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(md20), {}, model, error));
    CHECK(SequenceFlags(model.m2, 1) == (0x40 | 0x20));

    std::vector<uint8_t> loop = NewMd20();
    AddArray(loop, M2At::SEQUENCES, 2, Concat({ Sequence(0, 0x40, 0, 0, 1), Sequence(5, 0x40, 0, 0, 0) }));
    CHECK(!ConvertModel(Md21File(loop), {}, model, error) && error.find("alias") != std::string::npos);
}

static float FloatAt(const std::vector<uint8_t>& blob, size_t at) {
    float value;
    std::memcpy(&value, blob.data() + at, 4);
    return value;
}

TEST(CamerasBecome100ByteRecords) {
    std::vector<uint8_t> md20 = MinimalMd20();
    AddArray(md20, M2At::CAMERAS, 2, Concat({ RetailCamera(0, 30.0f, 0.5f), RetailCamera(1, 40.0f, 0.25f) }));
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(md20), {}, model, error));
    CHECK(GetU32(model.m2, M2At::CAMERAS) == 2);
    size_t portrait = RecordAt(model.m2, M2At::CAMERAS, 0, 100);
    size_t other = RecordAt(model.m2, M2At::CAMERAS, 1, 100);
    CHECK(GetU32(model.m2, portrait) == 0 && std::fabs(FloatAt(model.m2, portrait + 4) - 0.7f) < 1e-6f);
    CHECK(FloatAt(model.m2, portrait + 8) == 30.0f && FloatAt(model.m2, portrait + 0x0C) == 0.5f);
    CHECK(GetU32(model.m2, portrait + 0x10 + 4) == 3 && GetU32(model.m2, portrait + 0x10 + 8) == 0x1234);
    CHECK(GetU32(model.m2, other) == 1 && std::fabs(FloatAt(model.m2, other + 4) - 0.97f) < 1e-6f);
}

// AnimationData with the given id -> Fallback rows (WDC5, the known layout).
static Retail::AnimationFallbacks Fallbacks(std::vector<std::pair<uint32_t, uint16_t>> rows) {
    Wdc5Spec spec;
    spec.layoutHash = Retail::ANIMATION_DATA_LAYOUT;
    spec.recordSize = 2;
    spec.columns = { { 16, 0, 16, 0, 0, { 0, 0, 0 } } };
    Wdc5Section section;
    for (const auto& row : rows) {
        section.ids.push_back(row.first);
        section.records.push_back(static_cast<uint8_t>(row.second));
        section.records.push_back(static_cast<uint8_t>(row.second >> 8));
    }
    spec.sections = { section };

    Retail::Db2Table table;
    Retail::AnimationFallbacks fallbacks;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error) && fallbacks.Load(table, error));
    return fallbacks;
}

static uint16_t SequenceField(const std::vector<uint8_t>& m2, size_t s, size_t field) {
    return GetU16(m2, RecordAt(m2, M2At::SEQUENCES, s, 0x40) + field);
}

TEST(NewAnimationIdsFollowTheirFallbacks) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 4, Concat({ Sequence(0, 0x20, 0, 100), Sequence(1500, 0x20, 0, 50), Sequence(1600, 0x20, 0, 30), Sequence(1600, 0x20, 1, 20) }));
    Retail::AnimationFallbacks fallbacks = Fallbacks({ { 1500, 0 }, { 1600, 69 } });
    ModelSources sources;
    sources.fallbacks = &fallbacks;
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(md20), sources, model, error));

    // Stand keeps its frequency; the remapped stand joins as a variation that never plays at random.
    CHECK(SequenceField(model.m2, 0, SeqAt::VARIATION_NEXT) == 1);
    CHECK(SequenceField(model.m2, 1, SeqAt::ID) == 0 && SequenceField(model.m2, 1, SeqAt::VARIATION) == 1);
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::SEQUENCES, 1, 0x40) + SeqAt::FREQUENCY) == 0);

    // Id 69 had no sequence: the remapped ones become it, frequencies kept.
    CHECK(SequenceField(model.m2, 2, SeqAt::ID) == 69 && SequenceField(model.m2, 2, SeqAt::VARIATION) == 0);
    CHECK(SequenceField(model.m2, 2, SeqAt::VARIATION_NEXT) == 3);
    CHECK(SequenceField(model.m2, 3, SeqAt::ID) == 69 && SequenceField(model.m2, 3, SeqAt::VARIATION) == 1);
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::SEQUENCES, 3, 0x40) + SeqAt::FREQUENCY) == 20);

    std::vector<uint16_t> lookup = U16sAt(model.m2, M2At::SEQUENCE_LOOKUP);
    CHECK(lookup.size() == 70 && lookup[0] == 0 && lookup[69] == 2 && lookup[1] == 0xFFFF);

    CHECK(model.remapped.size() == 3);
    CHECK(model.remapped.size() == 3 && model.remapped[2].index == 3 && model.remapped[2].originalId == 1600 && model.remapped[2].originalVariation == 1 && model.remapped[2].id == 69);
}

TEST(NewAnimationIdsWithoutFallbacksAreRefused) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(1500, 0x20));
    ConvertedModel model;
    std::string error;

    CHECK(!ConvertModel(Md21File(md20), {}, model, error) && error.find("1500") != std::string::npos);
}

TEST(ModelsWithoutNewIdsKeepTheirLookupTable) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(4, 0x20));
    AddArray(md20, M2At::SEQUENCE_LOOKUP, 3, Concat({ { 0xFF, 0xFF }, { 0, 0 }, { 0xFF, 0xFF } }));
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(Md21File(md20), {}, model, error));
    CHECK(U16sAt(model.m2, M2At::SEQUENCE_LOOKUP) == std::vector<uint16_t>({ 0xFFFF, 0, 0xFFFF }));
    CHECK(model.remapped.empty());
}
