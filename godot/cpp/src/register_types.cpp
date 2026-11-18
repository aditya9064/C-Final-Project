#include "fireteam_session.hpp"

#include <godot_cpp/godot.hpp>

namespace {

void initialize(godot::ModuleInitializationLevel level) {
  if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
    GDREGISTER_CLASS(fireteam::FireteamSession);
  }
}

void uninitialize(godot::ModuleInitializationLevel) {}

}  // namespace

extern "C" {
GDExtensionBool GDE_EXPORT fireteam_library_init(
    GDExtensionInterfaceGetProcAddress getProcAddress,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization) {
  godot::GDExtensionBinding::InitObject init(getProcAddress, library,
                                            initialization);
  init.register_initializer(initialize);
  init.register_terminator(uninitialize);
  init.set_minimum_library_initialization_level(
      godot::MODULE_INITIALIZATION_LEVEL_SCENE);
  return init.init();
}
}
