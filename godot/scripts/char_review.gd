extends Node3D
# Visual review for Player #6: spawns the teal male + orange female with the SMG,
# lights them, and saves a render. Run WINDOWED (not --headless):
#   godot --path godot --rendering-driver vulkan --resolution 1280x960 \
#         --quit-after 120 res://scenes/char_review.tscn -- --view front|side|back
# Writes <out_dir>/char_<view>.png (out_dir via "--out <dir>", default user dir).

var _view := "front"
var _out := "user://review"   # override with "--out <dir>"
var _frames := 0

func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	for i in args.size():
		if args[i] == "--view" and i + 1 < args.size():
			_view = args[i + 1]
		elif args[i] == "--out" and i + 1 < args.size():
			_out = args[i + 1]
	DirAccess.make_dir_recursive_absolute(_out)

	# Lighting + neutral backdrop.
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.16, 0.17, 0.19)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.45, 0.47, 0.5)
	env.ambient_light_energy = 0.35
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	var we := WorldEnvironment.new(); we.environment = env; add_child(we)
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-50), deg_to_rad(-40), 0)
	sun.light_energy = 1.3; add_child(sun)
	var fill := DirectionalLight3D.new()
	fill.rotation = Vector3(deg_to_rad(-20), deg_to_rad(130), 0)
	fill.light_energy = 0.4; add_child(fill)

	# Floor WITH a collider so the CharacterBody3D players rest on it.
	var floor_body := StaticBody3D.new()
	var fcs := CollisionShape3D.new()
	var fbx := BoxShape3D.new(); fbx.size = Vector3(12, 1, 12); fcs.shape = fbx
	floor_body.add_child(fcs); floor_body.position = Vector3(0, -0.5, 0)
	var floor_mi := MeshInstance3D.new()
	var pm := PlaneMesh.new(); pm.size = Vector2(12, 12)
	var fmat := StandardMaterial3D.new(); fmat.albedo_color = Color(0.28, 0.3, 0.33); fmat.roughness = 0.9
	pm.material = fmat; floor_mi.mesh = pm; floor_mi.position = Vector3(0, 0.5, 0)
	floor_body.add_child(floor_mi)
	add_child(floor_body)

	_spawn(0, Vector3(-1.1, 0.9, 0))  # teal male
	_spawn(1, Vector3(1.1, 0.9, 0))   # orange female

	var cam := Camera3D.new()
	add_child(cam)
	if _view == "top":
		cam.position = Vector3(0.0, 7.0, 0.0)
		cam.look_at(Vector3(0, 1.0, 0), Vector3(0, 0, -1))
	else:
		match _view:
			"side": cam.position = Vector3(5.0, 1.6, 0.0)
			"back": cam.position = Vector3(0.0, 1.6, -4.5)
			_: cam.position = Vector3(0.0, 1.6, 4.5)
		cam.look_at(Vector3(0, 1.0, 0))
	cam.current = true

func _spawn(team: int, pos: Vector3) -> void:
	var p: Node = load("res://scenes/player.tscn").instantiate()
	p.team_id = team
	p.is_local = false
	p.forced_anim = "aim_idle"  # a natural weapon-holding pose
	p.spawn_face_inward = false  # face the camera for the showcase
	p.position = pos
	add_child(p)

func _process(_d: float) -> void:
	_frames += 1
	if _frames == 50:
		await RenderingServer.frame_post_draw
		var img := get_viewport().get_texture().get_image()
		var path := _out + "/char_" + _view + ".png"
		img.save_png(path)
		print("CHAR_REVIEW saved ", path)
		get_tree().quit(0)
