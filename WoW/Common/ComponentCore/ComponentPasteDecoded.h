#ifndef WOW_COMMON_COMPONENTCORE_COMPONENTPASTEDECODED_H
#define WOW_COMMON_COMPONENTCORE_COMPONENTPASTEDECODED_H

#include <cstdint>
#include "Engine/Source/BLPFile/Types.h"
#include "Engine/Source/Tempest/C2iVector.h"
#include "WoW/Common/TextureCache.h"

// Not in the original. A section of the character's composed texture: its mips, the row stride of the first
// (each mip's halves), and the section's place and size in the first.
struct PasteTarget {
    MipBits* mips;
    uint32_t stride;
    C2iVector pos;
    C2iVector size;
};

// Pastes texture over the section at any encoding and size: the original reads palettized textures only (else
// PasteCrappyGreen) at the section's size or half of it. Decodes the mip nearest the section's size, resamples it to
// each of the section's mips and blends it by its alpha, as PasteTransparent8Bit does; without alpha bits it covers.
void PasteDecodedTexture(HTEXTURECACHE texture, const PasteTarget& target);

#endif
