#include "Engine/Source/BLPFile/BlpPixels.h"
#include <cstring>
#include "Engine/Source/Gx/Blit.h"

// BLP2 preferred formats (PIXEL_FORMAT) of the DXT encodings.
constexpr uint8_t PREFERRED_DXT3 = 1;
constexpr uint8_t PREFERRED_DXT5 = 7;

static uint8_t AlphaAt(const uint8_t* alpha, uint8_t alphaSize, uint32_t texel) {
    switch (alphaSize) {
        case ALPHA_1:
            return (alpha[texel >> 3] >> (texel & 7)) & 1 ? 0xFF : 0;

        case ALPHA_4:
            return static_cast<uint8_t>(((alpha[texel >> 1] >> ((texel & 1) * 4)) & 0xF) * 0x11);

        case ALPHA_8:
            return alpha[texel];

        default:
            return 0xFF;
    }
}

static void DecodePalettized(const BLPHeader& header, const uint8_t* mip, uint32_t texels, std::vector<C4Pixel>& pixels) {
    const uint8_t* alpha = mip + texels;
    for (uint32_t texel = 0; texel < texels; texel++) {
        const BlpPalPixel& color = header.m_extended.m_palette[mip[texel]];
        pixels[texel] = { color.m_b, color.m_g, color.m_r, AlphaAt(alpha, header.m_alphaSize, texel) };
    }
}

static BlitFormat DxtFormat(const BLPHeader& header) {
    if (header.m_preferredFormat == PREFERRED_DXT5) {
        return BlitFormat_Dxt5;
    }
    if (header.m_preferredFormat == PREFERRED_DXT3 || header.m_alphaSize > ALPHA_1) {
        return BlitFormat_Dxt3;
    }
    return BlitFormat_Dxt1;
}

bool DecodeBlpMip(const BLPHeader& header, const uint8_t* mip, uint32_t width, uint32_t height, std::vector<C4Pixel>& pixels) {
    uint32_t texels = width * height;
    pixels.assign(texels, C4Pixel {});

    switch (header.m_colorEncoding) {
        case COLOR_PAL:
            DecodePalettized(header, mip, texels, pixels);
            return true;

        case COLOR_DXT: {
            BlitFormat format = DxtFormat(header);
            C2iVector size = { static_cast<int32_t>(width), static_cast<int32_t>(height) };
            return Blit(size, BlitAlpha_0, mip, CalcRowStride(format, width, height), format, pixels.data(), 4 * width, BlitFormat_Argb8888) != 0;
        }

        case COLOR_RAW:
            std::memcpy(pixels.data(), mip, texels * sizeof(C4Pixel));
            return true;

        default:
            return false;
    }
}

MipFit FitMipToRect(uint32_t width, uint32_t height, uint32_t rectWidth, uint32_t rectHeight) {
    if (width < rectWidth || height < rectHeight) {
        return { true, 0 };
    }

    uint32_t mip = 0;
    while (width > rectWidth) {
        width >>= 1;
        height >>= 1;
        mip++;
    }
    return width == rectWidth && height == rectHeight ? MipFit { false, mip } : MipFit { true, 0 };
}

// The source texels [first, last) a target texel covers along one axis; at least one.
static void Span(uint32_t target, uint32_t size, uint32_t targetSize, uint32_t& first, uint32_t& last) {
    first = target * size / targetSize;
    last = (target + 1) * size / targetSize;
    last = last > first ? last : first + 1;
}

static C4Pixel Average(const std::vector<C4Pixel>& source, uint32_t width, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1) {
    uint32_t sum[4] = {};
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            const C4Pixel& texel = source[y * width + x];
            sum[0] += texel.m_b;
            sum[1] += texel.m_g;
            sum[2] += texel.m_r;
            sum[3] += texel.m_a;
        }
    }
    uint32_t count = (x1 - x0) * (y1 - y0);
    return { static_cast<uint8_t>(sum[0] / count), static_cast<uint8_t>(sum[1] / count), static_cast<uint8_t>(sum[2] / count), static_cast<uint8_t>(sum[3] / count) };
}

std::vector<C4Pixel> ResamplePixels(const std::vector<C4Pixel>& source, uint32_t width, uint32_t height, uint32_t targetWidth, uint32_t targetHeight) {
    std::vector<C4Pixel> target(targetWidth * targetHeight);
    for (uint32_t ty = 0; ty < targetHeight; ty++) {
        uint32_t y0, y1;
        Span(ty, height, targetHeight, y0, y1);
        for (uint32_t tx = 0; tx < targetWidth; tx++) {
            uint32_t x0, x1;
            Span(tx, width, targetWidth, x0, x1);
            target[ty * targetWidth + tx] = Average(source, width, x0, x1, y0, y1);
        }
    }
    return target;
}
