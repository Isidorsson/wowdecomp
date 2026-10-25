#ifndef ENGINE_SOURCE_RETAIL_RETAILSKELETON_H
#define ENGINE_SOURCE_RETAIL_RETAILSKELETON_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailModel.h"

// Shared skeletons (SKID): a model naming a .skel file has no sequences, bones or attachments of its own; the
// skeleton has them, merged with the parent it names in turn (SKPD). Follows the wow-toolbox-editor downporter
// (downport/read/skeleton): the skeleton that owns a sequence's keys is authoritative for it, the parent supplies
// the rest and appends what only it defines.

namespace Retail {

// The chunks of one .skel file. Offsets inside each count from its own payload.
struct SkeletonFile {
    uint32_t parent = 0;               // SKPD; 0 is none.
    std::vector<uint8_t> sequences;    // SKS1: global loops, sequences, sequence lookup.
    std::vector<uint8_t> bones;        // SKB1: bones, key bone lookup.
    std::vector<uint8_t> attachments;  // SKA1: attachments, attachment lookup. Empty when the skeleton has none.
    std::vector<uint8_t> afid;         // The .anim file of each (animation id, variation).
};

bool ParseSkeleton(const std::vector<uint8_t>& file, SkeletonFile& skeleton, std::string& error);

// The skeleton fileDataId names and its parents, nearest first.
bool LoadSkeletonChain(uint32_t fileDataId, const FileReader& read, std::vector<SkeletonFile>& chain, std::string& error);

// Which skeleton of the chain, and which of its own sequences, supplies a merged sequence's keys.
struct SequenceSource {
    uint32_t level;
    uint32_t index;
};

// The sequence list of a chain merged into the nearest skeleton's: its own sequences keep their indices, the
// parent's that it lacks are appended with their links pointing into the merged list.
struct MergedSequences {
    std::vector<uint8_t> records;        // 0x40 bytes each.
    std::vector<uint16_t> lookup;        // The nearest skeleton's id lookup, completed with the appended ids.
    std::vector<SequenceSource> sources;
    std::vector<int32_t> parentIndex;    // The index in the parent's merged list of each sequence whose keys came from it; -1 otherwise.
    uint32_t own = 0;                    // Sequences the nearest skeleton defines.
    uint32_t parentCount = 0;            // Sequences of the parent's merged list.
};

bool MergeSequences(const std::vector<SkeletonFile>& chain, MergedSequences& merged, std::string& error);

// Puts the merged sequences, global loops, bones and attachments of chain into m2, every bone and attachment
// track with one run per merged sequence and its keys appended (in-chunk keys and AFSB/AFSA .anim keys alike).
bool MergeSkeleton(std::vector<uint8_t>& m2, const std::vector<SkeletonFile>& chain, const FileReader& read, ModelSkeleton& skeleton,
    std::vector<std::string>& warnings, std::string& error);

// Gives the model's events the PEDC timings of the sequences whose keys came from the parent skeleton. A PEDC
// that does not line up with the events or the parent's sequences changes nothing and is warned about.
bool ApplyParentEvents(std::vector<uint8_t>& m2, const std::vector<uint8_t>& pedc, const ModelSkeleton& skeleton, std::vector<std::string>& warnings, std::string& error);

}

#endif
