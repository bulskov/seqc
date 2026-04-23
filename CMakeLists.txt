cmake_minimum_required(VERSION 3.20)
project(seqc LANGUAGES C)

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
