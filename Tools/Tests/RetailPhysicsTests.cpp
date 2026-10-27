#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RetailPhysics.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"
#include <cmath>

using Retail::BakePhysicsPose;
using Retail::ConvertedModel;
using Retail::ConvertModel;

// Retail drives a helm's plumes and flaps with physics (bones flagged 0x400, bodies and joints in a PFDC chunk or a
// PFID .phys file); 3.3.5 has none, so they stood in their bind pose. The conversion bakes the pose gravity settles
// them in: each joint swings its body toward straight down, as far as its cone allows.

constexpr size_t BONE_SIZE = 0x58;
constexpr size_t BONE_FLAGS = 0x04;
constexpr size_t BONE_PARENT = 0x08;
constexpr size_t BONE_ROTATION = 0x24;
constexpr size_t BONE_PIVOT = 0x4C;
constexpr uint32_t BONE_KINEMATIC = 0x400;
constexpr uint32_t BONE_TRANSFORMED = 0x200;
constexpr uint16_t BODY_STATIC = 0;
constexpr uint16_t BODY_DYNAMIC = 1;
constexpr uint16_t JOINT_SHOULDER = 1;
constexpr uint16_t JOINT_WELD = 2;

struct Vec3 {
    float x, y, z;
};

static void PutFloat(std::vector<uint8_t>& bytes, size_t at, float value) {
    std::memcpy(bytes.data() + at, &value, 4);
}

// A physics chunk as the file stores it: the tag's bytes reversed.
static std::vector<uint8_t> PhysChunk(const char* tag, const std::vector<uint8_t>& data) {
    char reversed[5] = { tag[3], tag[2], tag[1], tag[0], 0 };
    return Chunk(reversed, data);
}

static std::vector<uint8_t> Body(uint16_t type, uint16_t bone) {
    std::vector<uint8_t> record(48, 0);
    PutU16(record, 0, type);
    PutU16(record, 2, bone);
    return record;
}

static std::vector<uint8_t> Join(uint32_t bodyA, uint32_t bodyB, uint16_t type, uint16_t index) {
    std::vector<uint8_t> record(16, 0);
    PutU32(record, 0, bodyA);
    PutU32(record, 4, bodyB);
    PutU16(record, 12, type);
    PutU16(record, 14, index);
    return record;
}

// A shoulder joint whose frame (on body A) twists about axis, with a swing cone of coneDegrees. Retail stores the
// frame's rows scaled, so the axis is given unnormalized.
static std::vector<uint8_t> Shoulder(Vec3 axis, float coneDegrees) {
    std::vector<uint8_t> record(124, 0);
    PutFloat(record, 6 * 4, axis.x);
    PutFloat(record, 7 * 4, axis.y);
    PutFloat(record, 8 * 4, axis.z);
    PutFloat(record, 26 * 4, coneDegrees);
    return record;
}

struct Joint {
    uint32_t bodyA, bodyB;
    uint16_t type;
    Vec3 axis;
    float cone;
};

// The physics data: bodies (type, bone) and joints, shoulders numbered in order.
static std::vector<uint8_t> Physics(const std::vector<std::pair<uint16_t, uint16_t>>& bodies, const std::vector<Joint>& joints) {
    std::vector<uint8_t> bodyRecords, joinRecords, shoulders;
    for (const auto& body : bodies) {
        bodyRecords = Concat({ bodyRecords, Body(body.first, body.second) });
    }
    uint16_t shoulderCount = 0;
    for (const Joint& joint : joints) {
        bool shoulder = joint.type == JOINT_SHOULDER;
        joinRecords = Concat({ joinRecords, Join(joint.bodyA, joint.bodyB, joint.type, shoulder ? shoulderCount : 0) });
        if (shoulder) {
            shoulders = Concat({ shoulders, Shoulder(joint.axis, joint.cone) });
            shoulderCount++;
        }
    }
    std::vector<uint8_t> version(2, 0);
    PutU16(version, 0, 6);
    return Concat({ PhysChunk("PHYS", version), PhysChunk("BDY4", bodyRecords), PhysChunk("SHJ2", shoulders), PhysChunk("JOIN", joinRecords) });
}

static std::vector<uint8_t> PivotBone(uint32_t flags, uint16_t parent, Vec3 pivot) {
    std::vector<uint8_t> record = Bone(flags);
    PutU16(record, BONE_PARENT, parent);
    PutFloat(record, BONE_PIVOT, pivot.x);
    PutFloat(record, BONE_PIVOT + 4, pivot.y);
    PutFloat(record, BONE_PIVOT + 8, pivot.z);
    return record;
}

// A head bone, then a chain of links plumes back along -x from it, each 0.1 long.
static std::vector<uint8_t> PlumeModel(uint16_t links, uint32_t sequences = 1) {
    std::vector<uint8_t> md20 = NewMd20();
    std::vector<uint8_t> records;
    for (uint32_t s = 0; s < sequences; s++) {
        records = Concat({ records, Sequence(static_cast<uint16_t>(s), 0x20) });
    }
    AddArray(md20, M2At::SEQUENCES, sequences, records);
    std::vector<uint8_t> bones = PivotBone(0, 0xFFFF, { 0, 0, 0 });
    for (uint16_t link = 0; link < links; link++) {
        bones = Concat({ bones, PivotBone(BONE_KINEMATIC, link, { -0.1f * link, 0, 0.1f }) });
    }
    AddArray(md20, M2At::BONES, links + 1u, bones);
    return md20;
}

