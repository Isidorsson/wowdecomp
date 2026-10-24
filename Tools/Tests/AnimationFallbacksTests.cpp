#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/Wdc5Builder.h"

using Retail::AnimationFallbacks;
using Retail::Db2Table;

// An AnimationData table: noninline ids, physical column 0 is a 16-bit Fallback.
static Db2Table AnimationTable(std::vector<std::pair<uint32_t, uint16_t>> rows, uint32_t layout = Retail::ANIMATION_DATA_LAYOUT) {
    Wdc5Spec spec;
    spec.layoutHash = layout;
    spec.recordSize = 2;
    spec.columns = { { 16, 0, 16, 0, 0, { 0, 0, 0 } } };
    Wdc5Section section;

    for (const auto& row : rows) {
        section.ids.push_back(row.first);
        section.records.push_back(static_cast<uint8_t>(row.second));
        section.records.push_back(static_cast<uint8_t>(row.second >> 8));
    }

    spec.sections = { section };
    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error));
    return table;
}

static AnimationFallbacks Loaded(std::vector<std::pair<uint32_t, uint16_t>> rows) {
    AnimationFallbacks fallbacks;
    std::string error;
    CHECK(fallbacks.Load(AnimationTable(rows), error));
    return fallbacks;
}

TEST(LegacyAnimationsResolveToThemselves) {
    AnimationFallbacks fallbacks = Loaded({ { 12, 0 }, { 600, 12 } });

    CHECK(fallbacks.Resolve(0) == 0);
    CHECK(fallbacks.Resolve(12) == 12);
    CHECK(fallbacks.Resolve(505) == 505);
}

TEST(NewAnimationsFollowTheirFallbackChain) {
    AnimationFallbacks fallbacks = Loaded({ { 600, 550 }, { 550, 12 }, { 507, 229 } });

    CHECK(fallbacks.Size() == 3);
    CHECK(fallbacks.Resolve(600) == 12);
    CHECK(fallbacks.Resolve(550) == 12);
    CHECK(fallbacks.Resolve(507) == 229);
}

TEST(BrokenFallbackChainsGiveStand) {
    AnimationFallbacks fallbacks = Loaded({ { 700, 700 }, { 610, 611 }, { 611, 610 }, { 800, 900 } });

    CHECK(fallbacks.Resolve(700) == 0);
    CHECK(fallbacks.Resolve(610) == 0);
    CHECK(fallbacks.Resolve(800) == 0);
    CHECK(fallbacks.Resolve(12345) == 0);
}

TEST(OtherAnimationDataLayoutsAreRefused) {
    AnimationFallbacks fallbacks;
    std::string error;

    CHECK(!fallbacks.Load(AnimationTable({ { 600, 12 } }, 0x11111111), error));
    CHECK(!error.empty());
}
