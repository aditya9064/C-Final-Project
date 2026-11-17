extends Node3D
# Respawn gate for Player #6. Runs the REAL host authority through the REAL
# respawn countdown, with the player first moved OFF its spawn and the camera
# turned away, so "back at spawn" and "camera reset" can't pass trivially.
#   --team 0|1     which team's local player
#   --authority 0  omit the authority node -> player must STAY dead (req 8)
#   --out <dir>
# Prints RESPAWN_CHECK PASS/FAIL. Windowed, ~7 s (5 s countdown + checks).

const SPAWN := {0: Vector3(-18, 1.2, 0), 1: Vector3(18, 1.2, 0)}

var _out := "user://review"
var _team := 0
var _authority := true
var _player: Node
var _world: Node
var _hud: Node
var _auth: Node
var _spawn := Vector3.ZERO
var _state := 0
var _elapsed := 0.0
var _mark := 0.0
var _rem_a := -1.0
var _txt_a := ""
var _results: Array = []

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		if a[i] == "--out" and i + 1 < a.size(): _out = a[i + 1]
		elif a[i] == "--team" and i + 1 < a.size(): _team = int(a[i + 1])
		elif a[i] == "--authority" and i + 1 < a.size(): _authority = a[i + 1] != "0"
	DirAccess.make_dir_recursive_absolute(_out)
	FireteamInput.ensure_actions()
	_spawn = SPAWN[_team]

	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.16, 0.17, 0.19)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.5, 0.52, 0.55); env.ambient_light_energy = 0.4
	var we := WorldEnvironment.new(); we.environment = env; add_child(we)
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-50), deg_to_rad(-40), 0); add_child(sun)
	var fb := StaticBody3D.new(); var cs := CollisionShape3D.new()
	var bx := BoxShape3D.new(); bx.size = Vector3(60, 1, 60); cs.shape = bx
	fb.add_child(cs); fb.position = Vector3(0, -0.5, 0)
	var mi := MeshInstance3D.new(); var pm := PlaneMesh.new(); pm.size = Vector2(60, 60)
	mi.mesh = pm; mi.position = Vector3(0, 0.5, 0); fb.add_child(mi); add_child(fb)

	_world = GameWorldNode.new(); add_child(_world); _world.start_match()
	_player = load("res://scenes/player.tscn").instantiate()
	_player.team_id = _team; _player.is_local = true; _player.position = _spawn
	add_child(_player)
	_hud = load("res://scenes/hud.tscn").instantiate(); add_child(_hud)
	_hud.bind(_player, _world)
	if _authority:
		_auth = RespawnAuthority.new(); add_child(_auth); _auth.watch(_player)

func _check(name: String, cond: bool) -> void:
	_results.append(cond)
	print("  ", "ok  " if cond else "FAIL", name)

func _cam_fwd_x() -> float:
	var arm = _player._cam_arm
	if arm == null: return 0.0
	var f: Vector3 = -arm.global_transform.basis.z
	return Vector2(f.x, f.z).normalized().x

func _finish(ok: bool) -> void:
	var img := get_viewport().get_texture().get_image()
	img.save_png(_out + "/respawn_%s_team%d.png" % ["auth" if _authority else "noauth", _team])
	var pass_all := not _results.has(false)
	print("RESPAWN_CHECK ", "PASS" if (pass_all and ok) else "FAIL",
		" team=", _team, " authority=", _authority, " (", _results.size(), " checks)")
	get_tree().quit(0 if (pass_all and ok) else 1)

func _process(delta: float) -> void:
	_elapsed += delta
	match _state:
		0:
			if _elapsed > 0.4:
				Input.action_press("move_forward")   # drive OFF the spawn
				_state = 1
		1:
			if _elapsed > 1.0:
				Input.action_release("move_forward")
				_player._yaw += 2.0                   # turn the camera away from inward
				var moved: float = (_player.global_position - _spawn).length()
				_check("moved off spawn before death", moved > 1.0)
				_player.apply_damage(100)             # combat lands the kill
				_state = 2
		2:  # wait for death to register, then let the HUD catch up a few frames
			if _player.is_awaiting_respawn():
				_rem_a = _player.get_respawn_remaining()
				_mark = _elapsed
				_state = 21
			elif _elapsed > 2.0:
				_check("death detected -> awaiting", false)
				_finish(false)
		21:
			if _elapsed > _mark + 0.15:
				_check("HUD overlay up + crosshair hidden while dead",
					_hud._respawn_box.visible and not _hud._crosshair.visible)
				_mark = _elapsed
				_txt_a = _hud._respawn_sub.text   # "Respawning in N"
				_state = 3
		3:  # let >1s pass, then compare both the getter and the HUD countdown TEXT
			if _elapsed > _mark + 1.2:
				var txt_b: String = _hud._respawn_sub.text
				_check("respawn countdown decreasing (getter)",
					_player.get_respawn_remaining() < _rem_a - 0.2)
				_check("HUD countdown text counts down",
					_txt_a.begins_with("Respawning in") and txt_b.begins_with("Respawning in")
					and _txt_a != txt_b)
				_state = 4 if _authority else 5
		4:  # authority path: wait until revived, then wait for the body to settle
			if _player.get_health() > 0 and not _player.is_awaiting_respawn():
				_mark = _elapsed
				_state = 41
			elif _elapsed > 9.0:
				_check("authority respawned within countdown+margin", false)
				_finish(false)
		41:  # wait until the body is grounded and no longer moving
			if _player.is_on_floor() and _player.get_planar_velocity().length() < 0.2:
				_mark = _elapsed
				_state = 42
			elif _elapsed > _mark + 3.0:
				_check("player grounded after respawn", false)
				_finish(false)
		42:  # let the jump->idle crossfade finish, then assert the full reset at idle
			if _elapsed > _mark + 0.4:
				var node: String = _player._playback.get_current_node()
				print("  [debug] anim after settle=", node, " on_floor=", _player.is_on_floor())
				var at_spawn: float = (_player.global_position - _spawn).length()
				var inward := 1.0 if _team == 0 else -1.0
				_check("respawned at team spawn", at_spawn < 1.5)
				_check("health restored to full", _player.get_health() == _player.get_max_health())
				_check("ammo refilled", _player.get_ammo() == _player.get_magazine_size())
				_check("camera reset to face inward", _cam_fwd_x() * inward > 0.5)
				_check("animation settled to idle (not death/jump)", node == "idle")
				_check("HUD overlay cleared + crosshair back",
					not _hud._respawn_box.visible and _hud._crosshair.visible)
				_finish(true)
		5:  # NO-authority path: player must STAY dead past the countdown (req 8)
			if _elapsed > 7.0:
				_check("no authority -> stays dead past countdown",
					_player.get_health() == 0 and _player.is_ready_to_respawn())
				_check("HUD shows 'waiting for respawn'",
					_hud._respawn_sub.text == "waiting for respawn")
				_finish(true)
