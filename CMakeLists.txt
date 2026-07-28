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

# The OpenCV-based local client (kungfu_ui/kungfu_app) - off for server-only builds (e.g.
# Dockerfile.server's Ubuntu container, which has no OpenCV installed at all) so configure
# doesn't even search for OpenCV in that case. On by default, matching every build this
# project has done before this option existed.
option(BUILD_APP "Build the OpenCV-based local client (kungfu_ui/kungfu_app)" ON)

# PostgresAccountRepository (Server/PostgresAccountRepository.cpp) via libpqxx - off by
# default so the native Windows/SQLite dev build (see CLAUDE.md) stays exactly as it was
# before this option existed, with no libpqxx search at all. Dockerfile.server turns this on;
# see Server/IAccountRepository.hpp for why swapping backends needs nothing else.
option(KUNGFU_ENABLE_POSTGRES "Build PostgresAccountRepository via libpqxx" OFF)

# RedisRoomRegistry (Gateway/RedisRoomRegistry.cpp) via hiredis - off by default so the
# native Windows dev build (and any single-Gateway-process local run) stays exactly as it
# was before this option existed, with no hiredis search at all. Dockerfile.gateway turns
# this on; see Gateway/IRoomRegistry.hpp for why swapping backends needs nothing else.
option(KUNGFU_ENABLE_REDIS "Build RedisRoomRegistry via hiredis" OFF)


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
if(BUILD_APP)
    find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui)
    if(NOT OpenCV_FOUND)
        message(FATAL_ERROR "OpenCV not found. Set OpenCV_DIR to your OpenCV build (e.g. C:/opencv/build) and re-run CMake.")
    endif()
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
target_link_libraries(kungfu_network PUBLIC kungfu_logic ixwebsocket)
if(WIN32)
    # Winsock - needed by ixwebsocket's socket layer on Windows only; Linux (Dockerfile.server)
    # uses the native BSD sockets API already provided by libc, no separate library to link.
    target_link_libraries(kungfu_network PUBLIC ws2_32)
endif()

