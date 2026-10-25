#ifndef ENGINE_SOURCE_RETAIL_BAKEBLP_H
#define ENGINE_SOURCE_RETAIL_BAKEBLP_H

#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// The BLP side of a warped NPC bake: retail bakes arrive as BLP2 DXT1 (some raw), the warp writes a raw BLP2 the
// client's TextureCreate reads like any other.

struct Texel {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;

    bool operator==(const Texel& other) const { return b == other.b && g == other.g && r == other.r && a == other.a; }
};

struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Texel> texels;   // Rows top to bottom.

    const Texel& At(uint32_t x, uint32_t y) const { return this->texels[size_t(y) * this->width + x]; }
};

// The first mip of a BLP2 of palettized, DXT1, DXT3, DXT5 or raw texels. False for JPEG and damaged files.
bool DecodeBakeBlp(const std::vector<uint8_t>& blp, Image& image, std::string& error);

// A BLP2 of raw BGRA texels with 8 alpha bits and every mip down to 1x1, each a box average of the one above.
// The image's sides must be powers of two.
std::vector<uint8_t> EncodeRawBlp(const Image& image);

}

#endif
