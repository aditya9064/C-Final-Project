extends SceneTree

var errors: Array[String] = []

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		errors.append(message)
		push_error(message)

func _mesh_count(node: Node) -> int:
	var total := 1 if node is MeshInstance3D and node.mesh != null else 0
	for child in node.get_children():
		total += _mesh_count(child)
	return total

func _check_mesh_lighting(node: Node) -> void:
	if node is MeshInstance3D and node.mesh != null:
		for surface in range(node.mesh.get_surface_count()):
			var arrays: Array = node.mesh.surface_get_arrays(surface)
			var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
			var normals = arrays[Mesh.ARRAY_NORMAL]
			var tangents = arrays[Mesh.ARRAY_TANGENT]
			_check(normals != null and normals.size() == vertices.size(), "Model is missing vertex normals")
			_check(tangents != null and tangents.size() == vertices.size() * 4, "Model is missing tangents")
	for child in node.get_children():
		_check_mesh_lighting(child)

func _check_clip_motion(player: AnimationPlayer, skeleton: Skeleton3D, clip: String) -> void:
	player.play(clip)
	player.seek(0, true)
	var initial: Array[Transform3D] = []
	for bone in range(skeleton.get_bone_count()):
		initial.append(skeleton.get_bone_pose(bone))
	player.seek(player.get_animation(clip).length * 0.4, true)
	var moved := false
	for bone in range(skeleton.get_bone_count()):
		if not skeleton.get_bone_pose(bone).is_equal_approx(initial[bone]):
			moved = true
			break
	_check(moved, clip + " must animate the viewmodel skeleton")

