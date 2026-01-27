cmake_minimum_required(VERSION 3.22)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pull in definitions from
# Effective Range CMake module like ER_DEPS, ER_PACK, etc.
include(ERBuild NO_POLICY_SCOPE)

# Get version from git tag
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_RESULT
)

if(GIT_RESULT EQUAL 0 AND GIT_TAG)
    string(REGEX REPLACE "^v" "" proj_ver "${GIT_TAG}")
else()
    message(FATAL_ERROR "No git tag found. Please create a tag (e.g., git tag v0.1.0)")
endif()

project(er_example_cpp VERSION ${proj_ver} DESCRIPTION "This project enables querying the GPIO pins for Effective Range hardware on Raspberry Pi devices." LANGUAGES CXX)

ER_DEPS()

# Lookup dependencies and make them available to CMake
find_package(fmt REQUIRED)
find_package(Catch2  REQUIRED)
# This is a thin wrapper on top of vanilla cmake add_executable
# it just marks the artifacts for installation automatically
# so the binaries will be packaged without any other user interaction
ER_ADD_EXECUTABLE(er-hwinfo SOURCES src/main.cpp)


# cross platform wrapper, to only enable running tests when the host arch == target arch
ER_ENABLE_TEST()

add_subdirectory(test)
# this function performs packaging into a debian package, all metadata is 
# extracted from the project description and the deps.json file
ER_PACK()