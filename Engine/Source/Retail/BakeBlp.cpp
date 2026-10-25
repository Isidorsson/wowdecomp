#include "Engine/Source/Retail/BakeBlp.h"
#include <algorithm>
#include <cstring>

namespace Retail {

// BLP2 header (BLPFile/Types.h BLPHeader): 20 bytes, 16 mip offsets, 16 mip sizes, then the 1024-byte palette.
constexpr uint32_t BLP2_MAGIC = 0x32504C42;
constexpr size_t COLOR_ENCODING = 0x08;
constexpr size_t ALPHA_SIZE = 0x09;
constexpr size_t PREFERRED_FORMAT = 0x0A;
constexpr size_t HAS_MIPS = 0x0B;
constexpr size_t WIDTH = 0x0C;
constexpr size_t HEIGHT = 0x10;
constexpr size_t MIP_OFFSETS = 0x14;
constexpr size_t MIP_SIZES = 0x54;
constexpr size_t PALETTE = 0x94;
constexpr size_t HEADER_SIZE = PALETTE + 1024;
constexpr size_t MIP_SLOTS = 16;

constexpr uint8_t ENCODING_PALETTE = 1;
constexpr uint8_t ENCODING_DXT = 2;
constexpr uint8_t ENCODING_RAW = 3;
constexpr uint8_t FORMAT_DXT1 = 0;
constexpr uint8_t FORMAT_DXT3 = 1;
constexpr uint8_t FORMAT_ARGB8888 = 2;
constexpr uint8_t FORMAT_DXT5 = 7;

static uint32_t U32(const std::vector<uint8_t>& data, size_t at) {
    uint32_t value;
    std::memcpy(&value, data.data() + at, 4);
    return value;
}

static void PutU32(std::vector<uint8_t>& data, size_t at, uint32_t value) {
    std::memcpy(data.data() + at, &value, 4);
}

static bool Fail(std::string& error, const std::string& message) {
    error = "bake: " + message;
    return false;
}

static Texel Expand565(uint16_t c) {
    uint8_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return { static_cast<uint8_t>((b << 3) | (b >> 2)), static_cast<uint8_t>((g << 2) | (g >> 4)), static_cast<uint8_t>((r << 3) | (r >> 2)), 255 };
}

static Texel Blend(const Texel& a, const Texel& b, int wa, int wb) {
    int sum = wa + wb;
    return { static_cast<uint8_t>((a.b * wa + b.b * wb) / sum), static_cast<uint8_t>((a.g * wa + b.g * wb) / sum),
        static_cast<uint8_t>((a.r * wa + b.r * wb) / sum), 255 };
}

// The four colours of a DXT colour block; DXT1's three-colour mode makes the fourth transparent black.
static void BlockColors(const uint8_t* block, bool dxt1, Texel colors[4]) {
    uint16_t c0 = static_cast<uint16_t>(block[0] | (block[1] << 8));
    uint16_t c1 = static_cast<uint16_t>(block[2] | (block[3] << 8));
    colors[0] = Expand565(c0);
    colors[1] = Expand565(c1);
    if (c0 > c1 || !dxt1) {
        colors[2] = Blend(colors[0], colors[1], 2, 1);
        colors[3] = Blend(colors[0], colors[1], 1, 2);
    } else {
        colors[2] = Blend(colors[0], colors[1], 1, 1);
        colors[3] = { 0, 0, 0, 0 };
    }
}

// The 16 alphas of a DXT3 (explicit 4-bit) or DXT5 (interpolated) alpha block.
static void BlockAlphas(const uint8_t* block, uint8_t format, uint8_t alphas[16]) {
    if (format == FORMAT_DXT3) {
        for (int i = 0; i < 16; i++) {
            uint8_t nibble = (block[i / 2] >> ((i & 1) * 4)) & 15;
            alphas[i] = static_cast<uint8_t>(nibble * 17);
        }
        return;
    }
    uint8_t table[8] = { block[0], block[1] };
    for (int i = 2; i < 8; i++) {
        bool eight = block[0] > block[1];
        int steps = eight ? 7 : 5;
        int k = i - 1;
        table[i] = eight || i < 6 ? static_cast<uint8_t>(((steps - k) * block[0] + k * block[1]) / steps) : (i == 6 ? 0 : 255);
    }
    uint64_t bits = 0;
    for (int i = 0; i < 6; i++) {
        bits |= uint64_t(block[2 + i]) << (8 * i);
    }
    for (int i = 0; i < 16; i++) {
        alphas[i] = table[(bits >> (3 * i)) & 7];
    }
}

static void DecodeDxt(const uint8_t* data, uint8_t format, Image& image) {
    size_t blockSize = format == FORMAT_DXT1 ? 8 : 16;
    uint32_t blocksX = (image.width + 3) / 4, blocksY = (image.height + 3) / 4;
    for (uint32_t by = 0; by < blocksY; by++) {
        for (uint32_t bx = 0; bx < blocksX; bx++) {
            const uint8_t* block = data + (size_t(by) * blocksX + bx) * blockSize;
            Texel colors[4];
            uint8_t alphas[16];
            BlockColors(block + blockSize - 8, format == FORMAT_DXT1, colors);
            if (format != FORMAT_DXT1) {
                BlockAlphas(block, format, alphas);
            }
            uint32_t indices;
            std::memcpy(&indices, block + blockSize - 4, 4);
            for (uint32_t i = 0; i < 16; i++) {
                uint32_t x = bx * 4 + i % 4, y = by * 4 + i / 4;
                if (x >= image.width || y >= image.height) {
                    continue;
                }
                Texel texel = colors[(indices >> (2 * i)) & 3];
                if (format != FORMAT_DXT1) {
                    texel.a = alphas[i];
                }
                image.texels[size_t(y) * image.width + x] = texel;
            }
        }
    }
}

// One index byte a texel, then the alpha plane: 0, 1, 4 or 8 bits a texel, least significant first.
static bool DecodePalettized(const std::vector<uint8_t>& blp, size_t offset, size_t size, Image& image, std::string& error) {
    uint8_t alphaBits = blp[ALPHA_SIZE];
    size_t count = image.texels.size();
    if ((alphaBits != 0 && alphaBits != 1 && alphaBits != 4 && alphaBits != 8) || size < count + (count * alphaBits + 7) / 8) {
        return Fail(error, "a damaged palettized mip");
    }
    const uint8_t* indices = blp.data() + offset;
    const uint8_t* alphas = indices + count;
    for (size_t i = 0; i < count; i++) {
        const uint8_t* entry = blp.data() + PALETTE + indices[i] * 4;
        uint8_t alpha = 255;
        if (alphaBits) {
            size_t bit = i * alphaBits;
            uint32_t value = (alphas[bit / 8] >> (bit % 8)) & ((1u << alphaBits) - 1);
            alpha = static_cast<uint8_t>(value * 255 / ((1u << alphaBits) - 1));
        }
        image.texels[i] = { entry[0], entry[1], entry[2], alpha };
    }
    return true;
}

bool DecodeBakeBlp(const std::vector<uint8_t>& blp, Image& image, std::string& error) {
    if (blp.size() < HEADER_SIZE || U32(blp, 0) != BLP2_MAGIC) {
        return Fail(error, "not a BLP2 file");
    }
    uint8_t encoding = blp[COLOR_ENCODING];
    uint8_t format = blp[PREFERRED_FORMAT];
    image.width = U32(blp, WIDTH);
    image.height = U32(blp, HEIGHT);
    size_t offset = U32(blp, MIP_OFFSETS);
    size_t size = U32(blp, MIP_SIZES);
    if (!image.width || !image.height || image.width > 4096 || image.height > 4096 || offset + size > blp.size() || offset + size < offset) {
        return Fail(error, "a damaged header");
    }

    image.texels.assign(size_t(image.width) * image.height, Texel{ 0, 0, 0, 255 });
    if (encoding == ENCODING_RAW && size >= image.texels.size() * 4) {
        std::memcpy(image.texels.data(), blp.data() + offset, image.texels.size() * 4);
        return true;
    }
    if (encoding == ENCODING_PALETTE) {
        return DecodePalettized(blp, offset, size, image, error);
    }
    if (encoding == ENCODING_DXT && (format == FORMAT_DXT1 || format == FORMAT_DXT3 || format == FORMAT_DXT5)) {
        size_t blocks = size_t((image.width + 3) / 4) * ((image.height + 3) / 4);
        if (size < blocks * (format == FORMAT_DXT1 ? 8 : 16)) {
            return Fail(error, "the first mip is truncated");
        }
        DecodeDxt(blp.data() + offset, format, image);
        return true;
    }
    return Fail(error, "encoding " + std::to_string(encoding) + " format " + std::to_string(format) + " is not read");
}

static Image HalfSize(const Image& image) {
    Image half;
    half.width = std::max(1u, image.width / 2);
    half.height = std::max(1u, image.height / 2);
    for (uint32_t y = 0; y < half.height; y++) {
        for (uint32_t x = 0; x < half.width; x++) {
            uint32_t sx = std::min(x * 2 + 1, image.width - 1), sy = std::min(y * 2 + 1, image.height - 1);
            const Texel* t[4] = { &image.At(x * 2, y * 2), &image.At(sx, y * 2), &image.At(x * 2, sy), &image.At(sx, sy) };
            half.texels.push_back({ static_cast<uint8_t>((t[0]->b + t[1]->b + t[2]->b + t[3]->b + 2) / 4),
                static_cast<uint8_t>((t[0]->g + t[1]->g + t[2]->g + t[3]->g + 2) / 4), static_cast<uint8_t>((t[0]->r + t[1]->r + t[2]->r + t[3]->r + 2) / 4),
                static_cast<uint8_t>((t[0]->a + t[1]->a + t[2]->a + t[3]->a + 2) / 4) });
        }
    }
    return half;
}

std::vector<uint8_t> EncodeRawBlp(const Image& image) {
    std::vector<uint8_t> blp(HEADER_SIZE, 0);
    PutU32(blp, 0, BLP2_MAGIC);
    PutU32(blp, 4, 1);
    blp[COLOR_ENCODING] = ENCODING_RAW;
    blp[ALPHA_SIZE] = 8;
    blp[PREFERRED_FORMAT] = FORMAT_ARGB8888;
    blp[HAS_MIPS] = 1;
    PutU32(blp, WIDTH, image.width);
    PutU32(blp, HEIGHT, image.height);

    Image mip = image;
    for (size_t level = 0; level < MIP_SLOTS; level++) {
        size_t bytes = mip.texels.size() * 4;
        PutU32(blp, MIP_OFFSETS + level * 4, static_cast<uint32_t>(blp.size()));
        PutU32(blp, MIP_SIZES + level * 4, static_cast<uint32_t>(bytes));
        const uint8_t* data = reinterpret_cast<const uint8_t*>(mip.texels.data());
        blp.insert(blp.end(), data, data + bytes);
        if (mip.width == 1 && mip.height == 1) {
            break;
        }
        mip = HalfSize(mip);
    }
    return blp;
}

}
