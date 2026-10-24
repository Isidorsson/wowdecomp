file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(DB STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(DB
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# Retail displays served after the stock creature rows (RetailCreatureRecords.cpp).
if (TARGET RetailAssets)
    target_link_libraries(DB PRIVATE RetailAssets)
endif ()
