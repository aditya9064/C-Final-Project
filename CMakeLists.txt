cmake_minimum_required(VERSION 3.16)
project(FireteamSkirmish LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Some fetched third-party dependencies (e.g. doctest) declare an older
# cmake_minimum_required than modern CMake accepts by default. This keeps
# their build working without patching vendored files.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

include(FetchContent)

# --- SFML: rendering, window, input -------------------------------------
FetchContent_Declare(
  SFML
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 2.6.2
  GIT_SHALLOW TRUE
)
set(SFML_BUILD_AUDIO FALSE CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK FALSE CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS TRUE CACHE BOOL "" FORCE)

# --- ENet: lightweight UDP networking for real-time multiplayer ---------
FetchContent_Declare(
  enet
  GIT_REPOSITORY https://github.com/lsalzman/enet.git
  GIT_TAG v1.3.18
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SFML enet)

# --- Game logic library ----------------------------------------------------
# Everything that is NOT main() lives here, so the executable and the test
# binary can both link it without compiling the same sources twice. Keeping
# gameplay code free of SFML/ENet where possible also makes it unit-testable
# without opening a window or a socket.
add_library(fireteam_core STATIC
  src/Vector2.cpp
  src/Entity.cpp
  src/Weapon.cpp
  src/Rifle.cpp
)
# PUBLIC: anyone linking fireteam_core also gets this include path.
target_include_directories(fireteam_core PUBLIC include)

# --- Smoke-test executable ------------------------------------------------
# This is a minimal build/dependency check, NOT the game itself. It proves
# SFML (rendering) and ENet (networking) are fetched, compiled, and linked
# correctly on a fresh clone/CI runner. Real game source will replace/extend
# this as subsystems (engine, rendering, combat, networking) land.
add_executable(fireteam_skirmish src/main.cpp)
target_include_directories(fireteam_skirmish PRIVATE ${enet_SOURCE_DIR}/include)
target_link_libraries(fireteam_skirmish PRIVATE fireteam_core sfml-graphics sfml-window sfml-system enet)

if(WIN32)
  target_link_libraries(fireteam_skirmish PRIVATE ws2_32 winmm)
endif()

# Copy shared libraries next to the executable on Windows, where they
# aren't found via rpath the way they are on macOS/Linux build trees.
if(WIN32)
  add_custom_command(TARGET fireteam_skirmish POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:fireteam_skirmish>
      $<TARGET_FILE_DIR:fireteam_skirmish>
    COMMAND_EXPAND_LISTS
  )
endif()

enable_testing()
add_subdirectory(tests)
