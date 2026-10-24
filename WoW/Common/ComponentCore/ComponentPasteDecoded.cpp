#include "WoW/Common/ComponentCore/ComponentPasteDecoded.h"
#include <algorithm>
#include <vector>
#include "Engine/Source/BLPFile/BlpPixels.h"

static uint32_t MipSize(uint32_t size, uint32_t mip) {
    return std::max<uint32_t>(size >> mip, 1);
}

// The section's mips: down to 1x1, as a palettized texture of the section's size pastes.
static uint32_t SectionLevels(const C2iVector& size) {
    uint32_t levels = 1;
    for (uint32_t side = static_cast<uint32_t>(std::max(size.x, size.y)); side > 1; side >>= 1) {
        levels++;
    }
    return levels;
}

static void BlendRow(const C4Pixel* source, uint32_t width, bool covers, uint8_t* dst) {
    for (uint32_t x = 0; x < width; x++, dst += 4) {
        const C4Pixel& texel = source[x];
        uint32_t a = covers ? 255 : texel.m_a;
        dst[3] = 0xFF;
        if (a == 255) {
            dst[2] = texel.m_r;
            dst[1] = texel.m_g;
            dst[0] = texel.m_b;
            continue;
        }
        dst[2] = static_cast<uint8_t>(((255 - a) * dst[2] + a * texel.m_r) >> 8);
        dst[1] = static_cast<uint8_t>(((255 - a) * dst[1] + a * texel.m_g) >> 8);
        dst[0] = static_cast<uint8_t>(((255 - a) * dst[0] + a * texel.m_b) >> 8);
    }
}

void PasteDecodedTexture(HTEXTURECACHE texture, const PasteTarget& target) {
    const CACHEENTRY* entry = texture;
    if (!entry || entry->m_loadFailed || !entry->m_data) {
        return;
    }

    const BLPHeader& header = *entry->m_data;
    MipFit fit = FitMipToRect(header.m_width, header.m_height, target.size.x, target.size.y);
    uint32_t mip = fit.scale ? 0 : fit.mip;
    uint32_t width = MipSize(header.m_width, mip);
    uint32_t height = MipSize(header.m_height, mip);
    const uint8_t* bits = static_cast<const uint8_t*>(TextureCacheGetMip(texture, mip));
    std::vector<C4Pixel> decoded;
    if (!bits || !DecodeBlpMip(header, bits, width, height, decoded)) {
        return;
    }

    bool covers = header.m_alphaSize == ALPHA_0;
    for (uint32_t level = 0, levels = SectionLevels(target.size); level < levels; level++) {
        uint32_t levelWidth = MipSize(target.size.x, level);
        uint32_t levelHeight = MipSize(target.size.y, level);
        uint32_t stride = target.stride >> level;
        std::vector<C4Pixel> pixels = ResamplePixels(decoded, width, height, levelWidth, levelHeight);
        uint8_t* row = reinterpret_cast<uint8_t*>(target.mips->m_mip[level]) + stride * (target.pos.y >> level) + 4 * (target.pos.x >> level);
        for (uint32_t y = 0; y < levelHeight; y++, row += stride) {
            BlendRow(&pixels[y * levelWidth], levelWidth, covers, row);
        }
    }
}
