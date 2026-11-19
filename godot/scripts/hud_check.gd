extends Node3D
# HUD #7 gate: builds a local player + match world + HUD, then ASSERTS the HUD
# formats the live C++ state (not eyeballed): ammo after firing, health bar after
# damage, the match timer advancing, and the respawn overlay on death. Finally it
# exercises the combat SEAM (hit marker + kill feed) and saves a labelled demo
# frame. Prints HUD_CHECK PASS/FAIL. Windowed:
#   godot --path godot --rendering-driver vulkan --resolution 1280x720 \
#         --quit-after 160 res://scenes/hud_check.tscn -- --out <dir>

var _out := "user://review"
var _frames := 0
var _player: Node
var _world: Node
var _hud: Node
var _t0 := 0.0
var _team := 0
var _results: Array = []

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		if a[i] == "--out" and i + 1 < a.size():
			_out = a[i + 1]
		elif a[i] == "--team" and i + 1 < a.size():
			_team = int(a[i + 1])
	DirAccess.make_dir_recursive_absolute(_out)
	FireteamInput.ensure_actions()

	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.16, 0.17, 0.19)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.5, 0.52, 0.55); env.ambient_light_energy = 0.4
	var we := WorldEnvironment.new(); we.environment = env; add_child(we)
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-50), deg_to_rad(-40), 0); add_child(sun)
	var fb := StaticBody3D.new(); var cs := CollisionShape3D.new()
	var bx := BoxShape3D.new(); bx.size = Vector3(12, 1, 12); cs.shape = bx
	fb.add_child(cs); fb.position = Vector3(0, -0.5, 0)
	var mi := MeshInstance3D.new(); var pm := PlaneMesh.new(); pm.size = Vector2(12, 12)
	mi.mesh = pm; mi.position = Vector3(0, 0.5, 0); fb.add_child(mi); add_child(fb)
	var cam := Camera3D.new(); add_child(cam)
	cam.position = Vector3(0, 1.4, 4.0); cam.look_at(Vector3(0, 1.0, 0)); cam.current = true

	_world = GameWorldNode.new(); add_child(_world); _world.start_match()
	_player = load("res://scenes/player.tscn").instantiate()
	_player.team_id = _team; _player.is_local = false; _player.position = Vector3(0, 1.0, 0)
	add_child(_player)
	_hud = load("res://scenes/hud.tscn").instantiate(); add_child(_hud)
	_hud.bind(_player, _world)

func _check(name: String, cond: bool) -> void:
	_results.append(cond)
	print("  ", "ok  " if cond else "FAIL", name)

func _process(_d: float) -> void:
	_frames += 1
	match _frames:
		18:
			_t0 = _world.get_match_time()
			_check("team chip reads correct team",
				_hud._team_chip.text == ("ORANGE" if _team == 1 else "TEAL"))
		20:
			for i in 3:
				_player.request_fire()          # 30 -> 27
		22:
			_check("ammo: 3 shots -> 27 and label matches",
				_player.get_ammo() == 27 and _hud._ammo_label.text == "27 / 30")
			_hud.flash_hitmarker()              # flash while ALIVE
		24:
			_check("hit marker visible while alive", _hud._hitmarker.modulate.a > 0.0)
			_frame_shot("hud_hitmarker")        # capture it before it fades
		26:
			_player.request_reload()            # exercise the reload bar
		28:
			_check("reload bar shows while reloading",
				_player.is_reloading() and _hud._reload_bar.visible)
			_player.apply_damage(40)            # 100 -> 60
		30:
			_check("health: -40 -> 60, bar + label match",
				_player.get_health() == 60 and int(round(_hud._hp_bar.value)) == 60
				and _hud._hp_label.text == "60")
		40:
			_check("match timer advanced", _world.get_match_time() > _t0)
			_player.apply_damage(100)           # lethal
		50:
			_check("death -> respawn overlay visible",
				_player.is_awaiting_respawn() and _hud._respawn_box.visible)
			_hud.push_feed("TEAL  >  ORANGE", 0)
		56:
			_frame_shot("hud_seam_demo")
			var ok: bool = not _results.has(false) and _results.size() == 7
			print("HUD_CHECK ", "PASS" if ok else "FAIL",
				" team=", _team, " (", _results.size(), " checks)")
			get_tree().quit(0 if ok else 1)

func _frame_shot(name: String) -> void:
	var img := get_viewport().get_texture().get_image()
	img.save_png(_out + "/" + name + ".png")
