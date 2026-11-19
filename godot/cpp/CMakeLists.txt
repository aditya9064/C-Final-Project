cmake_minimum_required(VERSION 3.17)
project(fireteam_gdext LANGUAGES CXX)

# The C++ gameplay layer (issues #6/#7) as a Godot GDExtension. It reuses the
# graded, unit-tested fireteam_core sources UNCHANGED and adds thin Godot
# adapters. See godot/cpp/README.md and docs/evidence/gdextension-spike-2026-11-16.md.

set(CMAKE_CXX_STANDARD 20)              # fireteam_core uses std::erase_if / C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "repo root (for src/ and include/)")

# --- godot-cpp -------------------------------------------------------------
# Default (teammates / CI): fetch + build godot-cpp pinned at the spike-proven
# commit. Fast local path: -DGODOTCPP_PREBUILT=ON reuses an already-built lib.
option(GODOTCPP_PREBUILT "Link a prebuilt godot-cpp instead of building it" OFF)
if(GODOTCPP_PREBUILT)
  add_library(godot-cpp STATIC IMPORTED GLOBAL)
  set_target_properties(godot-cpp PROPERTIES IMPORTED_LOCATION "${GODOTCPP_LIB}")
  target_include_directories(godot-cpp INTERFACE
    "${GODOTCPP_SRC}/include" "${GODOTCPP_SRC}/gdextension" "${GODOTCPP_GEN}/gen/include")
  if(WIN32)
    target_compile_definitions(godot-cpp INTERFACE
      DEBUG_ENABLED GDEXTENSION THREADS_ENABLED WINDOWS_ENABLED)
  endif()
else()
  include(FetchContent)
  # godot-cpp master requires the target API version explicitly; 4.7 matches the
  # pinned commit's bundled extension_api-4-7.json and our Godot 4.7 client.
  set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API version for godot-cpp")
  FetchContent_Declare(godot-cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG abb196939767b7bbee0b3a380faefdcbdb118a54)  # bundles extension_api-4-7.json
  FetchContent_MakeAvailable(godot-cpp)
endif()

# --- the extension ---------------------------------------------------------
add_library(fireteam SHARED
  src/register_types.cpp
  src/player_body.cpp
  src/game_world_node.cpp
  # REUSED UNCHANGED from fireteam_core:
  ${CORE}/src/Vector2.cpp
  ${CORE}/src/Entity.cpp
  ${CORE}/src/GameState.cpp
  ${CORE}/src/Player.cpp)

target_include_directories(fireteam PRIVATE src "${CORE}/include")
target_compile_options(fireteam PRIVATE -fvisibility=hidden)
target_link_libraries(fireteam PRIVATE godot-cpp)

if(MINGW)
  # GCC 15 + full -static drops CRT intrinsics; keep CRT dynamic, static the rest.
  # The DLL still needs libwinpthread-1.dll shipped beside it (spike-confirmed).
  # MinGW-only: MSVC rejects these flags.
  target_link_options(fireteam PRIVATE
    -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
endif()

# No "lib" prefix: output fireteam.dll (win) / fireteam.dylib (mac), which the
# fireteam.gdextension library entries point at, dropped in godot/cpp/bin/.
set_target_properties(fireteam PROPERTIES
  PREFIX "" OUTPUT_NAME "fireteam"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
