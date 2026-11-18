extends SceneTree

var session: FireteamSession
var failed := false
var is_host := false
var joined_once := false
var rematched := false
var frames := 0
var match_end_frame := 0
var fire_sent := false

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	is_host = "--test-host" in args
	session = FireteamSession.new()
	root.add_child(session)
	session.session_error.connect(func(message: String):
		push_error(message)
		failed = true
		quit(1)
	)
	if is_host:
		if not session.host_game(0):
			quit(1)
			return
		session.fire_requested.connect(func(id: int):
			if id == 2 and session.get_state().phase == 1:
				session.damage_player(2, 100)
		)
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
		push_error("Separate-process session test timed out")
		quit(1)
		return false
	var state := session.get_state()
	if state.phase == 1:
		joined_once = true
		if is_host:
			session.set_input(0, 0, 0, 0, false)
		elif not fire_sent:
			var moved: bool = state.players[1].position.z < -2
			session.set_input(0, 1 if not moved else 0, 0, 0, moved)
			fire_sent = moved
		elif rematched:
			if state.players[1].health != 100:
				push_error("Rematch health not restored")
				quit(1)
				return false
			session.stop_game()
			print("FIRETEAM_PROCESS_CLIENT PASS")
			quit(0)
	if state.phase == 2:
		if state.winner_id != 1 or state.players[1].health != 0:
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
