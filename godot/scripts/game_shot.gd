extends Node3D
# Capture / verify the playable game.tscn from the LOCAL player's own camera.
#   (default)  settle, then save game_inarena.png
#   --kill     kill the local player, let game_main's RespawnAuthority revive it,
#              assert it is alive back at TEAL_SPAWN, save game_respawned.png.
# Windowed:
#   godot --path godot --rendering-driver vulkan --resolution 1280x720 \
#         --quit-after 700 res://scenes/game_shot.tscn -- --out <dir> [--kill]
const TEAL_SPAWN := Vector3(-18, 1.2, 0)

var _frames := 0
var _out := "user://review"
var _kill := false
var _game: Node
var _state := 0
var _elapsed := 0.0
var _mark := 0.0

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		if a[i] == "--out" and i + 1 < a.size(): _out = a[i + 1]
		elif a[i] == "--kill": _kill = true
	DirAccess.make_dir_recursive_absolute(_out)
	_game = load("res://scenes/game.tscn").instantiate()
	add_child(_game)

func _shot(name: String) -> void:
	get_viewport().get_texture().get_image().save_png(_out + "/" + name + ".png")

func _process(delta: float) -> void:
	_elapsed += delta
	if not _kill:
		_frames += 1
		if _frames == 70:
			await RenderingServer.frame_post_draw
			_shot("game_inarena")
			print("GAME_SHOT saved")
			get_tree().quit(0)
		return

	# --kill: exercise the full wired respawn path in game.tscn.
	match _state:
		0:
			if _elapsed > 1.0:
				_game._local.apply_damage(100)   # combat lands the kill
				_state = 1
		1:
			if _elapsed > 2.0 and _game._local.get_health() > 0 \
					and not _game._local.is_awaiting_respawn():
				_mark = _elapsed                 # revived by game_main's authority
				_state = 2
			elif _elapsed > 9.0:
				print("GAME_SHOT FAIL: authority never respawned")
				get_tree().quit(1)
		2:
			if _game._local.is_on_floor() and _elapsed > _mark + 0.3:
				await RenderingServer.frame_post_draw
				_shot("game_respawned")
				var d: float = (_game._local.global_position - TEAL_SPAWN).length()
				var ok: bool = _game._local.get_health() > 0 and d < 1.5
				print("GAME_SHOT ", "PASS" if ok else "FAIL",
					" respawned alive at TEAL_SPAWN d=", d)
				get_tree().quit(0 if ok else 1)
