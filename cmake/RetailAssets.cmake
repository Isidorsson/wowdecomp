# Pure parsing, routing and cache code. No CascLib, so its tests build and run offline.
add_library(RetailCore STATIC
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailConfig.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailPath.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/DecodedCache.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ConvertedCache.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CacheBudget.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/MemoryCache.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CompressedBlocks.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/Db2Table.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/Db2Read.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/AnimationFallbacks.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CreatureTables.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ModelSkins.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailDisplays.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailItemDisplays.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ItemSkins.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailModel.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailAnimation.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailSkeleton.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailRecords.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailShading.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailEffects.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailSecondUv.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailBoneWindows.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailSkinLayout.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/Decimate.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailViews.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/HelmFit.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailNpcDisplay.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailNpcLook.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BodyMatch.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BakeWarp.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BodyMatchCache.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/PrefetchSchedule.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BakeBlp.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailRaces.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailBodyLook.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailIndex.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailIndexSets.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailIndexSearch.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BrowserMarshal.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BrowserDetails.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ItemSparse.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/StatBudget.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ItemArmour.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ClassicStats.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BuildSql.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BrowserBuild.cpp
)
target_include_directories(RetailCore PUBLIC ${CMAKE_SOURCE_DIR})
# The Windows Compression API, for the converted cache.
target_link_libraries(RetailCore PUBLIC cabinet)

# Keys converted models by a hash of the converter's sources too, so an edit never serves an entry it would convert
# differently. Hashed on every build, since a restored file can be older than the header; the header is rewritten only
# when the hash changes, so an unchanged tree recompiles nothing.
option(WOWDECOMP_RETAIL_SOURCE_HASH "Invalidate converted retail models whenever the converter's sources change" ON)
if (WOWDECOMP_RETAIL_SOURCE_HASH)
    set(RETAIL_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated/retail)
    add_custom_target(RetailConverterHash
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}/Engine/Source/Retail -DOUTPUT=${RETAIL_GENERATED_DIR}/RetailConverterHash.h
            -P ${CMAKE_SOURCE_DIR}/cmake/RetailConverterHash.cmake
        BYPRODUCTS ${RETAIL_GENERATED_DIR}/RetailConverterHash.h
        VERBATIM
    )
    add_dependencies(RetailCore RetailConverterHash)
    target_include_directories(RetailCore PRIVATE ${RETAIL_GENERATED_DIR})
    target_compile_definitions(RetailCore PRIVATE WOWDECOMP_RETAIL_SOURCE_HASH=1)
endif ()

# CascLib 3.0, the latest stable release (MIT). Pinned by hash; WOWDECOMP_CASCLIB_SOURCE_DIR builds offline
# and must already carry cmake/casclib-range-fetch.patch, which makes online storages fetch one archived
# file's bytes with an HTTP range instead of its whole 256 MB CDN archive.
set(WOWDECOMP_CASCLIB_SOURCE_DIR "" CACHE PATH "Existing CascLib 3.0 source checkout")
include(FetchContent)
if (WOWDECOMP_CASCLIB_SOURCE_DIR)
    FetchContent_Declare(casclib SOURCE_DIR ${WOWDECOMP_CASCLIB_SOURCE_DIR})
else ()
    FetchContent_Declare(casclib
        URL https://codeload.github.com/ladislav-zezula/CascLib/zip/refs/tags/3.0
        URL_HASH SHA256=a2d72255918bcae23cec9715fd7de383c058b23130d60638ae77da24e1d98f35
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        PATCH_COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${FETCHCONTENT_BASE_DIR}/casclib-src
            -DPATCH=${CMAKE_SOURCE_DIR}/cmake/casclib-range-fetch.patch
            -P ${CMAKE_SOURCE_DIR}/cmake/ApplyPatchOnce.cmake
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
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CascSourceNpc.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CascSourceBrowser.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CascSourceStorage.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CascSourcePrefetch.cpp
)
target_link_libraries(RetailAssets PUBLIC RetailCore PRIVATE casc_static)
target_compile_definitions(RetailAssets PUBLIC WOWDECOMP_RETAIL_ASSETS=1)

add_executable(RetailAssetProbe ${CMAKE_SOURCE_DIR}/Tools/RetailAssetProbe/RetailAssetProbe.cpp)
target_link_libraries(RetailAssetProbe PRIVATE RetailAssets)

add_executable(RetailServerDisplays
    ${CMAKE_SOURCE_DIR}/Tools/RetailServerDisplays/RetailServerDisplays.cpp
    ${CMAKE_SOURCE_DIR}/Tools/RetailServerDisplays/ServerDisplays.cpp
    ${CMAKE_SOURCE_DIR}/Tools/RetailServerDisplays/Wdbc.cpp
)
target_link_libraries(RetailServerDisplays PRIVATE RetailAssets)
