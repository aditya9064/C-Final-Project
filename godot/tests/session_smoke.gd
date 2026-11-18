extends SceneTree

var errors: Array[String] = []
var fire_count := 0

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		errors.append(message)
		push_error(message)

func _wait_for(predicate: Callable, label: String) -> void:
	for _frame in range(240):
		if predicate.call():
			return
		await physics_frame
	_check(false, "Timed out: " + label)

func _run() -> void:
	var host := FireteamSession.new()
	var client := FireteamSession.new()
	root.add_child(host)
	root.add_child(client)
	host.session_error.connect(func(message: String): errors.append(message))
	client.session_error.connect(func(message: String): errors.append(message))
	host.fire_requested.connect(func(_id: int): fire_count += 1)

	var wall := StaticBody3D.new()
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(1, 4, 8)
	shape.shape = box
	wall.add_child(shape)
	root.add_child(wall)
	wall.position = Vector3(-16, 2, 0)
	await physics_frame
	_check(host.host_game(0), "Host failed to bind")
	_check(client.join_game("127.0.0.1", host.get_state().port), "Client failed to join")
	await _wait_for(func(): return client.get_state().phase == 1, "two-player match")
	_check(host.get_state().local_player_id == 1, "Host ID must be 1")
	_check(client.get_state().local_player_id == 2, "Client ID must be 2")

	for _frame in range(90):
		host.set_input(0, 1, PI / 2, 0.2, false)
		client.set_input(0, 1, 0, -0.1, false)
		await physics_frame
	host.set_input(0, 0, PI / 2, 0.2, true)
	client.set_input(0, 0, 0, -0.1, false)
	await _wait_for(func(): return fire_count > 0, "C++ fire integration signal")
	_check(host.get_state().players[0].position.x < -16.7, "Host passed through Godot collision")
	_check(host.get_state().players[0].position.x > -18, "Host movement did not run")
	_check(host.get_state().players[1].position.z < -1, "Client input did not move authoritative player")
	await _wait_for(func():
		return client.get_state().players[1].position.distance_to(host.get_state().players[1].position) < 0.01,
		"replicated positions")

	_check(host.damage_player(2, 100), "Host combat API failed")
	await _wait_for(func(): return client.get_state().phase == 2, "replicated match end")
	_check(client.get_state().winner_id == 1, "Wrong replicated winner")
	_check(host.restart_match(), "Rematch failed")
	await _wait_for(func(): return client.get_state().phase == 1, "replicated rematch")
	_check(client.get_state().players[1].health == 100, "Rematch did not restore health")
	client.stop_game()
	await _wait_for(func(): return host.get_state().phase == 0, "disconnect returns host to lobby")
	_check(host.get_state().players[1].id == 0, "Disconnected slot was not cleared")
	_check(client.join_game("127.0.0.1", host.get_state().port), "Reconnect failed")
	await _wait_for(func(): return client.get_state().phase == 1, "reconnected match")
	_check(client.get_state().players[1].health == 100, "Reconnect did not reset health")
	_check(client.get_state().players[1].position.distance_to(Vector3(18, 0, 0)) < 0.01, "Reconnect did not reset spawn")
	client.stop_game()
	host.stop_game()
	host.free()
	client.free()
	wall.free()
	if errors.is_empty():
		print("FIRETEAM_GODOT_SMOKE PASS: extension, IDs, movement, collision, replication, fire hook, winner, rematch, disconnect, reconnect")
		quit(0)
	else:
		quit(1)
