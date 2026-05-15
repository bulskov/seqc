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