# --- kungfu_client: the client-side GameEngine stand-in over WebSocket -
# depends on kungfu_logic + kungfu_network only, zero OpenCV/Win32.
file(GLOB_RECURSE CLIENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Client/*.cpp")
add_library(kungfu_client STATIC ${CLIENT_SOURCES})
target_include_directories(kungfu_client PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(kungfu_client PUBLIC kungfu_logic kungfu_network)

# --- kungfu_ui: the OpenCV-backed view, depends on kungfu_logic only ----
if(BUILD_APP)
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
endif()

# --- kungfu_accounts: account storage/auth, shared by kungfu_server and kungfu_api_gateway --
# Carved out of Server/*.cpp (files stay physically in Server/ - every existing #include in
# GameServer.cpp/MatchResult.cpp/main.cpp stays valid unchanged) so the REST API Gateway can
# link IAccountRepository/PostgresAccountRepository/EnvConfig without duplicating them.
# Exactly one of SqliteAccountRepository.cpp/PostgresAccountRepository.cpp is compiled,
# matching which one main.cpp's #ifdef KUNGFU_ENABLE_POSTGRES actually instantiates - not
# just unlinked, genuinely never attempted, since each pulls in headers the other platform
# doesn't have at all (bcrypt.h/windows.h are Windows-only; pqxx/openssl needs libpqxx/
# OpenSSL installed, which the native Windows dev build never requires).
set(ACCOUNTS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Server/EnvConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Server/SqliteAccountRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Server/PostgresAccountRepository.cpp
)
if(KUNGFU_ENABLE_POSTGRES)
    list(REMOVE_ITEM ACCOUNTS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Server/SqliteAccountRepository.cpp")
else()
    list(REMOVE_ITEM ACCOUNTS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Server/PostgresAccountRepository.cpp")
endif()
add_library(kungfu_accounts STATIC ${ACCOUNTS_SOURCES})
target_include_directories(kungfu_accounts PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Server
)

if(KUNGFU_ENABLE_POSTGRES)
    # Ubuntu 22.04's libpqxx-dev resolves to libpqxx 6.4.5 (confirmed from an actual
    # Dockerfile.server build log - Ubuntu's package, despite the name, is well behind
    # libpqxx's real 7.x release line), which predates libpqxx's own CMake package config
    # (added in 7.x) - find_package(libpqxx CONFIG) would fail here. pkg-config is what
    # libpqxx 6.x actually ships on Ubuntu, so use that instead; it also transitively pulls
    # in libpq's flags/libs, no separate find_package(PostgreSQL) needed.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
    target_link_libraries(kungfu_accounts PUBLIC PkgConfig::PQXX)
    target_compile_definitions(kungfu_accounts PUBLIC KUNGFU_ENABLE_POSTGRES)

    # PostgresAccountRepository needs a portable (Linux-buildable) PBKDF2 implementation -
    # SqliteAccountRepository's Windows-CNG one (bcrypt.h) doesn't compile outside Windows.
    # OpenSSL's PKCS5_PBKDF2_HMAC is the standard choice here rather than hand-rolling PBKDF2/
    # HMAC/SHA-256, which would be a real security risk to get subtly wrong.
    find_package(OpenSSL REQUIRED)
    target_link_libraries(kungfu_accounts PUBLIC OpenSSL::Crypto)
else()
    # sqlite3 (SqliteAccountRepository) and bcrypt (Windows CNG, password hashing) - both
    # Windows/native-only, never linked (and, per the exclusion above, never even compiled
    # against) on the Postgres/Linux path.
    target_link_libraries(kungfu_accounts PUBLIC sqlite3 bcrypt)
endif()

# --- Executables -------------------------------------------------------------
# Server: owns the real GameEngine (one per room), no OpenCV involved at all.
file(GLOB_RECURSE SERVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Server/*.cpp")
list(REMOVE_ITEM SERVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Server/EnvConfig.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Server/SqliteAccountRepository.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Server/PostgresAccountRepository.cpp"
)
add_executable(kungfu_server ${SERVER_SOURCES})
target_link_libraries(kungfu_server PRIVATE kungfu_logic kungfu_network kungfu_accounts)

# Client: owns the OpenCV window, talks to the server instead of owning GameEngine.
if(BUILD_APP)
    add_executable(kungfu_app main.cpp)
    target_link_libraries(kungfu_app PRIVATE kungfu_ui kungfu_client)
endif()

# Gateway: a WS proxy in front of a set of Shards (kungfu_server instances) - understands
# just enough of the protocol to route LOGIN/JOIN, never the game-state traffic, so it needs
# only kungfu_network, not kungfu_logic, no OpenCV. See Gateway/WsGateway.hpp for the full
# rationale. Exactly one of InMemoryRoomRegistry.cpp/RedisRoomRegistry.cpp is compiled,
# matching which one main.cpp's #ifdef KUNGFU_ENABLE_REDIS actually instantiates - same
# mutual-exclusion pattern as kungfu_server's Sqlite/PostgresAccountRepository split above.
file(GLOB_RECURSE GATEWAY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Gateway/*.cpp")
if(KUNGFU_ENABLE_REDIS)
    list(REMOVE_ITEM GATEWAY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Gateway/InMemoryRoomRegistry.cpp")
else()
    list(REMOVE_ITEM GATEWAY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Gateway/RedisRoomRegistry.cpp")
endif()
add_executable(kungfu_gateway ${GATEWAY_SOURCES})
target_link_libraries(kungfu_gateway PRIVATE kungfu_network)

if(KUNGFU_ENABLE_REDIS)
    # Same pkg-config path as KUNGFU_ENABLE_POSTGRES's libpqxx block above - Ubuntu 22.04's
    # libhiredis-dev ships a hiredis.pc, no CMake package config needed.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(HIREDIS REQUIRED IMPORTED_TARGET hiredis)
    target_link_libraries(kungfu_gateway PRIVATE PkgConfig::HIREDIS)
    target_compile_definitions(kungfu_gateway PRIVATE KUNGFU_ENABLE_REDIS)
endif()

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
