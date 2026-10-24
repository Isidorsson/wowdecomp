#include "Engine/Source/Retail/RetailAnimation.h"
#include <algorithm>
#include <map>
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

// An M2Track inside a record: where it sits and how big one key is (0 for a keyless M2TrackBase).
struct TrackField {
    size_t offset;
    size_t keySize;
};

// Every record type with tracks the client relocates per sequence (M2Init in M2Shared.cpp), in the 3.3.5
// layout: cameras are already converted when animations are inlined. Particles arrive with slice C4.
struct TrackedRecords {
    size_t header;
    size_t recordSize;
    std::vector<TrackField> tracks;
};

static const TrackedRecords TRACKED[] = {
    { M2At::BONES, 0x58, { { 0x10, 12 }, { 0x24, 8 }, { 0x38, 12 } } },
    { M2At::COLORS, 0x28, { { 0x00, 12 }, { 0x14, 2 } } },
    { M2At::TEXTURE_WEIGHTS, 0x14, { { 0x00, 2 } } },
    { M2At::TEXTURE_TRANSFORMS, 0x3C, { { 0x00, 12 }, { 0x14, 16 }, { 0x28, 12 } } },
    { M2At::ATTACHMENTS, 0x28, { { 0x14, 1 } } },
    { M2At::EVENTS, 0x24, { { 0x18, 0 } } },
    { M2At::LIGHTS, 0x9C, { { 0x10, 12 }, { 0x24, 4 }, { 0x38, 12 }, { 0x4C, 4 }, { 0x60, 4 }, { 0x74, 4 }, { 0x88, 1 } } },
    { M2At::CAMERAS, 0x64, { { 0x10, 36 }, { 0x30, 36 }, { 0x50, 12 } } },
    { M2At::RIBBONS, 0xB0, { { 0x24, 12 }, { 0x38, 2 }, { 0x4C, 4 }, { 0x60, 4 }, { 0x84, 2 }, { 0x98, 1 } } },
};

constexpr uint16_t NO_GLOBAL_LOOP = 0xFFFF;
constexpr size_t TRACK_TIMES = 0x04;
constexpr size_t TRACK_KEYS = 0x0C;
constexpr size_t TIME_SIZE = 4;

static std::string SequenceName(uint16_t id, uint16_t variation) {
    return "animation " + std::to_string(id) + "." + std::to_string(variation);
}

// What one sequence's entries in every track become.
struct EntryChange {
    size_t sequence;
    uint32_t base;          // Added to each entry's offset.
    size_t limit;           // Bytes an entry may reach from its own offset (the .anim payload).
    bool empty;             // Clear the entries instead.
};

// One per-sequence (count, offset) entry of the table at table, for elements of size bytes.
static bool ChangeEntry(std::vector<uint8_t>& m2, const Span& table, const EntryChange& change, size_t size, std::string& error) {
    if (change.sequence >= table.count) {
        return true;
    }

    size_t entry = table.At(change.sequence, 8);
    uint32_t count = Get<uint32_t>(m2, entry);
    uint32_t offset = Get<uint32_t>(m2, entry + 4);

    if (change.empty) {
        Put<uint32_t>(m2, entry, 0);
        Put<uint32_t>(m2, entry + 4, 0);
        return true;
    }

    if (count && uint64_t(offset) + uint64_t(count) * size > change.limit) {
        return Fail(error, "a track's keys run past the end of their .anim file");
    }

    Put<uint32_t>(m2, entry + 4, count ? offset + change.base : 0);
    return true;
}

static bool ChangeTracks(std::vector<uint8_t>& m2, const EntryChange& change, std::string& error) {
    for (const TrackedRecords& records : TRACKED) {
        Span span;
        if (!ReadSpan(m2, records.header, records.recordSize, "animated records", span, error)) {
            return false;
        }

        for (uint32_t r = 0; r < span.count; r++) {
            for (const TrackField& field : records.tracks) {
                size_t track = span.At(r, records.recordSize) + field.offset;
                if (Get<uint16_t>(m2, track + 2) != NO_GLOBAL_LOOP) {
                    continue;
                }

                Span times;
                Span keys;
                if (!ReadSpan(m2, track + TRACK_TIMES, 8, "track timestamps", times, error) || !ChangeEntry(m2, times, change, TIME_SIZE, error)) {
                    return false;
                }

                if (field.keySize && (!ReadSpan(m2, track + TRACK_KEYS, 8, "track keys", keys, error) || !ChangeEntry(m2, keys, change, field.keySize, error))) {
                    return false;
                }
            }
        }
    }

    return true;
}

