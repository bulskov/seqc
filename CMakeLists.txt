cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SEQC_VERSION)
project(seqc VERSION ${SEQC_VERSION} LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SEQC_SANITIZERS
    "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(SEQC_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# --- Dependency: arena_allocator -----------------------------------------
# Fetched from source so it is built with the same compiler and flags
# (including sanitizers) as seqc.  Override SEQC_ARENA_GIT_TAG to pin a
# different version, or pass -DFETCHCONTENT_SOURCE_DIR_ARENA=/path to build
# against a local checkout with no network access.
include(FetchContent)

set(SEQC_ARENA_GIT_REPOSITORY "https://github.com/bulskov/arena_allocation.git"
    CACHE STRING "Clone URL for the arena_allocator dependency")
set(SEQC_ARENA_GIT_TAG "v1.1.1"
    CACHE STRING "Tag or commit of the arena_allocator dependency")

FetchContent_Declare(arena
    GIT_REPOSITORY ${SEQC_ARENA_GIT_REPOSITORY}
    GIT_TAG        ${SEQC_ARENA_GIT_TAG}
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(arena)

add_subdirectory(src)

include(CTest)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

set(CPACK_PACKAGE_NAME "seqc")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "seqc-${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "seqc-${PROJECT_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES
    "/.git/"
    "/build/"
)
include(CPack)
