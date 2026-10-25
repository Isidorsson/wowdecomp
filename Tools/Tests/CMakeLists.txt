add_executable(WowdecompTests
    TestMain.cpp
    M2LimitsTests.cpp
    BlpPixelsTests.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/BLPFile/BlpPixels.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Gx/Blit.cpp
)

target_include_directories(WowdecompTests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(WowdecompTests PRIVATE Tempest)

if (TARGET RetailCore)
    target_sources(WowdecompTests PRIVATE
        RetailConfigTests.cpp
        RetailPathTests.cpp
        DecodedCacheTests.cpp
        Db2TableTests.cpp
        AnimationFallbacksTests.cpp
        RetailModelTests.cpp
        RetailAnimationTests.cpp
        RetailSkeletonTests.cpp
        RetailParticlesTests.cpp
        RetailShadingTests.cpp
        RetailSecondUvTests.cpp
        RetailOverlayTests.cpp
        RetailBoneWindowsTests.cpp
        RetailSkinLayoutTests.cpp
        DecimateTests.cpp
        ModelSkinsTests.cpp
        ItemSkinsTests.cpp
        RetailDisplaysTests.cpp
        RetailItemDisplaysTests.cpp
        HelmFitTests.cpp
    )
    target_link_libraries(WowdecompTests PRIVATE RetailCore)
endif ()

add_test(NAME wowdecomp-tests COMMAND WowdecompTests)
