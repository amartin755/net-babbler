###############################################################################
cmake_minimum_required (VERSION 3.13)

project (net-babbler VERSION 0.0.1
                 DESCRIPTION "A client/server communication simulator"
                 LANGUAGES CXX)
###############################################################################


# set output directories to "bin"
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

# compiler settings
###############################################################################
# C++11 is required
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# warning level
add_compile_options (-Wall -Wextra -Wpedantic -fstack-protector-all)

# check dependencies
###############################################################################
include (CheckSymbolExists)
check_symbol_exists (inet_pton "arpa/inet.h" HAVE_PTON)
check_symbol_exists (inet_ntop "arpa/inet.h" HAVE_NTOP)
check_symbol_exists (strerrordesc_np "string.h" HAVE_STRERRORDESC_NP)

# preprocessor definitions
###############################################################################
add_compile_definitions (APP_VERSION="${PROJECT_VERSION}")
if (HAVE_PTON)
    add_compile_definitions (HAVE_PTON)
endif ()
if (HAVE_NTOP)
    add_compile_definitions (HAVE_NTOP)
endif ()
if (HAVE_STRERRORDESC_NP)
    add_compile_definitions (HAVE_STRERRORDESC_NP)
endif ()
if (WIN32)
    add_compile_definitions (HAVE_WINDOWS)
endif ()

# generate build numbers
###############################################################################
execute_process (COMMAND git rev-parse --short HEAD    OUTPUT_VARIABLE GIT_COMMIT_SHORT ERROR_QUIET)

if (NOT "${GIT_COMMIT_SHORT}" STREQUAL "")
    execute_process (COMMAND git describe --exact-match --tags OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process (COMMAND git rev-parse --abbrev-ref HEAD OUTPUT_VARIABLE GIT_BRANCH)
    execute_process (COMMAND git rev-parse HEAD OUTPUT_VARIABLE GIT_COMMIT)
    execute_process(
        COMMAND git diff --quiet --exit-code
        RESULT_VARIABLE GIT_HAS_LOCAL_CHANGES
        )

    if (${GIT_HAS_LOCAL_CHANGES} EQUAL 1)
        set (GIT_HAS_LOCAL_CHANGES "+")
    endif ()

    string (STRIP "${GIT_COMMIT_SHORT}" GIT_COMMIT_SHORT)
    string (STRIP "${GIT_COMMIT}" GIT_COMMIT)
    string (STRIP "${GIT_TAG}" GIT_TAG)
    string (STRIP "${GIT_BRANCH}" GIT_BRANCH)

    set (GIT_COMMIT_SHORT ${GIT_COMMIT_SHORT}${GIT_HAS_LOCAL_CHANGES})
    set (GIT_COMMIT ${GIT_COMMIT}${GIT_HAS_LOCAL_CHANGES})

    add_compile_definitions (GIT_COMMIT_SHORT="${GIT_COMMIT_SHORT}")
    add_compile_definitions (GIT_TAG="${GIT_TAG}")
    add_compile_definitions (GIT_BRANCH="${GIT_BRANCH}")
    add_compile_definitions (GIT_COMMIT="${GIT_COMMIT}")
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M" UTC)
add_compile_definitions(BUILD_TIME="${BUILD_TIME}")
add_compile_definitions (BUILD_TYPE="${CMAKE_BUILD_TYPE}")

message (STATUS "Build infos:")
message (STATUS "  type       = ${CMAKE_BUILD_TYPE}")
message (STATUS "  timestamp  = ${BUILD_TIME}")
message (STATUS "  git tag    = ${GIT_TAG}")
message (STATUS "  git branch = ${GIT_BRANCH}")
message (STATUS "  git commit = ${GIT_COMMIT}")


# target dnscache (main library)
###############################################################################
add_executable (net-babbler)
set (SOURCE_DIR src)
set (SOURCES
    ${SOURCE_DIR}/strerror.cpp
    ${SOURCE_DIR}/application.cpp
    ${SOURCE_DIR}/socket.cpp
    ${SOURCE_DIR}/client.cpp
    ${SOURCE_DIR}/serverstateful.cpp
    ${SOURCE_DIR}/serverstateless.cpp
    ${SOURCE_DIR}/valueparser.cpp
    ${SOURCE_DIR}/responderthread.cpp
)
set (LIBTCPPUMP_SOURCE_DIR ${SOURCE_DIR}/libtcppump)
set (LIBTCPPUMP_SOURCES
    ${LIBTCPPUMP_SOURCE_DIR}/console.cpp
    ${LIBTCPPUMP_SOURCE_DIR}/cmdline.cpp
)

target_sources (net-babbler PRIVATE ${SOURCES} ${LIBTCPPUMP_SOURCES})
target_include_directories (net-babbler
    PRIVATE ${LIBTCPPUMP_SOURCE_DIR})
target_link_libraries (net-babbler pthread)

