extends Node3D
# Product-shot showcase for the generated weapons (and, later, characters).
# Reuses the arena's WorldEnvironment (set in the .tscn) and the same 3-light
# rig as build_arena.gd so assets are lit identically to the arena. Renders one
# framed screenshot after the shadows/SSAO settle, then quits.
# Run:  <godot_console> --path godot --rendering-driver vulkan \
#         --resolution 1600x900 --quit-after 240 res://scenes/showcase.tscn

var _frame := 0
const SHOT_FRAME := 72

func _ready() -> void:
	_build_lights()
	_build_stage()
	_spawn_guns()
	_build_camera()
	get_viewport().msaa_3d = Viewport.MSAA_4X

# same rig as build_arena.gd::_build_lighting (warm sun + canopy omni + sky fill)
func _build_lights() -> void:
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-45, -58, 0)
	sun.light_energy = 1.6
	sun.light_color = Color(1.0, 0.96, 0.88)
	sun.shadow_enabled = true
	sun.light_angular_distance = 0.7
	sun.shadow_bias = 0.03
	sun.shadow_normal_bias = 1.2
	add_child(sun)
	var skyfill := DirectionalLight3D.new()
	skyfill.rotation_degrees = Vector3(-62, 128, 0)
	skyfill.light_energy = 0.72
	skyfill.light_color = Color(0.98, 0.93, 0.84)
	skyfill.shadow_enabled = false
	add_child(skyfill)

func _build_stage() -> void:
	# neutral warm-grey studio floor so the guns read against a clean ground
	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = Vector2(30, 30)
	var fm := StandardMaterial3D.new()
	fm.albedo_color = Color(0.52, 0.52, 0.54)
	fm.roughness = 0.95
	var mi := MeshInstance3D.new()
	mi.mesh = floor_mesh
	mi.material_override = fm
	add_child(mi)
	# a simple pedestal under each gun
	for x in [-0.0]:
		pass

func _load(path: String) -> Node3D:
	var ps := load(path)
	if ps == null:
		push_error("missing: " + path)
		return null
	return (ps as PackedScene).instantiate()

func _spawn_guns() -> void:
	var teal := _load("res://assets/models/weapons/smg_teal.glb")
	var orange := _load("res://assets/models/weapons/smg_orange.glb")
	# guns are modelled lying along +X (muzzle +X); lay them flat, 3/4 view
	if teal:
		teal.position = Vector3(0.0, 0.98, 0.32)
		teal.rotation_degrees = Vector3(0, -18, 0)
		add_child(teal)
	if orange:
		orange.position = Vector3(0.0, 0.98, -0.32)
		orange.rotation_degrees = Vector3(0, -18, 0)
		add_child(orange)
	# two low pedestals
	for z in [0.32, -0.32]:
		var ped := BoxMesh.new()
		ped.size = Vector3(0.9, 0.9, 0.22)
		var pm := StandardMaterial3D.new()
		pm.albedo_color = Color(0.30, 0.30, 0.32)
		pm.roughness = 0.8
		var pmi := MeshInstance3D.new()
		pmi.mesh = ped
		pmi.material_override = pm
		pmi.position = Vector3(0.0, 0.45, z)
		add_child(pmi)

func _build_camera() -> void:
	var cam := Camera3D.new()
	cam.fov = 38.0
	cam.current = true
	add_child(cam)                       # must be in-tree before look_at
	cam.global_position = Vector3(0.62, 1.42, 1.95)
	cam.look_at(Vector3(0.0, 0.95, 0.0), Vector3.UP)

func _process(_dt: float) -> void:
	_frame += 1
	if _frame == SHOT_FRAME:
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://showcase_guns.png")
		print("SHOWCASE_SAVED res://showcase_guns.png")
		get_tree().quit()
