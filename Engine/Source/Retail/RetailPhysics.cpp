#include "Engine/Source/Retail/RetailPhysics.h"
#include "Engine/Source/Retail/RetailModelData.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace Retail {

// Layouts from wowdev.wiki PHYS, checked on the Ny'alotha plate helm (retail/2916410.m2, PHYS version 6).
constexpr size_t BODY_SIZE = 48;            // BDY4: u16 type, u16 bone, then position, shapes and dynamics.
constexpr size_t JOIN_SIZE = 16;            // u32 body A, u32 body B, 4 unknown bytes, u16 type, u16 index.
constexpr size_t SHOULDER_SIZE = 108;       // SHOJ: frame A and B (3x4 each), twist limits, cone; SHJ2 adds a motor.
constexpr size_t SHOULDER2_SIZE = 124;
constexpr size_t SHOULDER_AXIS = 6 * 4;     // Frame A's third row: the twist axis, the body's bind direction.
constexpr size_t SHOULDER_CONE = 26 * 4;    // Degrees.
constexpr uint16_t BODY_DYNAMIC = 1;
constexpr uint16_t JOINT_SPHERICAL = 0;
constexpr uint16_t JOINT_SHOULDER = 1;
constexpr size_t BONE_FLAGS = 0x04;
constexpr size_t BONE_PARENT = 0x08;
constexpr size_t BONE_ROTATION = 0x24;
constexpr uint32_t BONE_TRANSFORMED = 0x200;   // The client animates only bones with 0x200 or 0x80 (M2Model.cpp).
constexpr float PI = 3.14159265f;
constexpr float SETTLED = 1e-4f;               // Radians: a smaller swing is none.

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
};

constexpr Vec3 DOWN = { 0, 0, -1 };