// The physics of PlumeModel: a static head body and a dynamic body per link, each hung by a shoulder of cones[i].
static std::vector<uint8_t> PlumePhysics(const std::vector<float>& cones) {
    std::vector<std::pair<uint16_t, uint16_t>> bodies = { { BODY_STATIC, 0 } };
    std::vector<Joint> joints;
    for (uint32_t link = 0; link < cones.size(); link++) {
        bodies.push_back({ BODY_DYNAMIC, static_cast<uint16_t>(link + 1) });
        joints.push_back({ link, link + 1, JOINT_SHOULDER, { -0.9f, 0, 0 }, cones[link] });
    }
    return Physics(bodies, joints);
}

static size_t BoneAt(const std::vector<uint8_t>& m2, size_t bone) {
    return RecordAt(m2, M2At::BONES, bone, BONE_SIZE);
}

// The single rotation key a bone's track holds for sequence s, as the 3.3.5 client decodes it; false when none.
static bool RotationKey(const std::vector<uint8_t>& m2, size_t bone, size_t s, float q[4]) {
    size_t track = BoneAt(m2, bone) + BONE_ROTATION;
    if (GetU32(m2, track + 4) <= s || GetU32(m2, track + 0x0C) <= s) {
        return false;
    }
    std::pair<uint32_t, uint32_t> times = TrackEntry(m2, track, s, false);
    std::pair<uint32_t, uint32_t> keys = TrackEntry(m2, track, s, true);
    if (times.first != 1 || keys.first != 1 || GetU32(m2, times.second) != 0) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        q[i] = GetU16(m2, keys.second + i * 2) / 32767.5f - 1.0f;
    }
    return true;
}

static Vec3 Rotate(const float q[4], Vec3 v) {
    // v + 2w(u x v) + 2u x (u x v), u = q.xyz.
    Vec3 u = { q[0], q[1], q[2] };
    Vec3 t = { 2 * (u.y * v.z - u.z * v.y), 2 * (u.z * v.x - u.x * v.z), 2 * (u.x * v.y - u.y * v.x) };
    return { v.x + q[3] * t.x + (u.y * t.z - u.z * t.y), v.y + q[3] * t.y + (u.z * t.x - u.x * t.z), v.z + q[3] * t.z + (u.x * t.y - u.y * t.x) };
}

static bool Near(Vec3 a, Vec3 b) {
    constexpr float TOLERANCE = 0.002f;
    return std::fabs(a.x - b.x) < TOLERANCE && std::fabs(a.y - b.y) < TOLERANCE && std::fabs(a.z - b.z) < TOLERANCE;
}

static Vec3 Swung(float degrees) {
    float radians = degrees * 3.14159265f / 180.0f;
    return { -std::cos(radians), 0, -std::sin(radians) };
}

static bool Bake(std::vector<uint8_t>& m2, const std::vector<uint8_t>& physics, std::vector<std::string>& warnings) {
    std::string error;
    bool baked = BakePhysicsPose(m2, physics, warnings, error);
    if (!baked) {
        std::printf("    (%s)\n", error.c_str());
    }
    return baked;
}

TEST(PhysicsBodyHangsStraightDownWithinItsCone) {
    std::vector<uint8_t> m2 = PlumeModel(1);
    std::vector<std::string> warnings;
    float q[4];

    CHECK(Bake(m2, PlumePhysics({ 120 }), warnings));
    CHECK(RotationKey(m2, 1, 0, q) && Near(Rotate(q, { -1, 0, 0 }), { 0, 0, -1 }));
    CHECK(GetU32(m2, BoneAt(m2, 1) + BONE_FLAGS) & BONE_TRANSFORMED);
    CHECK(!RotationKey(m2, 0, 0, q));
    CHECK(warnings.empty());
}

TEST(PhysicsBodySwingsNoFurtherThanItsCone) {
    std::vector<uint8_t> m2 = PlumeModel(1);
    std::vector<std::string> warnings;
    float q[4];

    CHECK(Bake(m2, PlumePhysics({ 30 }), warnings));
    CHECK(RotationKey(m2, 1, 0, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(30)));
}

TEST(PhysicsChainLinksTakeTheirOwnSwingOnTopOfTheirParents) {
    std::vector<uint8_t> m2 = PlumeModel(3);
    std::vector<std::string> warnings;
    float q[4];

    // 50 + 30 swings the second link to 80 degrees; the third has 10 left to straight down, under its cone of 60.
    CHECK(Bake(m2, PlumePhysics({ 50, 30, 60 }), warnings));
    CHECK(RotationKey(m2, 1, 0, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(50)));
    CHECK(RotationKey(m2, 2, 0, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(30)));
    CHECK(RotationKey(m2, 3, 0, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(10)));
}

