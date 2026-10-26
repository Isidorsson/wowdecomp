#include <string>
#include "WoW/Common/ComponentCore/BeltBuckle.h"
#include "Tools/Tests/Check.h"

// A belt display naming a model hangs it on the body's belt buckle attachment (53), as retail does; every
// installed body has one. A served row names its files "retail-<fdid>", which CascSource reads in any folder.

TEST(ABeltWithAModelAndSkinNamesBothInTheWaistFolder) {
    BucklePaths paths;

    CHECK(BuckleItemPaths("retail-6758394.m2", "retail-868809", paths));
    CHECK(paths.model == "Item\\ObjectComponents\\Waist\\retail-6758394.m2");
    CHECK(paths.texture == "Item\\ObjectComponents\\Waist\\retail-868809.blp");
}

// Stock 3.3.5 belts name no model, so they keep drawing as textures only.
TEST(ABeltWithoutAModelHasNoBuckle) {
    BucklePaths paths;

    CHECK(!BuckleItemPaths("", "retail-868809", paths));
    CHECK(!BuckleItemPaths(nullptr, nullptr, paths));
}

// AddLink drops a model whose skin does not load, so a skinless buckle is not attempted.
TEST(ABeltModelWithoutASkinHasNoBuckle) {
    BucklePaths paths;

    CHECK(!BuckleItemPaths("retail-6758394.m2", "", paths));
}

TEST(TheBuckleHangsOnAttachment53) {
    CHECK(ATTACH_BELT_BUCKLE == 53);
}
