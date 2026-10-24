#include "Engine/Source/Retail/RetailRecords.h"
#include <algorithm>
#include <cmath>
#include <set>
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

// From version 271 the constant FoV is gone and the record ends in an FoV track. The track's unit is
// unverified, so the stock 3.3.5 values are used, as wow-toolbox-editor's downporter does.
constexpr size_t RETAIL_CAMERA_SIZE = 116;
constexpr size_t CAMERA_SIZE = 100;
constexpr size_t RETAIL_SHARED_FIELDS = 0x04;       // farClip through roll: the same bytes 4 later in 3.3.5.
constexpr size_t RETAIL_FOV_TRACK = 0x60;
constexpr uint32_t PORTRAIT_CAMERA = 0;
constexpr float PORTRAIT_FOV = 0.7f;
constexpr float DEFAULT_FOV = 0.97f;

// Retail emitters are the 3.3.5 record plus 16 bytes of texture scroll at the end.
constexpr size_t RETAIL_PARTICLE_SIZE = 492;
constexpr size_t PARTICLE_SIZE = 476;

namespace ParticleAt {
    constexpr size_t FLAGS = 0x04;
    constexpr size_t TEXTURE = 0x16;
    constexpr size_t BLEND = 0x28;
    constexpr size_t MULTI_TEXTURE_SCALE = 0x2C;      // unused_002C in 3.3.5.
    constexpr size_t GRAVITY = 0x84;
}

// The bits the stock 3.3.5 emitters carry (wow-toolbox-editor downport/limits.rs).
constexpr uint32_t PARTICLE_FLAGS_MASK = 0xFFFFF;
constexpr uint32_t PARTICLE_REFRACTION = 0x100000;
constexpr uint32_t PARTICLE_COMPRESSED_GRAVITY = 0x800000;
constexpr uint32_t PARTICLE_MULTI_TEXTURE = 0x10000000;
constexpr uint16_t PARTICLE_FIRST_TEXTURE = 0x1F;  // Multi-texture packs three 5-bit indices.
constexpr uint8_t PARTICLE_BLEND_BLEND_ADD = 7;
constexpr uint8_t PARTICLE_BLEND_ADD = 4;
constexpr uint16_t INTERPOLATION_HERMITE = 2;      // Hermite and bezier keys carry two tangents.
// One step of a packed gravity magnitude (wowdev M2 "compressed particle gravity").
constexpr float GRAVITY_UNIT = 0.04238648f;

bool ConvertCameras(std::vector<uint8_t>& m2, std::string& error) {
    Span cameras;
    if (!ReadSpan(m2, M2At::CAMERAS, RETAIL_CAMERA_SIZE, "cameras", cameras, error)) {
        return false;
    }

    if (!cameras.count) {
        return true;
    }

    std::vector<uint8_t> converted(cameras.count * CAMERA_SIZE, 0);

    for (uint32_t i = 0; i < cameras.count; i++) {
        size_t from = cameras.At(i, RETAIL_CAMERA_SIZE);
        size_t to = i * CAMERA_SIZE;
        uint32_t type = Get<uint32_t>(m2, from);

        Put<uint32_t>(converted, to, type);
        Put<float>(converted, to + 4, type == PORTRAIT_CAMERA ? PORTRAIT_FOV : DEFAULT_FOV);
        std::memcpy(converted.data() + to + 8, m2.data() + from + RETAIL_SHARED_FIELDS, RETAIL_FOV_TRACK - RETAIL_SHARED_FIELDS);
    }

    Put<uint32_t>(m2, M2At::CAMERAS + 4, AppendAligned(m2, converted));
    return true;
}

static std::string EmitterName(uint32_t index) {
    return "particle emitter " + std::to_string(index);
}

