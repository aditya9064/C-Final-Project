extends Node3D

var session: FireteamSession
var camera: Camera3D
var player_visuals: Node3D
var menu: VBoxContainer
var status: Label
var address: LineEdit
var port: SpinBox
var host_button: Button
var join_button: Button
var rematch_button: Button
var yaw := 0.0
var pitch := 0.0
var fire_pending := false
var last_error := ""
var controls_active := false
var was_active := false

func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	# Keep Koushik's inspection and QA camera modes unchanged.
	if "--inspect" in args or "--view" in args or "--bench" in args:
		set_physics_process(false)
		set_process_input(false)
		return
	session = FireteamSession.new()
	add_child(session)
	session.session_error.connect(_on_error)
	session.fire_requested.connect(func(_id: int): status.text = "Fire requested; weapon integration is pending.")
	camera = Camera3D.new()
	camera.near = 0.05
	camera.fov = 75
	add_child(camera)
	camera.current = true
	camera.position = Vector3(-18, 1.63, 0)
	player_visuals = load("res://scripts/player_visuals.gd").new()
	add_child(player_visuals)
	if not player_visuals.setup(session, camera):
		push_error("Could not initialize player visuals")
		set_physics_process(false)
		set_process_input(false)
		return
	# The arena's free-fly camera must not consume game input.
	$Arena.set_process(false)
	$Arena.set_process_input(false)
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_build_menu()
	var port_index := args.find("--port")
	if port_index != -1 and port_index + 1 < args.size():
		port.value = int(args[port_index + 1])
	if "--host" in args:
		_start_host()
	elif "--join" in args:
		var index := args.find("--join")
		if index + 1 < args.size():
			address.text = args[index + 1]
			_start_join()

func _build_menu() -> void:
	var layer := CanvasLayer.new()
	add_child(layer)
	menu = VBoxContainer.new()
	menu.position = Vector2(20, 20)
	menu.custom_minimum_size = Vector2(420, 0)
	layer.add_child(menu)
	var title := Label.new()
	title.text = "Fireteam: C++ LAN prototype"
	menu.add_child(title)
	address = LineEdit.new()
	address.text = "127.0.0.1"
	address.placeholder_text = "Host LAN address"
	menu.add_child(address)
	port = SpinBox.new()
	port.min_value = 1
	port.max_value = 65535
	port.value = 27770
	menu.add_child(port)
	host_button = Button.new()
	host_button.text = "Host"
	host_button.pressed.connect(_start_host)
	menu.add_child(host_button)
	join_button = Button.new()
	join_button.text = "Join"
	join_button.pressed.connect(_start_join)
	menu.add_child(join_button)
	rematch_button = Button.new()
	rematch_button.text = "Rematch (host)"
	rematch_button.pressed.connect(func(): session.restart_match())
	menu.add_child(rematch_button)
	var leave := Button.new()
	leave.text = "Disconnect"
	leave.pressed.connect(func():
		session.stop_game()
		controls_active = false
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		menu.show()
	)
	menu.add_child(leave)
	var instructions := Label.new()
	instructions.text = "WASD: move | Mouse: look | Esc: menu\nClick: capture mouse / request fire | R: visual reload\nArena collision is authoritative on the host.\nWeapon animations are visual; combat is not yet integrated."
	menu.add_child(instructions)
	status = Label.new()
	status.position = Vector2(20, 440)
	layer.add_child(status)

func _start_host() -> void:
	if session.host_game(int(port.value)):
		last_error = ""
		yaw = PI / 2
		pitch = 0
		_capture()

func _start_join() -> void:
	if session.join_game(address.text, int(port.value)):
		last_error = ""
		yaw = -PI / 2
		pitch = 0
		_capture()

func _capture() -> void:
	controls_active = true
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	menu.hide()

func _on_error(message: String) -> void:
	last_error = message
	controls_active = false
	status.text = message
	menu.show()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _input(event: InputEvent) -> void:
	if session == null:
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		controls_active = false
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		menu.show()
	elif event is InputEventMouseMotion and controls_active:
		yaw = wrapf(yaw + event.relative.x * 0.002, -PI, PI)
		pitch = clampf(pitch - event.relative.y * 0.002, -1.5, 1.5)
	elif event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_R:
		if controls_active and session.get_state().phase == 1:
			player_visuals.play_reload()
	elif event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		if controls_active:
			fire_pending = true
		elif session.get_state().active and not menu.get_global_rect().has_point(event.position):
			_capture()

func _physics_process(_delta: float) -> void:
	if session == null:
		return
	var state := session.get_state()
	player_visuals.update_view(state)
	host_button.disabled = state.active
	join_button.disabled = state.active
	rematch_button.disabled = not state.host or state.phase != 2
	if not state.active:
		if was_active:
			controls_active = false
			fire_pending = false
			menu.show()
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		was_active = false
		status.text = last_error if not last_error.is_empty() else "Not connected. Host or join a LAN match."
		return
	was_active = true
	var move := Vector2.ZERO
	if controls_active and state.phase == 1:
		move.x = float(Input.is_physical_key_pressed(KEY_D)) - float(Input.is_physical_key_pressed(KEY_A))
		move.y = float(Input.is_physical_key_pressed(KEY_W)) - float(Input.is_physical_key_pressed(KEY_S))
		move = move.limit_length()
	if state.host or state.connected:
		session.set_input(move.x, move.y, yaw, pitch, fire_pending)
	if fire_pending and controls_active and state.phase == 1:
		player_visuals.play_fire()
	fire_pending = false
	if state.local_player_id != 0:
		var player: Dictionary = state.players[state.local_player_id - 1]
		camera.position = player.position + Vector3(0, 1.63, 0)
		camera.rotation = Vector3(pitch, -yaw, 0)
		status.text = "Player %d | HP %d | %s | Winner %d" % [
			state.local_player_id, player.health,
			["Lobby / waiting", "In game", "Match over"][state.phase], state.winner_id
		]
		if not last_error.is_empty():
			status.text += "\n" + last_error

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_WINDOW_FOCUS_OUT and session != null:
		controls_active = false
		fire_pending = false
		menu.show()
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