TEST(PhysicsPoseHoldsInEverySequence) {
    std::vector<uint8_t> m2 = PlumeModel(1, 3);
    std::vector<std::string> warnings;
    float q[4];

    CHECK(Bake(m2, PlumePhysics({ 30 }), warnings));
    for (size_t s = 0; s < 3; s++) {
        CHECK(RotationKey(m2, 1, s, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(30)));
    }
}

TEST(PhysicsBodyAlreadyHangingIsLeftAlone) {
    std::vector<uint8_t> m2 = PlumeModel(1);
    std::vector<std::string> warnings;
    float q[4];
    std::vector<Joint> joints = { { 0, 1, JOINT_SHOULDER, { 0, 0, -0.8f }, 60 } };

    CHECK(Bake(m2, Physics({ { BODY_STATIC, 0 }, { BODY_DYNAMIC, 1 } }, joints), warnings));
    CHECK(!RotationKey(m2, 1, 0, q));
}

TEST(PhysicsWeldedBodyFollowsItsParent) {
    std::vector<uint8_t> m2 = PlumeModel(2);
    std::vector<std::string> warnings;
    float q[4];
    std::vector<Joint> joints = { { 0, 1, JOINT_SHOULDER, { -0.9f, 0, 0 }, 40 }, { 1, 2, JOINT_WELD, {}, 0 } };

    CHECK(Bake(m2, Physics({ { BODY_STATIC, 0 }, { BODY_DYNAMIC, 1 }, { BODY_DYNAMIC, 2 } }, joints), warnings));
    CHECK(RotationKey(m2, 1, 0, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(40)));
    CHECK(!RotationKey(m2, 2, 0, q));
}

TEST(PhysicsKeepsBonesThatAreAnimated) {
    std::vector<uint8_t> m2 = PlumeModel(1);
    size_t track = BoneAt(m2, 1) + BONE_ROTATION;
    m2.resize((m2.size() + 15) & ~size_t(15), 0);
    uint32_t keys = static_cast<uint32_t>(m2.size());
    m2.insert(m2.end(), 8, 0x11);
    SetTrack(m2, track, { { { 1, keys } }, { { 1, keys } } });
    std::vector<uint8_t> before = m2;
    std::vector<std::string> warnings;

    CHECK(Bake(m2, PlumePhysics({ 30 }), warnings));
    CHECK(TrackEntry(m2, track, 0, true) == TrackEntry(before, track, 0, true));
}

TEST(PhysicsInAnUnknownLayoutIsWarnedAboutAndSkipped) {
    std::vector<uint8_t> m2 = PlumeModel(1);
    std::vector<uint8_t> before = m2;
    std::vector<std::string> warnings;
    std::vector<uint8_t> version(2, 0);
    PutU16(version, 0, 6);

    CHECK(Bake(m2, Concat({ PhysChunk("PHYS", version), PhysChunk("BDY3", Body(BODY_DYNAMIC, 1)) }), warnings));
    CHECK(m2 == before);
    CHECK(warnings.size() == 1 && warnings[0].find("BDY4") != std::string::npos);
}

TEST(PhysicsBodyOnAMissingBoneIsRefused) {
    std::vector<uint8_t> m2 = PlumeModel(1);
    std::vector<std::string> warnings;
    std::string error;
    std::vector<Joint> joints = { { 0, 1, JOINT_SHOULDER, { -1, 0, 0 }, 30 } };

    CHECK(!BakePhysicsPose(m2, Physics({ { BODY_STATIC, 0 }, { BODY_DYNAMIC, 9 } }, joints), warnings, error) && error.find("bone") != std::string::npos);
}

TEST(ConvertedModelsBakeTheirEmbeddedPhysics) {
    std::vector<uint8_t> md20 = PlumeModel(1);
    AddArray(md20, M2At::VERTICES, 1, Vertex(255, 0, 0, 0));
    ConvertedModel model;
    std::string error;
    float q[4];

    CHECK(ConvertModel(Md21File(md20, { Chunk("PFDC", PlumePhysics({ 30 })) }), {}, model, error));
    CHECK(RotationKey(model.m2, 1, 0, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(30)));
}

TEST(ConvertedModelsBakeThePhysicsFileTheyName) {
    std::vector<uint8_t> md20 = PlumeModel(1);
    AddArray(md20, M2At::VERTICES, 1, Vertex(255, 0, 0, 0));
    Retail::ModelSources sources;
    sources.read = [](uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error) {
        if (fileDataId != 4242) {
            error = "missing";
            return false;
        }
        bytes = PlumePhysics({ 30 });
        return true;
    };
    ConvertedModel model;
    std::string error;
    float q[4];

    CHECK(ConvertModel(Md21File(md20, { Chunk("PFID", U32s({ 4242 })) }), sources, model, error));
    CHECK(RotationKey(model.m2, 1, 0, q) && Near(Rotate(q, { -1, 0, 0 }), Swung(30)));
}