static float Dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 Cross(Vec3 a, Vec3 b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

static Vec3 Normalized(Vec3 v) {
    float length = std::sqrt(Dot(v, v));
    return length > 0 ? Vec3 { v.x / length, v.y / length, v.z / length } : Vec3 {};
}

static Quat Mul(Quat a, Quat b) {
    return { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
}

static Quat Conjugate(Quat q) {
    return { -q.x, -q.y, -q.z, q.w };
}

static Vec3 Rotate(Quat q, Vec3 v) {
    Quat p = Mul(Mul(q, { v.x, v.y, v.z, 0 }), Conjugate(q));
    return { p.x, p.y, p.z };
}

static Quat AxisAngle(Vec3 axis, float angle) {
    float s = std::sin(angle / 2);
    return { axis.x * s, axis.y * s, axis.z * s, std::cos(angle / 2) };
}

struct Body {
    uint16_t type;
    uint16_t bone;
};

struct Joint {
    uint32_t bodyA;
    uint32_t bodyB;
    uint16_t type;
    Vec3 axis;
    float cone;   // Radians the body may swing off axis.
};

struct Rig {
    std::vector<Body> bodies;
    std::vector<Joint> joints;
};

static float GetFloat(const std::vector<uint8_t>& data, size_t at) {
    return Get<float>(data, at);
}

// The chunks of a physics file, by tag; the file stores each tag's bytes reversed.
static std::map<std::string, std::vector<uint8_t>> PhysicsChunks(const std::vector<uint8_t>& physics) {
    std::map<std::string, std::vector<uint8_t>> chunks;
    for (auto& [tag, data] : ReadChunks(physics)) {
        chunks[std::string(tag.rbegin(), tag.rend())] = std::move(data);
    }
    return chunks;
}

// The rig, or nothing with a warning when the bodies or shoulders are in a layout this does not read.
static std::optional<Rig> ReadRig(const std::vector<uint8_t>& physics, std::vector<std::string>& warnings) {
    auto chunks = PhysicsChunks(physics);
    const std::vector<uint8_t>& bodies = chunks["BDY4"];
    const std::vector<uint8_t>& joins = chunks["JOIN"];
    bool second = !chunks["SHJ2"].empty();
    const std::vector<uint8_t>& shoulders = second ? chunks["SHJ2"] : chunks["SHOJ"];
    size_t shoulderSize = second ? SHOULDER2_SIZE : SHOULDER_SIZE;

    if (bodies.empty() || bodies.size() % BODY_SIZE || joins.size() % JOIN_SIZE || shoulders.size() % shoulderSize) {
        warnings.push_back("physics bodies are not BDY4 records or its joints are not whole records, so its bones keep their bind pose");
        return std::nullopt;
    }

    Rig rig;
    for (size_t at = 0; at < bodies.size(); at += BODY_SIZE) {
        rig.bodies.push_back({ Get<uint16_t>(bodies, at), Get<uint16_t>(bodies, at + 2) });
    }

    for (size_t at = 0; at < joins.size(); at += JOIN_SIZE) {
        Joint joint = { Get<uint32_t>(joins, at), Get<uint32_t>(joins, at + 4), Get<uint16_t>(joins, at + 12), {}, 0 };
        size_t shoulder = Get<uint16_t>(joins, at + 14) * shoulderSize;
        if (joint.type == JOINT_SHOULDER) {
            if (shoulder >= shoulders.size()) {
                warnings.push_back("physics shoulder joint " + std::to_string(at / JOIN_SIZE) + " has no SHOJ or SHJ2 record, so its bones keep their bind pose");
                return std::nullopt;
            }
            joint.axis = Normalized({ GetFloat(shoulders, shoulder + SHOULDER_AXIS), GetFloat(shoulders, shoulder + SHOULDER_AXIS + 4), GetFloat(shoulders, shoulder + SHOULDER_AXIS + 8) });
            joint.cone = GetFloat(shoulders, shoulder + SHOULDER_CONE) * PI / 180;
        }
        rig.joints.push_back(joint);
    }

    return rig;
}

// Body B's rotation once gravity settles it, from body A's: it swings from its bind direction toward straight down,
// up to the joint's cone.
static Quat Settle(const Joint& joint, Quat parent) {
    float cone = joint.type == JOINT_SHOULDER ? joint.cone : joint.type == JOINT_SPHERICAL ? PI : 0;
    Vec3 direction = Rotate(parent, joint.axis);
    float swing = std::min(std::acos(std::clamp(Dot(direction, DOWN), -1.0f, 1.0f)), cone);
    if (swing < SETTLED) {
        return parent;
    }

    // Straight up has no one way down: tip it over toward -x, the back of a helm.
    Vec3 pivot = Normalized(Cross(direction, DOWN));
    if (Dot(pivot, pivot) == 0) {
        pivot = { 0, -1, 0 };
    }
    return Mul(AxisAngle(pivot, swing), parent);
}

// The settled rotation of each body; static and kinematic bodies, and those no joint reaches, hold still.
static std::vector<std::optional<Quat>> SettleBodies(const Rig& rig) {
    std::vector<std::optional<Quat>> rotations(rig.bodies.size());
    for (size_t b = 0; b < rig.bodies.size(); b++) {
        if (rig.bodies[b].type != BODY_DYNAMIC) {
            rotations[b] = Quat {};
        }
    }

    for (bool progressed = true; progressed;) {
        progressed = false;
        for (const Joint& joint : rig.joints) {
            if (joint.bodyA < rotations.size() && joint.bodyB < rotations.size() && rotations[joint.bodyA] && !rotations[joint.bodyB]) {
                rotations[joint.bodyB] = Settle(joint, *rotations[joint.bodyA]);
                progressed = true;
            }
        }
    }

    return rotations;
}

static bool IsAnimated(const std::vector<uint8_t>& m2, size_t track, std::string& error, bool& animated) {
    Span keys;
    if (!ReadSpan(m2, track + TrackAt::KEYS, 8, "bone rotation keys", keys, error)) {
        return false;
    }

    animated = false;
    for (uint32_t s = 0; s < keys.count; s++) {
        animated = animated || Get<uint32_t>(m2, keys.At(s, 8)) > 0;
    }
    return true;
}

static uint16_t Compress(float value) {
    return static_cast<uint16_t>(std::clamp(std::lround((value + 1.0f) * 32767.5f), 0L, 65535L));
}

// Points the rotation track at track to one key, rotation, at time 0 of each of sequences.
static void PutRestKey(std::vector<uint8_t>& m2, size_t track, Quat rotation, uint32_t sequences) {
    std::vector<uint8_t> time(4, 0);
    std::vector<uint8_t> key(8);
    Put<uint16_t>(key, 0, Compress(rotation.x));
    Put<uint16_t>(key, 2, Compress(rotation.y));
    Put<uint16_t>(key, 4, Compress(rotation.z));
    Put<uint16_t>(key, 6, Compress(rotation.w));
    uint32_t timeAt = AppendAligned(m2, time);
    uint32_t keyAt = AppendAligned(m2, key);

    std::vector<uint8_t> times(sequences * 8);
    std::vector<uint8_t> keys(sequences * 8);
    for (uint32_t s = 0; s < sequences; s++) {
        Put<uint32_t>(times, s * 8, 1);
        Put<uint32_t>(times, s * 8 + 4, timeAt);
        Put<uint32_t>(keys, s * 8, 1);
        Put<uint32_t>(keys, s * 8 + 4, keyAt);
    }
    uint32_t timesAt = AppendAligned(m2, times);
    uint32_t keysAt = AppendAligned(m2, keys);

    Put<uint16_t>(m2, track + TrackAt::INTERPOLATION, 0);
    Put<uint16_t>(m2, track + TrackAt::GLOBAL_SEQUENCE, NO_INDEX);
    Put<uint32_t>(m2, track + TrackAt::TIMES, sequences);
    Put<uint32_t>(m2, track + TrackAt::TIMES + 4, timesAt);
    Put<uint32_t>(m2, track + TrackAt::KEYS, sequences);
    Put<uint32_t>(m2, track + TrackAt::KEYS + 4, keysAt);
}

bool BakePhysicsPose(std::vector<uint8_t>& m2, const std::vector<uint8_t>& physics, std::vector<std::string>& warnings, std::string& error) {
    std::optional<Rig> rig = ReadRig(physics, warnings);
    if (!rig) {
        return true;
    }

    Span bones;
    Span sequences;
    if (!ReadSpan(m2, M2At::BONES, BONE_SIZE, "bones", bones, error) || !ReadSpan(m2, M2At::SEQUENCES, SEQUENCE_SIZE, "sequences", sequences, error)) {
        return false;
    }

    std::vector<std::optional<Quat>> bodyRotations = SettleBodies(*rig);
    std::vector<std::optional<Quat>> settled(bones.count);
    for (size_t b = 0; b < rig->bodies.size(); b++) {
        const Body& body = rig->bodies[b];
        if (body.bone >= bones.count) {
            return Fail(error, "physics body " + std::to_string(b) + " is on bone " + std::to_string(body.bone) + ", past the model's " + std::to_string(bones.count) + " bones");
        }
        if (body.type == BODY_DYNAMIC) {
            settled[body.bone] = bodyRotations[b];
        }
    }

    // Bones nest their parent's rotation, so each keeps only the part of its body's rotation its parent lacks.
    std::vector<Quat> world(bones.count);
    for (uint32_t i = 0; i < bones.count; i++) {
        size_t bone = bones.At(i, BONE_SIZE);
        uint16_t parent = Get<uint16_t>(m2, bone + BONE_PARENT);
        Quat parentWorld = parent < i ? world[parent] : Quat {};
        world[i] = settled[i].value_or(parentWorld);

        Quat local = Mul(Conjugate(parentWorld), world[i]);
        if (!settled[i] || 2 * std::acos(std::min(std::fabs(local.w), 1.0f)) < SETTLED) {
            continue;
        }

        bool animated = false;
        if (!IsAnimated(m2, bone + BONE_ROTATION, error, animated)) {
            return false;
        }
        if (animated) {
            continue;
        }

        PutRestKey(m2, bone + BONE_ROTATION, local, std::max<uint32_t>(sequences.count, 1));
        Put<uint32_t>(m2, bone + BONE_FLAGS, Get<uint32_t>(m2, bone + BONE_FLAGS) | BONE_TRANSFORMED);
    }

    return true;
}

}
