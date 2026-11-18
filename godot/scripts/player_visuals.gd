extends Node3D

const SOLDIERS := [
	"res://assets/models/characters/fireteam_teal_soldier_team_hat.glb",
	"res://assets/models/characters/fireteam_orange_soldier_team_hat.glb",
]
const VIEWMODEL := "res://assets/models/weapons/fennec_animated_viewmodel.glb"

var soldiers: Array[Node3D] = []
var viewmodel: Node3D
var animation_player: AnimationPlayer


func setup(session: FireteamSession, camera: Camera3D) -> bool:
	for index in range(2):
		var body := session.get_node("HostPlayer" if index == 0 else "ClientPlayer") as Node3D
		# The C++ capsule remains the collision shape; only its debug mesh is hidden.
		for child in body.get_children():
			if child is MeshInstance3D:
				child.hide()

		var soldier := _make_soldier(SOLDIERS[index])
		if soldier == null:
			return false
		body.add_child(soldier)
		soldiers.append(soldier)

	viewmodel = _make_viewmodel()
	if viewmodel == null:
		return false
	camera.add_child(viewmodel)
	viewmodel.hide()
	return true


func update_view(state: Dictionary) -> void:
	var player_id: int = state.local_player_id
	var should_show: bool = (
		state.active
		and player_id >= 1 and player_id <= state.players.size()
		and state.players[player_id - 1].id == player_id
		and state.players[player_id - 1].health > 0
	)
	if should_show != viewmodel.visible:
		viewmodel.visible = should_show
		if should_show:
			animation_player.play("draw_first")
		else:
			animation_player.stop()


func play_fire() -> void:
	if viewmodel.visible:
		animation_player.stop()
		animation_player.play("fire")


func play_reload() -> void:
	if viewmodel.visible:
		animation_player.play("reload")


func _make_soldier(path: String) -> Node3D:
	var pivot := Node3D.new()
	pivot.name = "SoldierVisual"
	var corrected := Node3D.new()
	# The supplied mesh has a baked sideways lean as well as a Z-up axis.
	corrected.rotation.z = -0.6
	var oriented := Node3D.new()
	# These user-provided GLBs use Z-up; Godot uses Y-up.
	oriented.rotation = Vector3(-PI / 2.0, PI, 0)
	var model := _instantiate(path)
	if model == null:
		oriented.free()
		corrected.free()
		pivot.free()
		return null
	oriented.add_child(model)
	corrected.add_child(oriented)
	pivot.add_child(corrected)
	var bounds := _bounds(corrected, Transform3D.IDENTITY)
	if bounds.size.y <= 0.0:
		push_error("Soldier model has invalid bounds: " + path)
		pivot.free()
		return null
	var factor := 1.8 / bounds.size.y
	pivot.scale = Vector3.ONE * factor
	pivot.position = Vector3(
		-bounds.get_center().x * factor,
		-bounds.position.y * factor,
		-bounds.get_center().z * factor
	)
	return pivot


func _make_viewmodel() -> Node3D:
	var pivot := Node3D.new()
	pivot.name = "FirstPersonViewmodel"
	var model := _instantiate(VIEWMODEL)
	if model == null:
		pivot.free()
		return null
	pivot.add_child(model)
	var players := model.find_children("*", "AnimationPlayer", true, false)
	if players.size() != 1:
		push_error("Animated viewmodel must contain one AnimationPlayer")
		pivot.free()
		return null
	animation_player = players[0] as AnimationPlayer
	for clip in ["draw_first", "fire", "reload"]:
		if not animation_player.has_animation(clip):
			push_error("Animated viewmodel is missing animation: " + clip)
			pivot.free()
			animation_player = null
			return null
	pivot.rotation.y = PI
	pivot.scale = Vector3.ONE * 0.025
	pivot.position = Vector3(0.23, -0.22, -0.38)
	return pivot


func _instantiate(path: String) -> Node3D:
	var scene := load(path) as PackedScene
	if scene == null:
		push_error("Could not load player model: " + path)
		return null
	var instance := scene.instantiate()
	if not instance is Node3D:
		push_error("Player model is not a 3D scene: " + path)
		instance.free()
		return null
	return instance


func _bounds(node: Node, parent_transform: Transform3D) -> AABB:
	var transform := parent_transform
	if node is Node3D:
		transform = parent_transform * node.transform
	var combined := AABB()
	var has_bounds := false
	if node is MeshInstance3D and node.mesh != null:
		for surface in range(node.mesh.get_surface_count()):
			var vertices: PackedVector3Array = node.mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]
			for vertex in vertices:
				var point := transform * vertex
				if not has_bounds:
					combined = AABB(point, Vector3.ZERO)
					has_bounds = true
				else:
					combined = combined.expand(point)
	for child in node.get_children():
		var child_bounds := _bounds(child, transform)
		if child_bounds.size != Vector3.ZERO:
			combined = combined.merge(child_bounds) if has_bounds else child_bounds
			has_bounds = true
	return combined
