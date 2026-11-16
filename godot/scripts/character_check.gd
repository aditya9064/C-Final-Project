extends Node3D
# Pipeline validation: load the assembled rigged character, play a clip, attach the
# SMG to the right-hand bone, render front/side/back in the arena lighting, then quit.

var _frame := 0
var _cam: Camera3D
var _anim: AnimationPlayer
var _char: Node3D
# [frame_to_capture, cam_pos, label]
var _shots := [
	[70, Vector3(0.0, 0.95, 2.6), "front"],
	[110, Vector3(2.6, 0.95, 0.0), "side"],
	[150, Vector3(0.0, 0.95, -2.6), "back"],
]
var _next := 0

func _ready() -> void:
	_build_lights()
	_build_floor()
	get_viewport().msaa_3d = Viewport.MSAA_4X
	_char = (load("res://assets/models/characters/male_dressed.glb") as PackedScene).instantiate()
	_char.scale = Vector3(0.63, 0.63, 0.63)   # ~2.8m import -> ~1.78m
	add_child(_char)
	_anim = _find(_char, "AnimationPlayer")
	if _anim:
		var want := "idle"
		if not _anim.has_animation(want):
			want = _anim.get_animation_list()[0]
		_anim.play(want)
		print("PLAYING:", want, " of ", _anim.get_animation_list())
	else:
		push_error("no AnimationPlayer")
	var skel := _find(_char, "Skeleton3D") as Skeleton3D
	if skel:
		var ba := BoneAttachment3D.new()
		ba.bone_name = "hand_r"
		skel.add_child(ba)
		var gun := (load("res://assets/models/weapons/smg_teal.glb") as PackedScene).instantiate() as Node3D
		# rough seat of the grip into the hand (refined later)
		gun.position = Vector3(0.0, 0.03, 0.0)
		gun.rotation_degrees = Vector3(0, 90, 10)
		gun.scale = Vector3(1.0, 1.0, 1.0) / 0.63   # cancel the character down-scale so the gun keeps real size
		ba.add_child(gun)
		print("HAND_R bone idx:", skel.find_bone("hand_r"))
	else:
		push_error("no Skeleton3D")
	_cam = Camera3D.new()
	_cam.fov = 40.0
	_cam.current = true
	add_child(_cam)

func _find(n: Node, cls: String) -> Node:
	if n.get_class() == cls:
		return n
	for c in n.get_children():
		var r := _find(c, cls)
		if r:
			return r
	return null

func _build_lights() -> void:
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-45, -58, 0)
	sun.light_energy = 1.6
	sun.light_color = Color(1.0, 0.96, 0.88)
	sun.shadow_enabled = true
	add_child(sun)
	var fill := DirectionalLight3D.new()
	fill.rotation_degrees = Vector3(-62, 128, 0)
	fill.light_energy = 0.72
	fill.light_color = Color(0.98, 0.93, 0.84)
	add_child(fill)

func _build_floor() -> void:
	var pm := PlaneMesh.new(); pm.size = Vector2(30, 30)
	var m := StandardMaterial3D.new(); m.albedo_color = Color(0.52, 0.52, 0.54); m.roughness = 0.95
	var mi := MeshInstance3D.new(); mi.mesh = pm; mi.material_override = m
	add_child(mi)

func _process(_dt: float) -> void:
	_frame += 1
	if _next < _shots.size() and _frame >= int(_shots[_next][0]):
		_cam.global_position = _shots[_next][1]
		_cam.look_at(Vector3(0.0, 0.92, 0.0), Vector3.UP)
		# capture next frame so the camera move applies
		if _frame >= int(_shots[_next][0]) + 1:
			var img := get_viewport().get_texture().get_image()
			img.save_png("res://char_%s.png" % _shots[_next][2])
			print("SAVED char_%s.png" % _shots[_next][2])
			_next += 1
	if _next >= _shots.size():
		get_tree().quit()
