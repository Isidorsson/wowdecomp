add_executable(WowdecompTests
    TestMain.cpp
    M2LimitsTests.cpp
)

target_include_directories(WowdecompTests PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME wowdecomp-tests COMMAND WowdecompTests)
