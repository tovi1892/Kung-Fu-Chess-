cmake_minimum_required(VERSION 3.16)
project(kung_fu_chess LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v2.13.9
)
FetchContent_MakeAvailable(Catch2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kungfu_engine
    src/common/Position.cpp
    src/pieces/Piece.cpp
    src/pieces/King.cpp
    src/pieces/Queen.cpp
    src/pieces/Rook.cpp
    src/pieces/Bishop.cpp
    src/pieces/Knight.cpp
    src/pieces/Pawn.cpp
    src/board/Board.cpp
    src/movement/MovementSystem.cpp
    src/collision/CollisionSystem.cpp
    src/rules/RuleEngine.cpp
    src/game/Game.cpp
    src/game/UIInputAdapter.cpp
    src/BoardParser.cpp
)
target_include_directories(kungfu_engine PUBLIC include)

add_executable(kungfu_app
    main.cpp
)
target_link_libraries(kungfu_app PRIVATE kungfu_engine)
target_include_directories(kungfu_app PRIVATE include)

add_executable(kungfu_integration_tests
    tests/integration/GameIntegrationTests.cpp
)
target_link_libraries(kungfu_integration_tests PRIVATE kungfu_engine)
target_include_directories(kungfu_integration_tests PRIVATE include)

add_test(NAME integration_tests COMMAND kungfu_integration_tests)

# הגדרת הטסטים של Catch2
add_executable(kungfu_tests
    tests/unit/TestMain.cpp
    tests/unit/PositionTests.cpp
    tests/unit/PieceTests.cpp
    tests/unit/BoardTests.cpp
    tests/unit/RuleEngineTests.cpp
    tests/unit/PieceTypesTests.cpp
    tests/unit/MovementSystemTests.cpp
    tests/unit/CollisionSystemTests.cpp
    tests/unit/GameTests.cpp
    tests/unit/PawnTests.cpp
    tests/unit/JumpTests.cpp
    tests/unit/ConfigAndBoundaryTests.cpp
    tests/unit/UIInputAdapterTests.cpp
    tests/unit/GameRealTimeTests.cpp
)

# הקישור חייב לבוא אחרי ה-add_executable
target_link_libraries(kungfu_tests PRIVATE kungfu_engine Catch2::Catch2)
target_include_directories(kungfu_tests PRIVATE include)

enable_testing()
add_test(NAME unit_tests COMMAND kungfu_tests)