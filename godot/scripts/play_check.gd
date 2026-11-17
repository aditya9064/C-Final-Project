extends Node3D
# Windowed playable check for Player #6 (Increment 2): drives a LOCAL player with
# simulated input + its real third-person camera, asserts it moves along the
# camera's forward, and saves a frame FROM the player camera so the aim/gun
# direction can be eyeballed. Run:
#   godot --path godot --rendering-driver vulkan --resolution 1280x960 \
#         --quit-after 200 res://scenes/play_check.tscn -- --out <dir>

var _p: Node3D
var _frames := 0
var _start := Vector3.ZERO
var _along := 0.0
var _out := "user://review"

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		if a[i] == "--out" and i + 1 < a.size():
			_out = a[i + 1]
	DirAccess.make_dir_recursive_absolute(_out)
	FireteamInput.ensure_actions()

	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.16, 0.17, 0.19)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.45, 0.47, 0.5); env.ambient_light_energy = 0.35
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	var we := WorldEnvironment.new(); we.environment = env; add_child(we)
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-50), deg_to_rad(-40), 0); sun.light_energy = 1.3
	add_child(sun)

	var fb := StaticBody3D.new(); var cs := CollisionShape3D.new()
	var bx := BoxShape3D.new(); bx.size = Vector3(40, 1, 40); cs.shape = bx
	fb.add_child(cs); fb.position = Vector3(0, -0.5, 0)
	var mi := MeshInstance3D.new(); var pm := PlaneMesh.new(); pm.size = Vector2(40, 40)
	var mat := StandardMaterial3D.new(); mat.albedo_color = Color(0.28, 0.3, 0.33); pm.material = mat
	mi.mesh = pm; mi.position = Vector3(0, 0.5, 0); fb.add_child(mi); add_child(fb)

	_p = load("res://scenes/player.tscn").instantiate()
	_p.team_id = 0
	_p.is_local = true
	_p.position = Vector3(0, 1.0, 0)
	add_child(_p)

func _cam() -> Camera3D:
	var cams := _p.find_children("*", "Camera3D", true, false)
	return cams[0] as Camera3D if cams.size() > 0 else null

func _process(_d: float) -> void:
	_frames += 1
	if _frames == 20:
		_start = _p.global_position
		Input.action_press("move_forward")
	elif _frames == 60:
		Input.action_release("move_forward")
		var disp := _p.global_position - _start
		var c := _cam()
		var fwd := -c.global_transform.basis.z; fwd.y = 0; fwd = fwd.normalized()
		_along = Vector2(disp.x, disp.z).normalized().dot(Vector2(fwd.x, fwd.z))
		# Team 0 (teal) spawns facing +X (inward); W must move it toward +X in WORLD
		# space, and the camera must sit behind it (not looking out of the arena).
		print("PLAY_MOVE disp=", disp, " along_cam_fwd=", _along, " world_dx=", disp.x)
		Input.action_press("aim")  # now aim so the character faces camera-forward
	elif _frames == 85:
		await RenderingServer.frame_post_draw
		var img := get_viewport().get_texture().get_image()
		img.save_png(_out + "/play_thirdperson.png")
		var disp2 := _p.global_position - _start
		var ok := _along > 0.7 and disp2.x > 0.1
		print("PLAY_CHECK ", "PASS" if ok else "FAIL",
			" along_cam_fwd=", _along, " world_dx=", disp2.x)
		get_tree().quit(0 if ok else 1)
