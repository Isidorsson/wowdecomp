#include <cmath>
#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ModelSources;

constexpr size_t PARTICLE_SIZE = 476;
constexpr size_t SPEED_TRACK = 0x34;
constexpr size_t GRAVITY_TRACK = 0x84;
constexpr uint32_t MULTI_TEXTURE = 0x10000000;
constexpr uint32_t REFRACTION = 0x100000;
constexpr uint32_t COMPRESSED_GRAVITY = 0x800000;
// One compressed gravity key: x and y as int8 / 128, then the signed magnitude in units of 0.04238648.
constexpr float GRAVITY_UNIT = 0.04238648f;

static bool Convert(const std::vector<uint8_t>& file, ConvertedModel& model, const ModelSources& sources = {}) {
    std::string error;
    bool converted = ConvertModel(file, sources, model, error);
    if (!converted) {
        std::printf("    (%s)\n", error.c_str());
    }
    return converted;
}

static size_t Particle(const std::vector<uint8_t>& m2, size_t index) {
    return RecordAt(m2, M2At::PARTICLES, index, PARTICLE_SIZE);
}

static float FloatAt(const std::vector<uint8_t>& blob, size_t at) {
    float value;
    std::memcpy(&value, blob.data() + at, 4);
    return value;
}

static bool Near(float value, float expected) {
    return std::fabs(value - expected) < 1e-4f;
}

static std::vector<uint8_t> GravityKey(int8_t x, int8_t y, int16_t magnitude) {
    std::vector<uint8_t> key(4, 0);
    key[0] = static_cast<uint8_t>(x);
    key[1] = static_cast<uint8_t>(y);
    PutU16(key, 2, static_cast<uint16_t>(magnitude));
    return key;
}

static size_t AppendBytes(std::vector<uint8_t>& blob, const std::vector<uint8_t>& bytes) {
    blob.resize((blob.size() + 15) & ~size_t(15), 0);
    size_t at = blob.size();
    blob.insert(blob.end(), bytes.begin(), bytes.end());
    return at;
}

// A one-sequence model whose emitters are given; returns where the first one starts in the blob.
static size_t WithParticles(std::vector<uint8_t>& md20, const std::vector<std::vector<uint8_t>>& emitters) {
    return AddArray(md20, M2At::PARTICLES, static_cast<uint32_t>(emitters.size()), Concat(emitters));
}

TEST(ParticlesBecome476ByteRecords) {
    std::vector<uint8_t> md20 = MinimalMd20();
    size_t first = WithParticles(md20, { RetailParticle(7, 0x10, 0, 2), RetailParticle(8, 0x20, 0, 2) });
    PutU32(md20, first + SPEED_TRACK + 4, 3);
    PutU32(md20, first + SPEED_TRACK + 8, 0x1234);
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(GetU32(model.m2, M2At::PARTICLES) == 2);
    size_t a = Particle(model.m2, 0);
    size_t b = Particle(model.m2, 1);
    CHECK(GetU32(model.m2, a) == 7 && GetU32(model.m2, a + 4) == 0x10);
    CHECK(GetU32(model.m2, b) == 8 && GetU32(model.m2, b + 4) == 0x20);
    CHECK(GetU32(model.m2, a + SPEED_TRACK + 4) == 3 && GetU32(model.m2, a + SPEED_TRACK + 8) == 0x1234);
    CHECK(model.m2[a + 0x2C] == 0 && model.m2[a + 0x2D] == 0);
    CHECK(model.warnings.empty());
}

TEST(ParticleFlagsAndBlendFitTheClient) {
    std::vector<uint8_t> md20 = MinimalMd20();
    WithParticles(md20, { RetailParticle(1, 0x400FFFFF, 0, 7), RetailParticle(2, 0, 0, 2) });
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(GetU32(model.m2, Particle(model.m2, 0) + 4) == 0xFFFFF);
    CHECK(model.m2[Particle(model.m2, 0) + 0x28] == 4);
    CHECK(model.m2[Particle(model.m2, 1) + 0x28] == 2);
}

TEST(MultiTextureEmittersKeepTheirFirstTexture) {
    std::vector<uint8_t> md20 = MinimalMd20();
    // Three 5-bit texture indices: 3, 5, 6.
    WithParticles(md20, { RetailParticle(1, MULTI_TEXTURE | 0x10, uint16_t(3 | (5 << 5) | (6 << 10))) });
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(GetU16(model.m2, Particle(model.m2, 0) + 0x16) == 3);
    CHECK(GetU32(model.m2, Particle(model.m2, 0) + 4) == 0x10);
    CHECK(model.warnings.size() == 1 && model.warnings[0].find("multi-texture") != std::string::npos);
}

TEST(RefractionAndParticleModelChunksAreWarned) {
    std::vector<uint8_t> md20 = MinimalMd20();
    WithParticles(md20, { RetailParticle(1, REFRACTION) });
    ConvertedModel model;

    CHECK(Convert(Md21File(md20, { Chunk("RPID", U32s({ 123 })) }), model));
    CHECK(GetU32(model.m2, Particle(model.m2, 0) + 4) == 0);
    CHECK(model.warnings.size() == 2);
    CHECK(model.warnings[0].find("refraction") != std::string::npos && model.warnings[1].find("RPID") != std::string::npos);
}

