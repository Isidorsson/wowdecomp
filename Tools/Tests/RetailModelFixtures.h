#ifndef TOOLS_TESTS_RETAILMODELFIXTURES_H
#define TOOLS_TESTS_RETAILMODELFIXTURES_H

#include <cstring>
#include <vector>
#include "Tools/Tests/M2Builder.h"

// Retail model files for the seed export tests, byte by byte: models with the files they name, skins, animations and
// skeletons.

// Bytes that name their file, so a file served in place of another is noticed.
inline std::vector<uint8_t> Blob(const char* tag, uint32_t id) {
    std::vector<uint8_t> bytes(tag, tag + 4);
    AppendU32(bytes, id);
    return bytes;
}

inline size_t AppendAligned16(std::vector<uint8_t>& blob, const std::vector<uint8_t>& bytes) {
    blob.resize((blob.size() + 15) & ~size_t(15), 0);
    size_t at = blob.size();
    blob.insert(blob.end(), bytes.begin(), bytes.end());
    return at;
}

// The files a model names: SFID skins, TXID textures (one per type 0 slot), the .anim of its walk sequence or the
// skeleton (SKID) that replaces its sequences and bones. A skin slot is a texture of type 11, which a display fills.
struct ModelParts {
    std::vector<uint32_t> skins;
    std::vector<uint32_t> textures;
    uint32_t walkAnim = 0;
    uint32_t skeleton = 0;
    bool skinSlot = false;
};

// Stand (embedded) and Walk (id 4) on one bone; the walk's keys are in its .anim when it has one.
inline void AddWalk(std::vector<uint8_t>& md20, bool external) {
    AddArray(md20, M2At::SEQUENCES, 2, Concat({ Sequence(0, 0x20), Sequence(4, external ? 0 : 0x20) }));
    size_t bone = AddArray(md20, M2At::BONES, 1, Bone(0));
    size_t times = AppendAligned16(md20, U32s({ 0 }));
    size_t keys = AppendAligned16(md20, std::vector<uint8_t>(12, 1));
    SetTrack(md20, bone + 0x10, { { { 1, uint32_t(times) }, { 2, 0 } }, { { 1, uint32_t(keys) }, { 2, 8 } } });
}

inline std::vector<uint8_t> RetailModel(const ModelParts& parts) {
    std::vector<uint8_t> md20 = NewMd20();
    PutU32(md20, M2At::VIEWS, 1);
    if (!parts.skeleton) {
        AddWalk(md20, parts.walkAnim != 0);
    }
    AddArray(md20, M2At::VERTICES, 2, Concat({ Vertex(255, 0, 0, 0), Vertex(128, 127, 0, 0) }));
    std::vector<uint8_t> textures;
    for (size_t i = 0; i < parts.textures.size(); i++) {
        textures = Concat({ textures, Texture(0) });
    }
    if (parts.skinSlot) {
        textures = Concat({ textures, Texture(11) });
    }
    if (!textures.empty()) {
        AddArray(md20, M2At::TEXTURES, static_cast<uint32_t>(parts.textures.size() + parts.skinSlot), textures);
    }
    AddArray(md20, M2At::TEXTURE_COMBOS, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 1, 0 } }));

    std::vector<std::vector<uint8_t>> chunks = { Chunk("SFID", U32s(parts.skins)) };
    if (!parts.textures.empty()) {
        chunks.push_back(Chunk("TXID", U32s(parts.textures)));
    }
    if (parts.walkAnim && !parts.skeleton) {
        chunks.push_back(Chunk("AFID", AfidRow(4, 0, parts.walkAnim)));
    }
    if (parts.skeleton) {
        chunks.push_back(Chunk("SKID", U32s({ parts.skeleton })));
    }
    return Md21File(md20, chunks);
}

// A skeleton file as the converter reads it (RetailSkeleton.h): sequences, bones, an optional parent and the
// animation files of its sequences. A bone's run list gives, per sequence, no key (0), a key kept in the chunk
// (positive) or a key in the sequence's .anim (negative).
inline std::vector<uint8_t> SkelU16s(const std::vector<uint16_t>& values) {
    std::vector<uint8_t> bytes;
    for (uint16_t value : values) {
        bytes.push_back(static_cast<uint8_t>(value));
        bytes.push_back(static_cast<uint8_t>(value >> 8));
    }
    return bytes;
}

