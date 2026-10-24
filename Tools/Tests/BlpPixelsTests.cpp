#include "Engine/Source/BLPFile/BlpPixels.h"
#include "Tools/Tests/Check.h"

// The character compositor pastes body textures by their pixels. Stock ones are palettized; retail's are DXT, which
// the original pastes as solid green (CCharacterComponent::PasteCrappyGreen).

// BLP2 preferred formats (PIXEL_FORMAT) of DXT encodings.
constexpr uint8_t DXT1 = 0;
constexpr uint8_t DXT5 = 7;

static BLPHeader Header(uint8_t encoding, uint8_t alphaSize, uint32_t width, uint32_t height, uint8_t preferred = 0) {
    BLPHeader header = {};
    header.m_magic = BLP_MAGIC_BLP2;
    header.m_formatVersion = 1;
    header.m_colorEncoding = encoding;
    header.m_alphaSize = alphaSize;
    header.m_preferredFormat = preferred;
    header.m_width = width;
    header.m_height = height;
    header.m_extended.m_palette[0] = { 10, 20, 30, 0 };
    header.m_extended.m_palette[1] = { 40, 50, 60, 0 };
    return header;
}

static std::vector<C4Pixel> Decoded(const BLPHeader& header, const std::vector<uint8_t>& mip, uint32_t width, uint32_t height) {
    std::vector<C4Pixel> pixels;
    CHECK(DecodeBlpMip(header, mip.data(), width, height, pixels));
    pixels.resize(width * height, C4Pixel { 1, 2, 3, 4 });
    return pixels;
}

static bool Is(const C4Pixel& pixel, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
    return pixel.m_b == b && pixel.m_g == g && pixel.m_r == r && pixel.m_a == a;
}

static bool White(const C4Pixel& pixel) {
    return pixel.m_b >= 248 && pixel.m_g >= 248 && pixel.m_r >= 248;
}

TEST(PalettizedTexelsTakeThePaletteAndAreOpaqueWithoutAlphaBits) {
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_PAL, ALPHA_0, 2, 1), { 1, 0 }, 2, 1);

    CHECK(Is(pixels[0], 40, 50, 60, 255));
    CHECK(Is(pixels[1], 10, 20, 30, 255));
}

TEST(EightBitAlphaFollowsTheIndices) {
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_PAL, ALPHA_8, 2, 1), { 0, 1, 0x80, 0xFF }, 2, 1);

    CHECK(Is(pixels[0], 10, 20, 30, 0x80));
    CHECK(Is(pixels[1], 40, 50, 60, 0xFF));
}

TEST(OneBitAlphaPacksEightTexelsAByteLowBitFirst) {
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_PAL, ALPHA_1, 8, 1), { 0, 0, 0, 0, 0, 0, 0, 0, 0x05 }, 8, 1);

    CHECK(pixels[0].m_a == 255 && pixels[1].m_a == 0 && pixels[2].m_a == 255 && pixels[7].m_a == 0);
}

TEST(FourBitAlphaPacksTwoTexelsAByteLowNibbleFirst) {
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_PAL, ALPHA_4, 2, 1), { 0, 0, 0xF3 }, 2, 1);

    CHECK(pixels[0].m_a == 0x33 && pixels[1].m_a == 0xFF);
}

// A DXT1 block: white and black, the first row's texels choosing black (index 1), the others white.
static const std::vector<uint8_t> DXT1_BLOCK = { 0xFF, 0xFF, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00 };

TEST(Dxt1BlocksDecodeTheirColours) {
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_DXT, ALPHA_0, 4, 4, DXT1), DXT1_BLOCK, 4, 4);

    CHECK(Is(pixels[0], 0, 0, 0, 255) && Is(pixels[3], 0, 0, 0, 255));
    CHECK(White(pixels[4]) && pixels[4].m_a == 255 && White(pixels[15]));
}

TEST(Dxt5AlphaUsesItsInterpolatedTable) {
    // Alpha 255 and 0, every texel choosing index 1 (0); then a white colour block.
    std::vector<uint8_t> block = { 0xFF, 0x00, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24, 0xFF, 0xFF, 0x00, 0x00, 0, 0, 0, 0 };
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_DXT, ALPHA_8, 4, 4, DXT5), block, 4, 4);

    CHECK(White(pixels[0]) && pixels[0].m_a == 0 && pixels[15].m_a == 0);
}

TEST(MipsSmallerThanABlockDecodeFromOne) {
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_DXT, ALPHA_0, 4, 4, DXT1), DXT1_BLOCK, 2, 2);

    CHECK(Is(pixels[0], 0, 0, 0, 255) && Is(pixels[1], 0, 0, 0, 255));
    CHECK(White(pixels[2]) && White(pixels[3]));
}

TEST(RawTexelsAreCopied) {
    std::vector<C4Pixel> pixels = Decoded(Header(COLOR_RAW, ALPHA_8, 1, 1), { 1, 2, 3, 4 }, 1, 1);

    CHECK(Is(pixels[0], 1, 2, 3, 4));
}

TEST(JpegTexturesAreRefused) {
    std::vector<C4Pixel> pixels;
    std::vector<uint8_t> mip(16, 0);

    CHECK(!DecodeBlpMip(Header(COLOR_JPEG, ALPHA_0, 2, 2), mip.data(), 2, 2, pixels));
}

TEST(ATextureOfTheSectionsSizeUsesItsFirstMip) {
    MipFit fit = FitMipToRect(256, 128, 256, 128);

    CHECK(!fit.scale && fit.mip == 0);
}

TEST(LargerTexturesOfTheSameShapeStepDownMips) {
    MipFit twice = FitMipToRect(512, 256, 256, 128);
    MipFit fourTimes = FitMipToRect(1024, 512, 256, 128);

    CHECK(!twice.scale && twice.mip == 1);
    CHECK(!fourTimes.scale && fourTimes.mip == 2);
}

TEST(SmallerOrOtherlyShapedTexturesAreScaled) {
    CHECK(FitMipToRect(128, 64, 256, 128).scale);
    // Narrower but as tall: the original stepped down mips forever looking for the section's width.
    CHECK(FitMipToRect(128, 256, 256, 128).scale);
    // Twice as wide but half as tall: the width's mip would read past the texture's rows.
    CHECK(FitMipToRect(512, 64, 256, 128).scale);
}

TEST(ResamplingDownAveragesTheTexelsItCovers) {
    std::vector<C4Pixel> source = { { 0, 0, 0, 255 }, { 200, 100, 50, 255 }, { 0, 0, 0, 255 }, { 200, 100, 50, 255 } };
    std::vector<C4Pixel> half = ResamplePixels(source, 2, 2, 1, 1);

    CHECK(half.size() == 1 && Is(half[0], 100, 50, 25, 255));
}

TEST(ResamplingUpTakesTheNearestTexel) {
    std::vector<C4Pixel> source = { { 1, 1, 1, 255 }, { 2, 2, 2, 255 } };
    std::vector<C4Pixel> twice = ResamplePixels(source, 2, 1, 4, 2);

    CHECK(twice.size() == 8 && Is(twice[0], 1, 1, 1, 255) && Is(twice[1], 1, 1, 1, 255) && Is(twice[2], 2, 2, 2, 255) && Is(twice[7], 2, 2, 2, 255));
}
