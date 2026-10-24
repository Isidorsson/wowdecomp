#ifndef ENGINE_SOURCE_RETAIL_RETAILANIMATION_H
#define ENGINE_SOURCE_RETAIL_RETAILANIMATION_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailModel.h"

namespace Retail {

// Appends the AFID .anim data of every sequence that needs a file and rebases that sequence's entry in every
// animated track, so the client never asks for an .anim file (flag 0x20 on all of them).
bool InlineAnimations(std::vector<uint8_t>& m2, const std::vector<uint8_t>& afid, const FileReader& read, std::vector<std::string>& warnings, std::string& error);

// Aliases whose chain ends at an embedded sequence are embedded too: the client follows the chain to play them.
bool EmbedAliases(std::vector<uint8_t>& m2, std::string& error);

// Renumbers ids 3.3.5 does not know to their AnimationData fallback and rebuilds variation chains and the
// sequence lookup table.
bool RemapSequences(std::vector<uint8_t>& m2, const AnimationFallbacks* fallbacks, std::vector<RemappedSequence>& remapped, std::string& error);

}

#endif