inline std::vector<uint8_t> SkelKey(float x) {
    std::vector<uint8_t> bytes(12);
    for (size_t i = 0; i < 3; i++) {
        std::memcpy(bytes.data() + i * 4, &x, 4);
    }
    return bytes;
}

// The keys of one sequence of an .anim part: timestamp 0, then one translation key of x.
inline std::vector<uint8_t> SkelAnimPart(float x) {
    return Concat({ U32s({ 0 }), SkelKey(x) });
}

inline std::vector<uint8_t> SkelSequences(const std::vector<std::vector<uint8_t>>& sequences, const std::vector<uint16_t>& lookup) {
    std::vector<uint8_t> payload(32, 0);
    AddArray(payload, 8, static_cast<uint32_t>(sequences.size()), Concat(sequences));
    AddArray(payload, 16, static_cast<uint32_t>(lookup.size()), SkelU16s(lookup));
    return payload;
}

inline std::vector<uint8_t> SkelBones(const std::vector<std::vector<float>>& bones) {
    constexpr size_t BONE_TRANSLATION = 0x10;
    std::vector<uint8_t> payload(16, 0);
    size_t at = AddArray(payload, 0, static_cast<uint32_t>(bones.size()), Concat(std::vector<std::vector<uint8_t>>(bones.size(), Bone(0))));
    AddArray(payload, 8, 1, SkelU16s({ 0 }));
    for (size_t b = 0; b < bones.size(); b++) {
        size_t track = at + b * 0x58 + BONE_TRANSLATION;
        PutU16(payload, track, 1);
        TrackEntries entries;
        for (float run : bones[b]) {
            if (run == 0) {
                entries.times.push_back({ 0, 0 });
                entries.keys.push_back({ 0, 0 });
            } else if (run < 0) {
                entries.times.push_back({ 1, 0 });
                entries.keys.push_back({ 1, 4 });
            } else {
                entries.times.push_back({ 1, uint32_t(AppendAligned16(payload, U32s({ 0 }))) });
                entries.keys.push_back({ 1, uint32_t(AppendAligned16(payload, SkelKey(run))) });
            }
        }
        SetTrack(payload, track, entries);
    }
    return payload;
}

struct SkeletonParts {
    std::vector<uint8_t> sequences = SkelSequences({}, {});
    std::vector<uint8_t> bones = SkelBones({});
    uint32_t parent = 0;
    std::vector<uint8_t> afid;
};

inline std::vector<uint8_t> SkeletonFile(const SkeletonParts& skeleton) {
    std::vector<uint8_t> file = Concat({ Chunk("SKL1", std::vector<uint8_t>(16, 0)), Chunk("SKS1", skeleton.sequences), Chunk("SKB1", skeleton.bones) });
    if (skeleton.parent) {
        std::vector<uint8_t> parent(16, 0);
        PutU32(parent, 8, skeleton.parent);
        file = Concat({ file, Chunk("SKPD", parent) });
    }
    return skeleton.afid.empty() ? file : Concat({ file, Chunk("AFID", skeleton.afid) });
}

// One view of two vertices, drawn by one batch.
inline std::vector<uint8_t> RetailSkin() {
    std::vector<uint8_t> skin = NewSkin();
    AddArray(skin, SkinAt::VERTICES, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::INDICES, 3, Concat({ { 0, 0 }, { 1, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::SECTIONS, 1, SkinSection(1, 0, 0, 3));
    AddArray(skin, SkinAt::BATCHES, 1, SkinBatch(0, 0x8000, 2));
    return skin;
}

// Timestamps 0 and 500, then two 12-byte translation keys: the .anim body of the walk sequence.
inline std::vector<uint8_t> WalkAnim() {
    std::vector<uint8_t> body = U32s({ 0, 500 });
    body.resize(8 + 24, 0x5A);
    return body;
}

#endif
