extends SceneTree

var session: FireteamSession
var failed := false
var is_host := false
var joined_once := false
var rematched := false
var frames := 0
var match_end_frame := 0

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	is_host = "--test-host" in args
	session = FireteamSession.new()
	root.add_child(session)
	var combat = load("res://scripts/combat.gd").new()
	root.add_child(combat)
	combat.setup(session)
	session.session_error.connect(func(message: String):
		push_error(message)
		failed = true
		quit(1)
	)
	if is_host:
		if not session.host_game(0):
			quit(1)
			return
		print("FIRETEAM_HOST_READY ", session.get_state().port)
	else:
		var index := args.find("--test-port")
		if index == -1 or index + 1 >= args.size() or not session.join_game("127.0.0.1", int(args[index + 1])):
			quit(1)
			return

func _physics_process(_delta: float) -> bool:
	if session == null or failed:
		return false
	frames += 1
	if frames > 2400:
		push_error("Separate-process session test timed out: %s" % [session.get_state()])
		quit(1)
		return false
	var state := session.get_state()
	if state.phase == 1:
		joined_once = true
		if is_host:
			session.set_input(0, 0, 0, 0, false)
		elif rematched:
			if state.players[1].health != 100:
				push_error("Rematch health not restored")
				quit(1)
				return false
			session.stop_game()
			print("FIRETEAM_PROCESS_CLIENT PASS")
			quit(0)
		else:
			var moved: bool = state.players[1].position.z < -2
			var direction: Vector3 = state.players[0].position - state.players[1].position
			var yaw := atan2(direction.x, -direction.z) if moved else 0.0
			session.set_input(0, 0 if moved else 1, yaw, 0, moved and frames % 24 == 0)
	if state.phase == 2:
		if state.winner_id != 2 or state.players[0].health != 0:
			push_error("Incorrect match result")
			quit(1)
			return false
		if match_end_frame == 0:
			match_end_frame = frames
		if is_host and frames - match_end_frame > 30:
			session.restart_match()
			rematched = true
		elif not is_host:
			rematched = true
	if is_host and joined_once and rematched and state.phase == 0:
		session.stop_game()
		print("FIRETEAM_PROCESS_HOST PASS")
		quit(0)
	return false
