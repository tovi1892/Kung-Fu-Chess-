cmake_minimum_required(VERSION 3.16)
project(kung_fu_chess LANGUAGES CXX)

# הגדרת סטנדרט C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Choose whether to build tests
option(BUILD_TESTS "Build unit and integration tests" ON)


# --- Dependencies ---------------------------------------------------------
# Fetch Catch2 only when building tests
include(FetchContent)
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

# יצירת הספרייה הראשית (ה-Engine)

# Collect engine sources. Using GLOB_RECURSE keeps the CMakeLists concise
# while still including all .cpp files under the expected source dirs.
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Logic/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/UI/Img/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/UI/UI_OpenCV/*.cpp"
)

add_library(kungfu_engine STATIC ${ENGINE_SOURCES})


# Include directories needed to resolve headers like "board/Board.hpp"
target_include_directories(kungfu_engine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Logic/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Core_Interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/UI
)

# Link OpenCV (prefer imported targets if available)
if(TARGET OpenCV::opencv_core)
    target_link_libraries(kungfu_engine PUBLIC OpenCV::opencv_core OpenCV::imgproc OpenCV::highgui)
else()
    target_include_directories(kungfu_engine PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(kungfu_engine PUBLIC ${OpenCV_LIBS})
endif()

# בניית ה-App הראשי
# Application
add_executable(kungfu_app main.cpp)
target_link_libraries(kungfu_app PRIVATE kungfu_engine)

# הגדרת טסטים (Integration)
# Integration tests
if(BUILD_TESTS)
    add_executable(kungfu_integration_tests Logic/tests/integration/GameIntegrationTests.cpp)
    target_link_libraries(kungfu_integration_tests PRIVATE kungfu_engine Catch2::Catch2)
endif()

# הגדרת טסטים (Unit)
if(BUILD_TESTS)
    file(GLOB_RECURSE UNIT_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Logic/tests/unit/*.cpp")
    add_executable(kungfu_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(kungfu_tests PRIVATE kungfu_engine Catch2::Catch2)

    enable_testing()
    add_test(NAME unit_tests COMMAND kungfu_tests)
    add_test(NAME integration_tests COMMAND kungfu_integration_tests)
endif()