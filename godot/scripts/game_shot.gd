extends Node3D
# One-shot capture of the playable game.tscn from the LOCAL player's own camera,
# to confirm the arena + player + third-person view render together. Windowed:
#   godot --path godot --rendering-driver vulkan --resolution 1280x720 \
#         --quit-after 160 res://scenes/game_shot.tscn -- --out <dir>
var _frames := 0
var _out := "user://review"

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		if a[i] == "--out" and i + 1 < a.size():
			_out = a[i + 1]
	DirAccess.make_dir_recursive_absolute(_out)
	add_child(load("res://scenes/game.tscn").instantiate())

func _process(_d: float) -> void:
	_frames += 1
	if _frames == 70:
		await RenderingServer.frame_post_draw
		var img := get_viewport().get_texture().get_image()
		img.save_png(_out + "/game_inarena.png")
		print("GAME_SHOT saved")
		get_tree().quit(0)
