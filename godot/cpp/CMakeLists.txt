FetchContent_Declare(
  godot_cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77
)
# Godot 4.5's stable extension ABI is supported by Godot 4.7.
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/build_profile.json" CACHE FILEPATH "" FORCE)
# Core validation uses exceptions; the bridge catches/logs them before Godot.
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(godot_cpp)

add_library(fireteam_godot SHARED src/register_types.cpp src/fireteam_session.cpp)
target_link_libraries(fireteam_godot PRIVATE fireteam_core godot::cpp)
set_target_properties(fireteam_godot PROPERTIES
  PREFIX ""
  OUTPUT_NAME "fireteam"
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set_target_properties(fireteam_godot PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY_${config} "${PROJECT_SOURCE_DIR}/godot/bin"
    RUNTIME_OUTPUT_DIRECTORY_${config} "${PROJECT_SOURCE_DIR}/godot/bin"
  )
endforeach()