// One emitter with the given gravity track: one embedded sequence whose keys are keys.
static std::vector<uint8_t> GravityModel(uint32_t flags, const std::vector<uint8_t>& keys, uint16_t interpolation, size_t& keysAt) {
    std::vector<uint8_t> md20 = MinimalMd20();
    size_t emitter = WithParticles(md20, { RetailParticle(1, flags) });
    size_t times = AppendBytes(md20, U32s({ 0, 100, 200 }));
    keysAt = AppendBytes(md20, keys);
    uint32_t count = interpolation >= 2 ? uint32_t(keys.size() / 12) : uint32_t(keys.size() / 4);
    PutU16(md20, emitter + GRAVITY_TRACK, interpolation);
    SetTrack(md20, emitter + GRAVITY_TRACK, { { { count, uint32_t(times) } }, { { count, uint32_t(keysAt) } } });
    return md20;
}

TEST(CompressedGravityBecomesTheDownwardPull) {
    size_t keys;
    std::vector<uint8_t> md20 = GravityModel(COMPRESSED_GRAVITY, Concat({ GravityKey(0, 0, -100), GravityKey(0, 0, 100), GravityKey(64, 0, 100) }), 1, keys);
    ConvertedModel model;

    // -100 is a pull along -z of 4.238648: 3.3.5 gravity counts down as positive.
    CHECK(Convert(Md21File(md20), model));
    CHECK(Near(FloatAt(model.m2, keys), 100 * GRAVITY_UNIT));
    CHECK(Near(FloatAt(model.m2, keys + 4), -100 * GRAVITY_UNIT));
    CHECK(Near(FloatAt(model.m2, keys + 8), -std::sqrt(0.75f) * 100 * GRAVITY_UNIT));
    CHECK((GetU32(model.m2, Particle(model.m2, 0) + 4) & COMPRESSED_GRAVITY) == 0);
}

TEST(HermiteGravityTangentsAreDecodedToo) {
    size_t keys;
    std::vector<uint8_t> md20 = GravityModel(COMPRESSED_GRAVITY, Concat({ GravityKey(0, 0, -100), GravityKey(0, 0, -200), GravityKey(0, 0, 100) }), 2, keys);
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(Near(FloatAt(model.m2, keys), 100 * GRAVITY_UNIT));
    CHECK(Near(FloatAt(model.m2, keys + 4), 200 * GRAVITY_UNIT));
    CHECK(Near(FloatAt(model.m2, keys + 8), -100 * GRAVITY_UNIT));
}

TEST(PlainGravityIsLeftAlone) {
    float pull = 9.5f;
    std::vector<uint8_t> key(4);
    std::memcpy(key.data(), &pull, 4);
    size_t keys;
    std::vector<uint8_t> md20 = GravityModel(0, key, 1, keys);
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(FloatAt(model.m2, keys) == 9.5f);
}

TEST(SharedGravityKeysAreDecodedOnce) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 2, Concat({ Sequence(0, 0x20), Sequence(4, 0x20) }));
    AddArray(md20, M2At::BONES, 1, Bone(0));
    AddArray(md20, M2At::VERTICES, 1, Vertex(255, 0, 0, 0));
    size_t emitters = WithParticles(md20, { RetailParticle(1, COMPRESSED_GRAVITY), RetailParticle(2, COMPRESSED_GRAVITY) });
    size_t times = AppendBytes(md20, U32s({ 0 }));
    size_t keys = AppendBytes(md20, GravityKey(0, 0, -100));
    TrackEntries shared { { { 1, uint32_t(times) }, { 1, uint32_t(times) } }, { { 1, uint32_t(keys) }, { 1, uint32_t(keys) } } };
    SetTrack(md20, emitters + GRAVITY_TRACK, shared);
    SetTrack(md20, emitters + 492 + GRAVITY_TRACK, shared);
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(Near(FloatAt(model.m2, keys), 100 * GRAVITY_UNIT));
}

TEST(ParticleTracksAreInlined) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 2, Concat({ Sequence(0, 0x20), Sequence(4, 0) }));
    AddArray(md20, M2At::BONES, 1, Bone(0));
    AddArray(md20, M2At::VERTICES, 1, Vertex(255, 0, 0, 0));
    size_t emitter = WithParticles(md20, { RetailParticle(1, 0) });
    SetTrack(md20, emitter + SPEED_TRACK, { { { 0, 0 }, { 2, 0 } }, { { 0, 0 }, { 2, 8 } } });
    std::vector<uint8_t> anim = U32s({ 0, 500 });
    anim.resize(16, 0x5A);
    ModelSources sources;
    sources.read = [anim](uint32_t, std::vector<uint8_t>& bytes, std::string&) {
        bytes = anim;
        return true;
    };
    ConvertedModel model;

    CHECK(Convert(Md21File(md20, { Chunk("AFID", AfidRow(4, 0, 777)) }), model, sources));
    size_t track = Particle(model.m2, 0) + SPEED_TRACK;
    auto times = TrackEntry(model.m2, track, 1, false);
    auto values = TrackEntry(model.m2, track, 1, true);
    CHECK(times.first == 2 && GetU32(model.m2, times.second + 4) == 500);
    CHECK(values.first == 2 && values.second == times.second + 8 && model.m2[values.second] == 0x5A);
}
