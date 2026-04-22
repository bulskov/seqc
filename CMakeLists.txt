cmake_minimum_required(VERSION 3.20)
project(seqc LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory(src)

include(CTest)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
