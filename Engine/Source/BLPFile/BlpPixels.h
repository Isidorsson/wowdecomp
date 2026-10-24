#ifndef ENGINE_SOURCE_BLPFILE_BLPPIXELS_H
#define ENGINE_SOURCE_BLPFILE_BLPPIXELS_H

#include <cstdint>
#include <vector>
#include "Engine/Source/BLPFile/Types.h"

// Not in the original. The character compositor (CCharacterComponent::Paste) reads palettized textures only and
// paints anything else solid green, but retail body textures are DXT or raw. These let it read every encoding and fit
// any size to a section.

// One mip of a BLP, width by height texels, as the 32-bit pixels MipBits holds (C4Pixel). Palettized texels take the
// header's palette and their alpha plane (1, 4 or 8 bits, after the indices); DXT1, 3 and 5 decode as the engine's
// Blit does; raw texels are copied. A texture without alpha bits is opaque. False for JPEG and unknown encodings.
bool DecodeBlpMip(const BLPHeader& header, const uint8_t* mip, uint32_t width, uint32_t height, std::vector<C4Pixel>& pixels);

// How a texture fills a rect: the mip that has the rect's size, or scaling when none has (a texture smaller than the
// rect in either direction, or of another shape).
struct MipFit {
    bool scale;
    uint32_t mip;
};

MipFit FitMipToRect(uint32_t width, uint32_t height, uint32_t rectWidth, uint32_t rectHeight);

// Pixels at another size: each target texel averages the source texels it covers, or takes the nearest when the
// target is larger.
std::vector<C4Pixel> ResamplePixels(const std::vector<C4Pixel>& source, uint32_t width, uint32_t height, uint32_t targetWidth, uint32_t targetHeight);

#endif
