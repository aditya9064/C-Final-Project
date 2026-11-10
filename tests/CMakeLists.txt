include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)

add_executable(fireteam_tests
  test_main.cpp
  test_networking_example.cpp
  test_vector2.cpp
  test_entity.cpp
  test_game_world.cpp
  test_game_state.cpp
)
target_include_directories(fireteam_tests PRIVATE
  ${doctest_SOURCE_DIR}
  ${enet_SOURCE_DIR}/include
)
target_link_libraries(fireteam_tests PRIVATE fireteam_core enet)

if(WIN32)
  target_link_libraries(fireteam_tests PRIVATE ws2_32 winmm)
endif()

add_test(NAME fireteam_tests COMMAND fireteam_tests)
