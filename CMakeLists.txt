# OddSockets C SDK
cmake_minimum_required(VERSION 3.10)
project(OddSockets VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" OFF)
option(ENABLE_SSL "Enable SSL/TLS support" ON)
option(ENABLE_STATIC_ALLOCATION "Enable static memory allocation" OFF)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Find libcurl
find_package(CURL REQUIRED)

# Find pthread
find_package(Threads REQUIRED)

# Find OpenSSL (optional)
if(ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    add_definitions(-DODDSOCKETS_ENABLE_SSL)
endif()

# Find libwebsockets
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# Source files
set(ODDSOCKETS_SOURCES
    src/oddsockets.c
    src/manager_discovery.c
    src/websocket_client.c
    src/json_parser.c
    src/http_client.c
)

set(ODDSOCKETS_HEADERS
    src/oddsockets.h
    src/manager_discovery.h
    src/websocket_client.h
    src/message_validator.h
    src/json_parser.h
    src/http_client.h
    src/message_size_validator.h
    src/enhanced_features.h
)

# Static allocation support
if(ENABLE_STATIC_ALLOCATION)
    add_definitions(-DODDSOCKETS_ENABLE_STATIC_ALLOCATION)
endif()

# Create library
add_library(oddsockets ${ODDSOCKETS_SOURCES})

# Link libraries
target_link_libraries(oddsockets
    ${CURL_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    Threads::Threads
)

if(ENABLE_SSL)
    target_link_libraries(oddsockets OpenSSL::SSL OpenSSL::Crypto)
endif()

# Set library properties
set_target_properties(oddsockets PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${ODDSOCKETS_HEADERS}"
)

# Compiler definitions
target_compile_definitions(oddsockets PRIVATE
    ODDSOCKETS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ODDSOCKETS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ODDSOCKETS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS oddsockets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/oddsockets
)

# Install pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/oddsockets.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/oddsockets.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/oddsockets.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# CPack configuration
set(CPACK_PACKAGE_NAME "oddsockets")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OddSockets C SDK for real-time messaging")
set(CPACK_PACKAGE_VENDOR "OddSockets")
set(CPACK_PACKAGE_CONTACT "support@oddsockets.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "OddSockets C SDK Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  SSL support: ${ENABLE_SSL}")
message(STATUS "  Static allocation: ${ENABLE_STATIC_ALLOCATION}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "")
