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
        ConvertedCacheTests.cpp
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
        NpcDisplayTests.cpp
        BodyMatchTests.cpp
        BakeWarpTests.cpp
    )
    target_link_libraries(WowdecompTests PRIVATE RetailCore)
    target_sources(WowdecompTests PRIVATE
        WdbcTests.cpp
        ServerDisplaysTests.cpp
        ${CMAKE_SOURCE_DIR}/Tools/RetailServerDisplays/Wdbc.cpp
        ${CMAKE_SOURCE_DIR}/Tools/RetailServerDisplays/ServerDisplays.cpp
    )
endif ()

if (TARGET RetailAssets)
    target_sources(WowdecompTests PRIVATE CascSourceTests.cpp NpcBakeSourceTests.cpp)
    target_link_libraries(WowdecompTests PRIVATE RetailAssets)
endif ()

if (TARGET casc_static)
    target_sources(WowdecompTests PRIVATE CascRangeFetchTests.cpp)
    target_link_libraries(WowdecompTests PRIVATE casc_static ws2_32)
endif ()

add_test(NAME wowdecomp-tests COMMAND WowdecompTests)
