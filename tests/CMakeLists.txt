# חיפוש או הורדת Catch2
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v2.13.9 # גרסה יציבה ופשוטה
)
FetchContent_MakeAvailable(Catch2)



file(GLOB UNIT_TEST_SOURCES "unit/*.cpp")

add_executable(unit_tests 
    TestMain.cpp
    ${UNIT_TEST_SOURCES}
)

target_link_libraries(unit_tests PRIVATE Catch2::Catch2 kungfu_core)

