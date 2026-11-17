class_name FireteamInput
extends RefCounted
# Registers the gameplay input actions at runtime (robust + editor-independent).
# Call FireteamInput.ensure_actions() once at startup. Teammates can also mirror
# these into Project Settings > Input Map if they prefer editor-defined actions.

static func ensure_actions() -> void:
	_key("move_forward", KEY_W)
	_key("move_back", KEY_S)
	_key("move_left", KEY_A)
	_key("move_right", KEY_D)
	_key("run", KEY_SHIFT)
	_key("jump", KEY_SPACE)
	_key("reload", KEY_R)
	_mouse("fire", MOUSE_BUTTON_LEFT)
	_mouse("aim", MOUSE_BUTTON_RIGHT)

static func _key(action: String, physical_keycode: int) -> void:
	if not InputMap.has_action(action):
		InputMap.add_action(action)
	var ev := InputEventKey.new()
	ev.physical_keycode = physical_keycode
	InputMap.action_add_event(action, ev)

static func _mouse(action: String, button: int) -> void:
	if not InputMap.has_action(action):
		InputMap.add_action(action)
	var ev := InputEventMouseButton.new()
	ev.button_index = button
	InputMap.action_add_event(action, ev)
