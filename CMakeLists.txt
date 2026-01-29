cmake_minimum_required(VERSION 3.22)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
project(er-hwinfo VERSION ${proj_ver} DESCRIPTION "This project enables querying the GPIO pins for Effective Range hardware on Raspberry Pi devices." LANGUAGES CXX)

# Pull in definitions from
# Effective Range CMake module like ER_DEPS, ER_PACK, etc.
include(ERBuild NO_POLICY_SCOPE)




ER_DEPS()

# Lookup dependencies and make them available to CMake
find_package(fmt REQUIRED)
find_package(Catch2  REQUIRED)
find_package(RapidJSON REQUIRED)

add_library(lib-er-hwinfo INTERFACE)
add_library(er-hwinfo::lib ALIAS lib-er-hwinfo)
target_include_directories(lib-er-hwinfo INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${RapidJSON_INCLUDE_DIRS}
)
target_compile_definitions(lib-er-hwinfo INTERFACE FMT_HEADER_ONLY)
target_compile_features(lib-er-hwinfo INTERFACE cxx_std_20)
target_link_libraries(lib-er-hwinfo INTERFACE fmt::fmt)

# This is a thin wrapper on top of vanilla cmake add_executable
# it just marks the artifacts for installation automatically
# so the binaries will be packaged without any other user interaction
add_executable(er-hwinfo src/main.cpp)
target_link_libraries(er-hwinfo PRIVATE lib-er-hwinfo)

install(FILES resources/hwdb.json DESTINATION /etc/er-hwinfo COMPONENT db)
install(FILES resources/hwdb-schema.json DESTINATION /etc/er-hwinfo COMPONENT db)
install(TARGETS er-hwinfo RUNTIME DESTINATION bin COMPONENT db)

# Install headers
install(DIRECTORY include/ DESTINATION include COMPONENT dev)

# CMake package configuration
include(CMakePackageConfigHelpers)
# GNUInstallDirs already included by ERBuild

# Install the library target and export it
install(TARGETS lib-er-hwinfo
    EXPORT er-hwinfoTargets
    INCLUDES DESTINATION include COMPONENT dev
)

# Generate and install the export file
install(EXPORT er-hwinfoTargets
    FILE er-hwinfoTargets.cmake
    NAMESPACE er-hwinfo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/er-hwinfo
    COMPONENT dev
)

# Generate the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/er-hwinfoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/er-hwinfoConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/er-hwinfo
)

# Generate the version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/er-hwinfoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the config and version files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/er-hwinfoConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/er-hwinfoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/er-hwinfo
    COMPONENT dev
)

# cross platform wrapper, to only enable running tests when the host arch == target arch
ER_ENABLE_TEST()

add_subdirectory(test)
# this function performs packaging into a debian package, all metadata is
# extracted from the project description and the deps.json file
set(CPACK_DEB_COMPONENT_INSTALL ON)
# Component-specific dependencies (CPack uses uppercase component names)
set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "libfmt-dev, rapidjson-dev")
ER_PACK()