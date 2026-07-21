cmake_minimum_required(VERSION 3.16)
# C is needed for sqlite3.c (the vendored SQLite amalgamation, see below) - everything
# else in this project is C++.
project(kung_fu_chess LANGUAGES CXX C)

# הגדרת סטנדרט C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Choose whether to build tests
option(BUILD_TESTS "Build unit and integration tests" ON)


# --- Dependencies ---------------------------------------------------------
include(FetchContent)

# Fetch Catch2 only when building tests
if(BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v2.13.9
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# Prefer that users set OpenCV_DIR externally (e.g. via CMake cache or env).
# If OpenCV is installed and discoverable, this will succeed on Windows/MSVC.
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found. Set OpenCV_DIR to your OpenCV build (e.g. C:/opencv/build) and re-run CMake.")
endif()

# IXWebSocket: the transport for the client<->server split (Network/). TLS/ZLIB are
# disabled - Phase 1 only ever talks plain ws:// on a LAN/localhost, so neither OpenSSL
# nor zlib is needed just to move text lines between two trusted local processes.
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)

# SQLite: account storage (Server/AccountStore.cpp) for Phase 4. Fetched as the official
# amalgamation (a single sqlite3.c/sqlite3.h pair) straight from sqlite.org - not a
# third-party CMake-wrapped mirror, keeping supply-chain trust anchored to the same
# upstream-authoritative bar ixwebsocket/Catch2 already meet. The amalgamation has no
# CMakeLists.txt of its own (FetchContent_MakeAvailable just populates the source; nothing
# tries to add_subdirectory it), so the one .c file is built into a library manually below,
# same as vendoring any single-file library.
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3530300.zip
    URL_HASH SHA3_256=d45c688a8cb23f68611a894a756a12d7eb6ab6e9e2468ca70adbeab3808b5ab9
    # TLS_VERIFY FALSE: this build environment's proxy/CA setup fails certificate-chain
    # revocation checking against sqlite.org specifically (CMake's downloader, not plain
    # curl, which reaches the same host fine here) - the file's actual integrity is still
    # fully guaranteed by URL_HASH above regardless of this flag, so disabling TLS identity
    # verification for just this one download doesn't weaken content authenticity.
    TLS_VERIFY FALSE
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
# SQLITE_THREADSAFE=1 (the default, set explicitly rather than assumed) is what lets
# AccountStore call into the same sqlite3* handle from multiple connection threads without
# its own extra mutex - SQLite's own internal locking already covers it.
target_compile_definitions(sqlite3 PUBLIC SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)

# --- kungfu_logic: the engine, no OpenCV, no networking --------------------
file(GLOB_RECURSE LOGIC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Logic/src/*.cpp")
add_library(kungfu_logic STATIC ${LOGIC_SOURCES})
target_include_directories(kungfu_logic PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Logic/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Core_Interfaces
)

# --- kungfu_network: WebSocket transport + wire protocol --------------------
# Pure transport + message encode/decode - no OpenCV, no rendering. Depends on
# kungfu_logic only for the plain-data shapes (Position, PieceType, the event structs)
# it serializes.
file(GLOB_RECURSE NETWORK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Network/*.cpp")
add_library(kungfu_network STATIC ${NETWORK_SOURCES})
target_include_directories(kungfu_network PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(kungfu_network PUBLIC kungfu_logic ixwebsocket ws2_32)

# --- kungfu_client: the client-side GameEngine stand-in over WebSocket -
# depends on kungfu_logic + kungfu_network only, zero OpenCV/Win32.
file(GLOB_RECURSE CLIENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Client/*.cpp")
add_library(kungfu_client STATIC ${CLIENT_SOURCES})
target_include_directories(kungfu_client PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(kungfu_client PUBLIC kungfu_logic kungfu_network)

# --- kungfu_ui: the OpenCV-backed view, depends on kungfu_logic only ----
file(GLOB_RECURSE UI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/UI/*.cpp")
add_library(kungfu_ui STATIC ${UI_SOURCES})
target_include_directories(kungfu_ui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/UI
)
target_link_libraries(kungfu_ui PUBLIC kungfu_logic)

if(TARGET OpenCV::opencv_core)
    target_link_libraries(kungfu_ui PUBLIC OpenCV::opencv_core OpenCV::imgproc OpenCV::highgui)
else()
    target_include_directories(kungfu_ui PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(kungfu_ui PUBLIC ${OpenCV_LIBS})
endif()

# SoundPlayer (UI/Windows/SoundPlayer.cpp) uses PlaySoundW from winmm - a native
# Windows API, not a new third-party dependency.
target_link_libraries(kungfu_ui PUBLIC winmm)

# --- Executables -------------------------------------------------------------
# Server: owns the real GameEngine (one per room), no OpenCV involved at all.
# Links sqlite3 (AccountStore) and bcrypt (Windows CNG, password hashing) - scoped only
# here, since the client never touches the database or does its own hashing.
file(GLOB_RECURSE SERVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Server/*.cpp")
add_executable(kungfu_server ${SERVER_SOURCES})
target_link_libraries(kungfu_server PRIVATE kungfu_logic kungfu_network sqlite3 bcrypt)

# Client: owns the OpenCV window, talks to the server instead of owning GameEngine.
add_executable(kungfu_app main.cpp)
target_link_libraries(kungfu_app PRIVATE kungfu_ui kungfu_client)

# הגדרת טסטים (Integration)
# Integration tests
if(BUILD_TESTS)
    add_executable(kungfu_integration_tests Logic/tests/integration/GameIntegrationTests.cpp)
    target_link_libraries(kungfu_integration_tests PRIVATE kungfu_logic Catch2::Catch2)
endif()

# הגדרת טסטים (Unit)
if(BUILD_TESTS)
    file(GLOB_RECURSE UNIT_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Logic/tests/unit/*.cpp")
    add_executable(kungfu_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(kungfu_tests PRIVATE kungfu_logic Catch2::Catch2)

    enable_testing()
    add_test(NAME unit_tests COMMAND kungfu_tests)
    add_test(NAME integration_tests COMMAND kungfu_integration_tests)
endif()