// The bytes track offsets of an .anim file count from: the whole file, or the AFM2 payload of a chunked one.
static bool AnimPayload(const std::vector<uint8_t>& file, std::vector<uint8_t>& payload, std::string& error) {
    bool chunked = file.size() >= 4 && (!std::memcmp(file.data(), "AFM2", 4) || !std::memcmp(file.data(), "AFSA", 4) || !std::memcmp(file.data(), "AFSB", 4));
    if (!chunked) {
        payload = file;
        return true;
    }

    auto chunks = ReadChunks(file);
    auto model = chunks.find("AFM2");
    if (model == chunks.end()) {
        return Fail(error, "skeleton-only .anim data (AFSA/AFSB) is merged by converter slice C3");
    }

    payload = model->second;
    return true;
}

bool InlineAnimations(std::vector<uint8_t>& m2, const std::vector<uint8_t>& afid, const FileReader& read, std::vector<std::string>& warnings, std::string& error) {
    std::map<std::pair<uint16_t, uint16_t>, uint32_t> files;
    for (size_t at = 0; at + 8 <= afid.size(); at += 8) {
        files[{ Get<uint16_t>(afid, at), Get<uint16_t>(afid, at + 2) }] = Get<uint32_t>(afid, at + 4);
    }

    Span sequences;
    if (!ReadSpan(m2, M2At::SEQUENCES, SEQUENCE_SIZE, "sequences", sequences, error)) {
        return false;
    }

    for (uint32_t s = 0; s < sequences.count; s++) {
        size_t at = sequences.At(s, SEQUENCE_SIZE);
        uint32_t flags = Get<uint32_t>(m2, at + SequenceAt::FLAGS);
        uint16_t id = Get<uint16_t>(m2, at + SequenceAt::ID);
        uint16_t variation = Get<uint16_t>(m2, at + SequenceAt::VARIATION);

        if (flags & SEQUENCE_ALIAS) {
            continue;
        }

        // The client reads only 0x20 as "in the model"; retail also marks local data with 0x10 and 0x100.
        Put<uint32_t>(m2, at + SequenceAt::FLAGS, flags | SEQUENCE_EMBEDDED);
        auto file = files.find({ id, variation });

        if ((flags & SEQUENCE_LOCAL_BITS) || file == files.end() || file->second == 0) {
            continue;
        }

        std::vector<uint8_t> bytes;
        std::vector<uint8_t> payload;
        std::string readError;

        if (!read || !read(file->second, bytes, readError)) {
            warnings.push_back(SequenceName(id, variation) + ": .anim " + std::to_string(file->second) + " not read, so it plays the bind pose (" + readError + ")");
            if (!ChangeTracks(m2, { s, 0, 0, true }, error)) {
                return false;
            }
            continue;
        }

        if (!AnimPayload(bytes, payload, error)) {
            return false;
        }

        uint32_t base = AppendAligned(m2, payload);
        if (!ChangeTracks(m2, { s, base, payload.size(), false }, error)) {
            error += " (" + SequenceName(id, variation) + ")";
            return false;
        }
    }

    return true;
}

bool EmbedAliases(std::vector<uint8_t>& m2, std::string& error) {
    Span sequences;
    if (!ReadSpan(m2, M2At::SEQUENCES, SEQUENCE_SIZE, "sequences", sequences, error)) {
        return false;
    }

    for (uint32_t s = 0; s < sequences.count; s++) {
        size_t at = sequences.At(s, SEQUENCE_SIZE);
        uint32_t flags = Get<uint32_t>(m2, at + SequenceAt::FLAGS);
        if (!(flags & SEQUENCE_ALIAS)) {
            continue;
        }

        uint32_t target = s;
        for (uint32_t steps = 0; Get<uint32_t>(m2, sequences.At(target, SEQUENCE_SIZE) + SequenceAt::FLAGS) & SEQUENCE_ALIAS; steps++) {
            target = Get<uint16_t>(m2, sequences.At(target, SEQUENCE_SIZE) + SequenceAt::ALIAS_NEXT);
            if (steps >= sequences.count || target >= sequences.count) {
                return Fail(error, "the alias chain of sequence " + std::to_string(s) + " loops or leaves the sequence table");
            }
        }

        if (Get<uint32_t>(m2, sequences.At(target, SEQUENCE_SIZE) + SequenceAt::FLAGS) & SEQUENCE_EMBEDDED) {
            Put<uint32_t>(m2, at + SequenceAt::FLAGS, flags | SEQUENCE_EMBEDDED);
        }
    }

    return true;
}

