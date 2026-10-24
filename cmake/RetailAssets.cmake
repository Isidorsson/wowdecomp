# Pure parsing, routing and cache code. No CascLib, so its tests build and run offline.
add_library(RetailCore STATIC
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailConfig.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailPath.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/DecodedCache.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/Db2Table.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/AnimationFallbacks.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CreatureTables.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ModelSkins.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailDisplays.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailItemDisplays.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ItemSkins.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailModel.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailAnimation.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailRecords.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailShading.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailEffects.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailSecondUv.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailBoneWindows.cpp
)
target_include_directories(RetailCore PUBLIC ${CMAKE_SOURCE_DIR})

# CascLib 3.0, the latest stable release (MIT). Pinned by hash; WOWDECOMP_CASCLIB_SOURCE_DIR builds offline.
set(WOWDECOMP_CASCLIB_SOURCE_DIR "" CACHE PATH "Existing CascLib 3.0 source checkout")
include(FetchContent)
if (WOWDECOMP_CASCLIB_SOURCE_DIR)
    FetchContent_Declare(casclib SOURCE_DIR ${WOWDECOMP_CASCLIB_SOURCE_DIR})
else ()
    FetchContent_Declare(casclib
        URL https://codeload.github.com/ladislav-zezula/CascLib/zip/refs/tags/3.0
        URL_HASH SHA256=a2d72255918bcae23cec9715fd7de383c058b23130d60638ae77da24e1d98f35
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
endif ()

set(CASC_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(CASC_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(CASC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CASC_UNICODE OFF CACHE BOOL "" FORCE)
# CascLib declares CMake 3.2; CMake 4 refuses anything below 3.5 unless told otherwise.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(casclib)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# The CASC-backed source the client's SFile hooks call.
add_library(RetailAssets STATIC
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CascSource.cpp
)
target_link_libraries(RetailAssets PUBLIC RetailCore PRIVATE casc_static)
target_compile_definitions(RetailAssets PUBLIC WOWDECOMP_RETAIL_ASSETS=1)

add_executable(RetailAssetProbe ${CMAKE_SOURCE_DIR}/Tools/RetailAssetProbe/RetailAssetProbe.cpp)
target_link_libraries(RetailAssetProbe PRIVATE RetailAssets)