// One 476-byte record in place: features 3.3.5 lacks are dropped, the lossy ones with a warning.
static void ConvertParticle(std::vector<uint8_t>& records, size_t at, uint32_t index, std::vector<std::string>& warnings) {
    uint32_t flags = Get<uint32_t>(records, at + ParticleAt::FLAGS);

    if (flags & PARTICLE_MULTI_TEXTURE) {
        Put<uint16_t>(records, at + ParticleAt::TEXTURE, Get<uint16_t>(records, at + ParticleAt::TEXTURE) & PARTICLE_FIRST_TEXTURE);
        warnings.push_back(EmitterName(index) + ": multi-texture particles draw their first texture only");
    }

    if (flags & PARTICLE_REFRACTION) {
        warnings.push_back(EmitterName(index) + ": refraction is drawn as a plain particle");
    }

    Put<uint32_t>(records, at + ParticleAt::FLAGS, flags & PARTICLE_FLAGS_MASK);
    Put<uint16_t>(records, at + ParticleAt::MULTI_TEXTURE_SCALE, 0);

    if (records[at + ParticleAt::BLEND] == PARTICLE_BLEND_BLEND_ADD) {
        records[at + ParticleAt::BLEND] = PARTICLE_BLEND_ADD;
    }
}

bool ConvertParticles(std::vector<uint8_t>& m2, std::vector<uint32_t>& compressed, std::vector<std::string>& warnings, std::string& error) {
    Span particles;
    if (!ReadSpan(m2, M2At::PARTICLES, RETAIL_PARTICLE_SIZE, "particle emitters", particles, error)) {
        return false;
    }

    if (!particles.count) {
        return true;
    }

    std::vector<uint8_t> converted(particles.count * PARTICLE_SIZE, 0);

    for (uint32_t i = 0; i < particles.count; i++) {
        size_t to = i * PARTICLE_SIZE;
        std::memcpy(converted.data() + to, m2.data() + particles.At(i, RETAIL_PARTICLE_SIZE), PARTICLE_SIZE);

        if (Get<uint32_t>(converted, to + ParticleAt::FLAGS) & PARTICLE_COMPRESSED_GRAVITY) {
            compressed.push_back(i);
        }

        ConvertParticle(converted, to, i, warnings);
    }

    Put<uint32_t>(m2, M2At::PARTICLES + 4, AppendAligned(m2, converted));
    return true;
}

// A packed key (int8 x, int8 y, int16 magnitude; z completes the unit vector, flipped by a negative
// magnitude) as the 3.3.5 gravity float, which pulls along -z.
static float UnpackGravity(const std::vector<uint8_t>& m2, size_t at) {
    float x = static_cast<int8_t>(m2[at]) / 128.0f;
    float y = static_cast<int8_t>(m2[at + 1]) / 128.0f;
    float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));
    float magnitude = Get<int16_t>(m2, at + 2) * GRAVITY_UNIT;

    if (magnitude < 0) {
        z = -z;
        magnitude = -magnitude;
    }

    // 0 - pull, not -pull: no gravity stays +0.0, as the downporter writes it.
    return 0.0f - z * magnitude;
}

static bool UnpackGravityKeys(std::vector<uint8_t>& m2, const Span& keys, size_t valuesPerKey, std::string& error) {
    size_t values = size_t(keys.count) * valuesPerKey;
    if (uint64_t(keys.offset) + values * 4 > m2.size()) {
        return Fail(error, "particle gravity keys run past the end of the file");
    }

    for (size_t v = 0; v < values; v++) {
        Put<float>(m2, keys.At(v, 4), UnpackGravity(m2, keys.At(v, 4)));
    }

    return true;
}

bool DecodeParticleGravity(std::vector<uint8_t>& m2, const std::vector<uint32_t>& compressed, std::string& error) {
    Span particles;
    if (!ReadSpan(m2, M2At::PARTICLES, PARTICLE_SIZE, "particle emitters", particles, error)) {
        return false;
    }

    // Sequences and emitters may share one key array; unpacking it twice would read floats as packed keys.
    std::set<uint32_t> unpacked;

    for (uint32_t index : compressed) {
        size_t track = particles.At(index, PARTICLE_SIZE) + ParticleAt::GRAVITY;
        size_t valuesPerKey = Get<uint16_t>(m2, track + TrackAt::INTERPOLATION) >= INTERPOLATION_HERMITE ? 3 : 1;
        Span entries;
        if (!ReadSpan(m2, track + TrackAt::KEYS, 8, "particle gravity keys", entries, error)) {
            return false;
        }

        for (uint32_t s = 0; s < entries.count; s++) {
            Span keys { Get<uint32_t>(m2, entries.At(s, 8)), Get<uint32_t>(m2, entries.At(s, 8) + 4) };
            if (keys.count && unpacked.insert(keys.offset).second && !UnpackGravityKeys(m2, keys, valuesPerKey, error)) {
                return false;
            }
        }
    }

    return true;
}

}
