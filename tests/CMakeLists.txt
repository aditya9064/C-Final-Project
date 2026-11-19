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
  test_vector3.cpp
  test_entity.cpp
  test_weapon.cpp
  test_network_manager.cpp
  test_game_world.cpp
  test_game_state.cpp
  test_delta_time.cpp
  test_combat.cpp
  test_grenade.cpp
  test_bot.cpp
)
target_include_directories(fireteam_tests PRIVATE
  ${doctest_SOURCE_DIR}
)
target_link_libraries(fireteam_tests PRIVATE fireteam_core)

add_test(NAME fireteam_tests COMMAND fireteam_tests)
