extends Node3D
# Preview harness: renders each subject across several clips/angles to stress-test
# clothing deformation. Saves previews/<prefix>_<label>.png.

var _subjects := [
	["res://assets/models/characters/male_tailored.glb", "res://assets/models/weapons/smg_teal.glb", "m"],
	["res://assets/models/characters/female_tailored.glb", "res://assets/models/weapons/smg_orange.glb", "f"],
]
# [clip, seek_time, cam_pos, look_y, label]
var _shots := [
	["idle", 0.0, Vector3(0.0, 1.05, 4.0), 1.0, "idle_front"],
	["idle", 0.0, Vector3(3.9, 1.05, 0.2), 1.0, "idle_side"],
	["idle", 0.0, Vector3(0.0, 1.05, -4.0), 1.0, "idle_back"],
	["walk", 0.5, Vector3(3.9, 1.05, 0.4), 1.0, "walk_side"],
	["run", 0.33, Vector3(3.9, 1.05, 0.4), 1.0, "run_side"],
	["jump", 1.0, Vector3(3.9, 1.05, 0.4), 1.0, "jump_side"],
	["death", 1.3, Vector3(3.9, 0.8, 0.6), 0.5, "death_side"],
	["reload", 0.8, Vector3(0.0, 1.05, 3.6), 1.1, "reload_front"],
	["aim", 0.12, Vector3(0.7, 1.2, 3.6), 1.25, "aim_front"],
	["aim", 0.12, Vector3(3.8, 1.25, 0.5), 1.25, "aim_side"],
	["fire", 0.3, Vector3(0.7, 1.2, 3.6), 1.25, "fire_front"],
	["reload", 0.8, Vector3(3.8, 1.1, 0.5), 1.1, "reload_side"],
]
var _si := -1
var _i := 0
var _frame := 0
var _cam: Camera3D
var _ap: AnimationPlayer
var _char: Node3D
var _prefix := ""

func _ready() -> void:
	_build_lights(); _build_floor()
	get_viewport().msaa_3d = Viewport.MSAA_4X
	_cam = Camera3D.new(); _cam.fov = 44.0; _cam.current = true; add_child(_cam)
	_load_subject()

func _load_subject() -> void:
	_si += 1
	if _si >= _subjects.size():
		get_tree().quit(); return
	if _char: _char.queue_free()
	var s = _subjects[_si]
	_prefix = s[2]
	_char = (load(s[0]) as PackedScene).instantiate() as Node3D
	add_child(_char)
	_ap = _find(_char, "AnimationPlayer") as AnimationPlayer
	var skel := _find(_char, "Skeleton3D") as Skeleton3D
	if skel:
		var ba := BoneAttachment3D.new(); ba.bone_name = "hand_r"; skel.add_child(ba)
		var g := (load(s[1]) as PackedScene).instantiate() as Node3D
		g.position = Vector3(0.0, 0.0, 0.0)
		g.rotation_degrees = Vector3(-0.89, -94.88, -78.76)  # solved: barrel forward+level, optic up
		ba.add_child(g)
	_i = -1
	_next_shot()

func _next_shot() -> void:
	_i += 1
	if _i >= _shots.size():
		_load_subject(); return
	var s = _shots[_i]
	if _ap and _ap.has_animation(s[0]):
		_ap.play(s[0]); _ap.seek(float(s[1]), true)
	_cam.global_position = s[2]
	_cam.look_at(Vector3(0.0, float(s[3]), 0.0), Vector3.UP)
	_frame = 0

func _process(_dt: float) -> void:
	_frame += 1
	if _frame == 3:
		var s = _shots[_i]
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://previews/%s_%s.png" % [_prefix, s[4]])
		print("SAVED previews/%s_%s.png" % [_prefix, s[4]])
		_next_shot()

func _find(n: Node, cls: String) -> Node:
	if n.get_class() == cls: return n
	for c in n.get_children():
		var r := _find(c, cls)
		if r: return r
	return null

func _build_lights() -> void:
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-45, -58, 0); sun.light_energy = 1.6
	sun.light_color = Color(1.0, 0.96, 0.88); sun.shadow_enabled = true
	add_child(sun)
	var fill := DirectionalLight3D.new()
	fill.rotation_degrees = Vector3(-62, 128, 0); fill.light_energy = 0.72
	fill.light_color = Color(0.98, 0.93, 0.84); add_child(fill)

func _build_floor() -> void:
	var pm := PlaneMesh.new(); pm.size = Vector2(40, 40)
	var m := StandardMaterial3D.new(); m.albedo_color = Color(0.52, 0.52, 0.54); m.roughness = 0.95
	var mi := MeshInstance3D.new(); mi.mesh = pm; mi.material_override = m; add_child(mi)
