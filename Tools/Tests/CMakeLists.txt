add_executable(WowdecompTests
    TestMain.cpp
    M2LimitsTests.cpp
)

target_include_directories(WowdecompTests PRIVATE ${CMAKE_SOURCE_DIR})

if (TARGET RetailCore)
    target_sources(WowdecompTests PRIVATE
        RetailConfigTests.cpp
        RetailPathTests.cpp
        DecodedCacheTests.cpp
        Db2TableTests.cpp
        AnimationFallbacksTests.cpp
        RetailModelTests.cpp
        RetailAnimationTests.cpp
        RetailParticlesTests.cpp
        RetailShadingTests.cpp
        RetailSecondUvTests.cpp
        RetailOverlayTests.cpp
        RetailBoneWindowsTests.cpp
        ModelSkinsTests.cpp
        ItemSkinsTests.cpp
        RetailDisplaysTests.cpp
    )
    target_link_libraries(WowdecompTests PRIVATE RetailCore)
endif ()

add_test(NAME wowdecomp-tests COMMAND WowdecompTests)
