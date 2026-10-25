# Applies PATCH to SOURCE_DIR unless it is already applied, so re-running the patch step is safe.
# cmake -DSOURCE_DIR=<dir> -DPATCH=<file> -P ApplyPatchOnce.cmake
find_package(Git REQUIRED)

# The fetched source sits inside this repo's build tree; keep git apply from treating it as part of the repo.
get_filename_component(PARENT_DIR "${SOURCE_DIR}" DIRECTORY)
set(ENV{GIT_CEILING_DIRECTORIES} "${PARENT_DIR}")

# CascLib is LF and so is the patch; a global autocrlf would rewrite the patched files as CRLF.
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -c core.autocrlf=false apply --reverse --check "${PATCH}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE ALREADY_APPLIED
    OUTPUT_QUIET ERROR_QUIET
)
if (ALREADY_APPLIED EQUAL 0)
    return()
endif ()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" -c core.autocrlf=false apply --whitespace=nowarn "${PATCH}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE APPLY_RESULT
)
if (NOT APPLY_RESULT EQUAL 0)
    message(FATAL_ERROR "cannot apply ${PATCH} to ${SOURCE_DIR}")
endif ()