func _run() -> void:
	var game = load("res://scenes/game.tscn").instantiate()
	root.add_child(game)
	var client := FireteamSession.new()
	root.add_child(client)
	game.session.session_error.connect(func(message: String): errors.append(message))
	client.session_error.connect(func(message: String): errors.append(message))
	_check(game.session.host_game(0), "Game host failed")
	_check(client.join_game("127.0.0.1", game.session.get_state().port), "Game client failed")
	for _frame in range(240):
		if client.get_state().phase == 1:
			break
		await physics_frame
	_check(client.get_state().phase == 1, "Game scene did not start a match")
	await physics_frame
	await physics_frame
	var visuals: Node = game.player_visuals
	_check(visuals.soldiers.size() == 2, "Both team soldier models must load")
	_check(visuals.viewmodel != null and visuals.animation_player != null, "Animated viewmodel must load")
	if visuals.soldiers.size() == 2 and visuals.viewmodel != null:
		for i in range(2):
			_check(_mesh_count(visuals.soldiers[i]) > 0, "Soldier model is empty")
			_check_mesh_lighting(visuals.soldiers[i])
			var soldier_skeletons: Array[Node] = visuals.soldiers[i].find_children("*", "Skeleton3D", true, false)
			_check(soldier_skeletons.size() == 1, "Soldier must have a skeleton")
			if soldier_skeletons.size() == 1:
				for clip in ["idle", "jog"]:
					_check(visuals.soldier_animations[i].get_animation(clip).loop_mode != Animation.LOOP_NONE,
						clip + " must loop")
					_check_clip_motion(visuals.soldier_animations[i], soldier_skeletons[0] as Skeleton3D, clip)
				visuals.soldier_animations[i].play("idle")
			var bounds: AABB = visuals._bounds(visuals.soldiers[i], Transform3D.IDENTITY)
			_check(absf(bounds.position.y) < 0.01 and absf(bounds.size.y - 1.8) < 0.01,
				"Soldier must be 1.8 m tall with feet at the player's position")
			var helmet := visuals.soldiers[i].find_child("helmet_helmet_0", true, false) as MeshInstance3D
			_check(helmet != null, "Soldier helmet is missing")
			if helmet != null:
				var helmet_bounds: AABB = helmet.get_aabb()
				var head_center: Vector3 = helmet.global_transform * helmet_bounds.get_center()
				var body_origin: Vector3 = visuals.soldiers[i].get_parent().global_position
				_check(head_center.y > body_origin.y + 1.4 and absf(head_center.x - body_origin.x) < 0.4,
					"Soldier head must be upright above the body")
		_check(_mesh_count(visuals.viewmodel) > 0, "Animated viewmodel is empty")
		_check_mesh_lighting(visuals.viewmodel)
		var skeletons: Array[Node] = visuals.viewmodel.find_children("*", "Skeleton3D", true, false)
		_check(skeletons.size() == 1, "Viewmodel must have a skeleton")
		if skeletons.size() == 1:
			for clip in ["draw_first", "fire", "reload"]:
				_check_clip_motion(visuals.animation_player, skeletons[0] as Skeleton3D, clip)
		_check(visuals.viewmodel.visible, "Host must see the first-person viewmodel")
		_check(not game.session.get_node("HostPlayer").visible,
			"Local soldier must be hidden in first person")
		_check(game.session.get_node("ClientPlayer").visible,
			"Remote soldier must be visible")
		_check(visuals.viewmodel.get_parent() == game.camera,
			"Viewmodel must follow the first-person camera")
		var dead_state: Dictionary = game.session.get_state().duplicate(true)
		dead_state.players[0].health = 0
		visuals.update_view(dead_state)
		_check(not visuals.viewmodel.visible, "Viewmodel must be hidden when the local player is dead")
		visuals.play_fire()
		_check(not visuals.animation_player.is_playing(), "Hidden viewmodel must not play fire")
		visuals.update_view(game.session.get_state())
		_check(visuals.viewmodel.visible and visuals.animation_player.current_animation == "draw_first",
			"Viewmodel must play its draw animation when shown")
		var movement_state: Dictionary = game.session.get_state().duplicate(true)
		visuals.update_view(movement_state, 0.016)
		movement_state.players[1].position.x += 0.1
		visuals.update_view(movement_state, 0.016)
		_check(visuals.soldier_animations[1].current_animation == "jog",
			"Remote position changes must start the locomotion animation")
		visuals.update_view(movement_state, 0.1)
		_check(visuals.soldier_animations[1].current_animation == "jog",
			"Locomotion must persist briefly between snapshots")
		visuals.update_view(movement_state, 0.11)
		_check(visuals.soldier_animations[1].current_animation == "idle",
			"Stopping must return the soldier to idle")
		movement_state.players[1].id = 0
		visuals.update_view(movement_state, 0.016)
		movement_state.players[1].id = 2
		movement_state.players[1].position.x += 20.0
		visuals.update_view(movement_state, 0.016)
		_check(visuals.soldier_animations[1].current_animation == "idle",
			"A newly connected player must not jog because of a spawn jump")
		movement_state.players[1].position.x += 0.1
		visuals.update_view(movement_state, 0.016)
		movement_state.players[1].health = 0
		visuals.update_view(movement_state, 0.016)
		_check(visuals.soldier_animations[1].current_animation == "idle",
			"An eliminated player must stop jogging")
	var client_camera := Camera3D.new()
	root.add_child(client_camera)
	var client_visuals = load("res://scripts/player_visuals.gd").new()
	root.add_child(client_visuals)
	_check(client_visuals.setup(client, client_camera), "Client visuals failed to load")
	await physics_frame
	client_visuals.update_view(client.get_state())
	if client_visuals.viewmodel != null:
		_check(client_visuals.viewmodel.visible, "Client must see the first-person viewmodel")
		_check(client_visuals.viewmodel.get_parent() == client_camera,
			"Client viewmodel must follow its first-person camera")
	_check(client.get_node("HostPlayer").visible, "Client must see the remote host")
	_check(not client.get_node("ClientPlayer").visible, "Client must not see its own soldier")
	client.hide()
	var hud: CanvasLayer = game.hud
	hud.update_state(game.session.get_state(), false)
	_check(not hud.visible, "HUD must hide when controls are inactive")
	hud.update_state(game.session.get_state(), true)
	_check(hud.visible and hud.team_label.text == "TEAL", "Host HUD must show the teal team")
	_check(hud.health_bar.value == 100 and hud.health_label.text == "100",
		"HUD must show replicated local health")
	_check(hud.phase_label.text == "IN GAME" and hud.crosshair.visible,
		"HUD must show the match phase and crosshair")
	hud.update_state(client.get_state(), true)
	_check(hud.team_label.text == "ORANGE", "Client HUD must show the orange team")
	var hud_dead_state: Dictionary = client.get_state().duplicate(true)
	hud_dead_state.players[1].health = 0
	hud_dead_state.phase = 2
	hud_dead_state.winner_id = 1
	hud.update_state(hud_dead_state, true)
	_check(hud.health_bar.value == 0 and not hud.crosshair.visible,
		"HUD must show elimination and hide the crosshair")
	_check(hud.phase_label.text == "MATCH OVER" and hud.result_label.text == "TEAL WINS",
		"HUD must show the authoritative match winner")
	game.yaw = PI / 2
	game._capture()
	var press := InputEventKey.new()
	press.physical_keycode = KEY_W
	press.keycode = KEY_W
	press.pressed = true
	Input.parse_input_event(press)
	for _frame in range(40):
		await physics_frame
	var release := InputEventKey.new()
	release.physical_keycode = KEY_W
	release.keycode = KEY_W
	release.pressed = false
	Input.parse_input_event(release)
	var player: Dictionary = game.session.get_state().players[0]
	_check(player.position.x > -17.5, "WASD input did not move out of the arena spawn")
	_check(absf(game.camera.position.y - player.position.y - 1.63) < 0.01, "Wrong first-person eye height")
	_check(game.camera.position.distance_to(player.position + Vector3(0, 1.63, 0)) < 0.2, "Camera did not follow C++ player")
	var mouse := InputEventMouseMotion.new()
	mouse.relative = Vector2(20, -20)
	game._input(mouse)
	await physics_frame
	_check(game.pitch > 0, "Mouse input did not update pitch")
	_check(game.camera.rotation.x > 0, "Camera did not use first-person aim")
	_check(not game.menu.visible, "Menu should be hidden while playing")
	var reload := InputEventKey.new()
	reload.keycode = KEY_R
	reload.pressed = true
	game._input(reload)
	_check(visuals.animation_player.current_animation == "reload", "R must play the visual reload")
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	game._input(click)
	await physics_frame
	await physics_frame
	_check(visuals.animation_player.current_animation == "fire", "Fire input must play the fire animation")
	var args := OS.get_cmdline_user_args()
	var capture_index := args.find("--capture")
	if capture_index != -1 and capture_index + 1 < args.size():
		await RenderingServer.frame_post_draw
		var image := root.get_texture().get_image()
		_check(image.save_png(args[capture_index + 1]) == OK, "Could not save visual test")
	client.stop_game()
	game.session.stop_game()
	await physics_frame
	await physics_frame
	_check(game.menu.visible and not game.controls_active, "Disconnect did not restore the menu")
	client.free()
	client_visuals.free()
	client_camera.free()
	game.free()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	if errors.is_empty():
		print("FIRETEAM_GAME_SMOKE PASS: real arena, keyboard, mouse, first-person camera, session UI")
		quit(0)
	else:
		quit(1)
