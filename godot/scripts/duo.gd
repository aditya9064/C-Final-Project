extends Node3D
# Renders both dressed operatives (male/teal + female/orange) with their SMGs,
# from front / side / back, in the arena lighting.

var _frame := 0
var _cam: Camera3D
var _shots := [
	[70, Vector3(0.0, 1.05, 5.4), "duo_front"],
	[110, Vector3(5.4, 1.05, 0.3), "duo_side"],
	[150, Vector3(0.0, 1.05, -5.4), "duo_back"],
]
var _next := 0

func _ready() -> void:
	_build_lights(); _build_floor()
	get_viewport().msaa_3d = Viewport.MSAA_4X
	_spawn("res://assets/models/characters/male_tailored.glb",
		"res://assets/models/weapons/smg_teal.glb", Vector3(-0.55, 0, 0))
	_spawn("res://assets/models/characters/female_tailored.glb",
		"res://assets/models/weapons/smg_orange.glb", Vector3(0.55, 0, 0))
	_cam = Camera3D.new(); _cam.fov = 42.0; _cam.current = true; add_child(_cam)

func _spawn(char_path: String, gun_path: String, pos: Vector3) -> void:
	var c := (load(char_path) as PackedScene).instantiate() as Node3D
	c.position = pos   # native height ~1.86m -> no rescale
	add_child(c)
	var ap := _find(c, "AnimationPlayer") as AnimationPlayer
	if ap and ap.has_animation("idle"):
		ap.play("idle")
	var skel := _find(c, "Skeleton3D") as Skeleton3D
	if skel:
		var ba := BoneAttachment3D.new(); ba.bone_name = "hand_r"; skel.add_child(ba)
		var g := (load(gun_path) as PackedScene).instantiate() as Node3D
		g.position = Vector3(0.02, 0.03, 0.0)
		g.rotation_degrees = Vector3(6, 92, 8)
		g.scale = Vector3.ONE
		ba.add_child(g)

func _find(n: Node, cls: String) -> Node:
	if n.get_class() == cls: return n
	for ch in n.get_children():
		var r := _find(ch, cls)
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

func _process(_dt: float) -> void:
	_frame += 1
	if _next < _shots.size() and _frame >= int(_shots[_next][0]):
		_cam.global_position = _shots[_next][1]
		_cam.look_at(Vector3(0.0, 1.0, 0.0), Vector3.UP)
		if _frame >= int(_shots[_next][0]) + 1:
			var img := get_viewport().get_texture().get_image()
			img.save_png("res://%s.png" % _shots[_next][2])
			print("SAVED %s.png" % _shots[_next][2])
			_next += 1
	if _next >= _shots.size():
		get_tree().quit()