// Sequences that end up with one id: those that had it keep their chain; remapped ones are appended to it.
struct SequenceGroup {
    std::vector<uint32_t> originals;
    std::vector<uint32_t> remapped;
};

static void ChainRemapped(std::vector<uint8_t>& m2, const Span& sequences, const SequenceGroup& group, const std::vector<uint16_t>& sourceIds) {
    auto field = [&](uint32_t s, size_t offset) { return sequences.At(s, SEQUENCE_SIZE) + offset; };
    uint32_t tail = NO_INDEX;
    uint16_t variation = 0;

    if (!group.originals.empty()) {
        tail = group.originals.front();
        for (uint32_t steps = 0; Get<uint16_t>(m2, field(tail, SequenceAt::VARIATION_NEXT)) < sequences.count && steps < sequences.count; steps++) {
            tail = Get<uint16_t>(m2, field(tail, SequenceAt::VARIATION_NEXT));
        }

        for (uint32_t s : group.originals) {
            variation = std::max<uint16_t>(variation, Get<uint16_t>(m2, field(s, SequenceAt::VARIATION)) + 1);
        }
    }

    // Only the first source keeps its weights, and only when the id had nothing of its own: the rest stay
    // reachable by index but never play at random.
    uint16_t primary = group.originals.empty() ? sourceIds[group.remapped.front()] : NO_INDEX;

    for (uint32_t s : group.remapped) {
        if (tail != NO_INDEX) {
            Put<uint16_t>(m2, field(tail, SequenceAt::VARIATION_NEXT), static_cast<uint16_t>(s));
        }

        Put<uint16_t>(m2, field(s, SequenceAt::VARIATION), variation++);
        Put<uint16_t>(m2, field(s, SequenceAt::VARIATION_NEXT), NO_INDEX);
        if (sourceIds[s] != primary) {
            Put<uint32_t>(m2, field(s, SequenceAt::FREQUENCY), 0);
        }
        tail = s;
    }
}

bool RemapSequences(std::vector<uint8_t>& m2, const AnimationFallbacks* fallbacks, std::vector<RemappedSequence>& remapped, std::string& error) {
    Span sequences;
    if (!ReadSpan(m2, M2At::SEQUENCES, SEQUENCE_SIZE, "sequences", sequences, error)) {
        return false;
    }

    std::vector<uint16_t> ids(sequences.count);
    std::map<uint16_t, SequenceGroup> groups;
    bool any = false;

    for (uint32_t s = 0; s < sequences.count; s++) {
        ids[s] = Get<uint16_t>(m2, sequences.At(s, SEQUENCE_SIZE) + SequenceAt::ID);
        any |= ids[s] > LEGACY_MAX_ANIMATION;
    }

    if (!any) {
        return true;
    }

    for (uint32_t s = 0; s < sequences.count; s++) {
        if (ids[s] <= LEGACY_MAX_ANIMATION) {
            groups[ids[s]].originals.push_back(s);
            continue;
        }

        if (!fallbacks) {
            return Fail(error, "animation " + std::to_string(ids[s]) + " needs AnimationData fallbacks, and none were read for this build");
        }

        uint16_t id = static_cast<uint16_t>(fallbacks->Resolve(ids[s]));
        groups[id].remapped.push_back(s);
        remapped.push_back({ static_cast<uint16_t>(s), ids[s], Get<uint16_t>(m2, sequences.At(s, SEQUENCE_SIZE) + SequenceAt::VARIATION), id });
        Put<uint16_t>(m2, sequences.At(s, SEQUENCE_SIZE) + SequenceAt::ID, id);
    }

    // A direct table: with count = highest id + 1 the client's id % count probe lands on the id itself.
    uint16_t highest = groups.rbegin()->first;
    std::vector<uint8_t> lookup((highest + 1) * 2, 0xFF);

    for (const auto& group : groups) {
        if (!group.second.remapped.empty()) {
            ChainRemapped(m2, sequences, group.second, ids);
        }

        uint16_t head = static_cast<uint16_t>(group.second.originals.empty() ? group.second.remapped.front() : group.second.originals.front());
        Put<uint16_t>(lookup, group.first * 2, head);
    }

    uint32_t offset = AppendAligned(m2, lookup);
    Put<uint32_t>(m2, M2At::SEQUENCE_LOOKUP, highest + 1u);
    Put<uint32_t>(m2, M2At::SEQUENCE_LOOKUP + 4, offset);
    return true;
}

}
