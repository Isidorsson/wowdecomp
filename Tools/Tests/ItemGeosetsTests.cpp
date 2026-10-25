#include <vector>
#include "WoW/Common/ComponentCore/ItemGeosets.h"
#include "Tools/Tests/Check.h"

// An item names a mesh of a geoset group (boots value v: mesh 501 + v). Retail item displays name meshes past the
// installed bodies' (retail boots 506 to 510); a mesh the model lacks draws as the group's default x01, the mesh the
// model draws when no item names one (the bare shin for boots).

// The installed HumanMale.m2 (HD port): boots 501 to 505, kneepads 902/903 (no 901), capes 1502 to 1506.
static const std::vector<uint32_t> HUMAN_MALE = { 0, 5, 401, 402, 403, 404, 501, 502, 503, 504, 505, 902, 903, 1502, 1503, 1504, 1505, 1506 };

TEST(ItemGeosetsKeepAMeshTheModelHas) {
    CHECK(ChooseItemGeoset(505, HUMAN_MALE) == 505);
    CHECK(ChooseItemGeoset(402, HUMAN_MALE) == 402);
    CHECK(ChooseItemGeoset(1506, HUMAN_MALE) == 1506);
}

TEST(ItemGeosetsDrawTheGroupDefaultForAMeshTheModelLacks) {
    // Retail boots 506 to 510 are tight shins (closest to 501 on retail HumanMale_HD).
    CHECK(ChooseItemGeoset(506, HUMAN_MALE) == 501);
    CHECK(ChooseItemGeoset(510, HUMAN_MALE) == 501);
    CHECK(ChooseItemGeoset(406, HUMAN_MALE) == 401);
}

TEST(ItemGeosetsWithoutAGroupDefaultDrawNothingForAMeshTheModelLacks) {
    // No 901 or 1501 in the model: the default is no mesh, as without the item.
    CHECK(ChooseItemGeoset(905, HUMAN_MALE) == 901);
    CHECK(ChooseItemGeoset(1507, HUMAN_MALE) == 1501);
}

TEST(ItemGeosetsOnARetailBodyKeepItsNewerMeshes) {
    // Retail bodies (slice b) carry boots 501 to 510.
    std::vector<uint32_t> retail = { 0, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510 };
    CHECK(ChooseItemGeoset(506, retail) == 506);
    CHECK(ChooseItemGeoset(510, retail) == 510);
}
