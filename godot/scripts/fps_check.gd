extends Node3D
# First-person gate + demonstration for Player #6. Spawns a LOCAL player (first
# person) and a REMOTE player (full body) in the real arena, drives real input,
# asserts the FP architecture, and captures frames FROM THE LOCAL FP CAMERA.
#   Windowed:  Godot --path godot --rendering-driver vulkan --resolution 1280x720 \
#              res://scenes/fps_check.tscn -- --out <dir>
# Verdict line: "FPS_CHECK PASS/FAIL (<n> checks)".

var _local: Node
var _remote: Node
var _hud: Node
var _world: Node
var _out := "user://review"
var _results: Array = []

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		if a[i] == "--out" and i + 1 < a.size():
			_out = a[i + 1]
	DirAccess.make_dir_recursive_absolute(_out)
	FireteamInput.ensure_actions()

	var arena: Node = load("res://scenes/arena.tscn").instantiate()
	if "qa_mode" in arena:
		arena.set("qa_mode", false)
	add_child(arena)

	_world = GameWorldNode.new(); add_child(_world); _world.start_match()
	# Open lane on the teal half (centre cover is near x=0): local faces +X down an
	# open stretch; the remote full-body player stands ahead-left, in view.
	_local = _spawn(0, Vector3(-16.0, 1.2, 0.0), true)     # first person
	_remote = _spawn(1, Vector3(-10.5, 1.2, 1.2), false)   # full-body "other player"
	_hud = load("res://scenes/hud.tscn").instantiate(); add_child(_hud)
	_hud.bind(_local, _world)
	var auth := RespawnAuthority.new(); add_child(auth); auth.watch(_local)
	_run()

func _spawn(team: int, pos: Vector3, local: bool) -> Node:
	var p: Node = load("res://scenes/player.tscn").instantiate()
	p.team_id = team
	p.is_local = local
	p.position = pos
	add_child(p)
	return p

func _chk(name: String, cond: bool) -> void:
	_results.append(cond)
	print("  ", "ok  " if cond else "FAIL", name)

func _all_shadow_only(n: Node) -> bool:
	if n is GeometryInstance3D:
		if (n as GeometryInstance3D).cast_shadow != \
				GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY:
			return false
	for c in n.get_children():
		if not _all_shadow_only(c):
			return false
	return true

func _shot_aligned() -> bool:
	var cam: Camera3D = _local._fp_cam
	if cam == null:
		return false
	var center := get_viewport().get_visible_rect().size * 0.5
	var origin := cam.project_ray_origin(center)
	var dir := cam.project_ray_normal(center)
	var q := PhysicsRayQueryParameters3D.create(origin, origin + dir * 200.0)
	q.exclude = [_local.get_rid()]
	var hit := get_world_3d().direct_space_state.intersect_ray(q)
	if hit.is_empty():
		return false
	# The crosshair is screen-centre; the hit must project back onto the centre.
	return cam.unproject_position(hit.position).distance_to(center) < 3.0

func _wait(frames: int) -> void:
	for i in range(frames):
		await get_tree().process_frame

func _cap(name: String) -> void:
	await RenderingServer.frame_post_draw
	var tex := get_viewport().get_texture()
	if tex != null:
		var img := tex.get_image()
		if img != null:
			img.save_png("%s/fps_%s.png" % [_out, name])
	print("SHOT ", name)

func _press(action: String, frames := 2) -> void:
	Input.action_press(action)
	await _wait(frames)
	Input.action_release(action)

func _run() -> void:
	await _wait(35)   # let both players settle on the floor

	# ---- structural assertions ----
	_chk("local has current FP camera", _local._fp_cam != null and _local._fp_cam.current)
	_chk("camera at eye height", _local._cam_arm != null \
		and absf(_local._cam_arm.position.y - 0.62) < 0.01)
	_chk("local body + hand gun self-hidden (shadows only)", _all_shadow_only(_local._model))
	_chk("viewmodel present and visible", _local._vm != null and _local._vm.visible)
	_chk("remote player body is visible (not self-hidden)", not _all_shadow_only(_remote._model))
	await _cap("idle")

	# ---- walk + sprint (held actions work with synthetic input) ----
	var start: Vector3 = _local.global_position
	Input.action_press("move_forward")
	await _wait(20)
	var walk_spd: float = _local.get_planar_velocity().length()
	await _cap("move")
	Input.action_press("run")
	await _wait(18)
	var run_spd: float = _local.get_planar_velocity().length()
	Input.action_release("run")
	Input.action_release("move_forward")
	_chk("moved under WASD", (_local.global_position - start).length() > 0.5)
	_chk("sprint faster than walk", run_spd > walk_spd + 0.5)
	await _wait(10)

	# ---- fire through the REAL input path (LMB -> _read_input -> request_fire) ----
	# Await a fresh frame boundary BEFORE pressing so the player's _process polls the
	# just_pressed on the same frame it is latched (hip fire, so the flash is in view).
	var pre_ammo: int = _local.get_ammo()
	await get_tree().process_frame
	Input.action_press("fire")
	# Capture on the exact frame the flash is live (fire may land a frame later).
	var flash_up := false
	for i in range(6):
		await get_tree().process_frame
		if _local._flash != null and _local._flash.visible:
			flash_up = true
			await RenderingServer.frame_post_draw
			var tex := get_viewport().get_texture()
			if tex != null and tex.get_image() != null:
				tex.get_image().save_png("%s/fps_fire.png" % _out)   # hip-fire, flash live
			break
	print("SHOT fire")
	Input.action_release("fire")
	print("  [debug] ammo ", pre_ammo, " -> ", _local.get_ammo())
	_chk("LMB fired through input path (ammo -1)", _local.get_ammo() == pre_ammo - 1)
	_chk("muzzle flash visible on the shot frame", flash_up)
	_chk("shot leaves along the crosshair", _shot_aligned())
	await _wait(10)

	# ---- aim (ADS): optic rides under the crosshair, target still visible ----
	Input.action_press("aim")
	await _wait(22)
	_chk("aim pulls weapon in + zooms", _local._ads > 0.8 and _local._fp_cam.fov < 70.0)
	await _cap("aim")
	Input.action_release("aim")
	await _wait(10)

	# ---- reload: fire a couple more, then reload via request (same call R binds to) ----
	_local.request_reload()
	await _wait(2)
	_chk("reload started", _local.is_reloading())
	await _wait(12)
	await _cap("reload")               # weapon dips; HUD reload bar shows
	await _wait(220)                   # wait out the full reload timer
	_chk("reload refilled the magazine", _local.get_ammo() == _local.get_magazine_size())

	# ---- jump ----
	_local.request_jump()
	await _wait(5)
	_chk("jump leaves the floor", not _local.is_on_floor())
	await _wait(55)                    # land + settle
	_chk("landed back on floor", _local.is_on_floor())

	# ---- wall clip: stand the player right in front of the central pillar ----
	_local.global_position = Vector3(-2.2, 1.2, 0.0)
	_local._yaw = atan2(-1.0, 0.0)     # look +X into the wall
	await _wait(14)
	await _cap("wallclip")

	# ---- death hides the viewmodel; authority respawn restores it ----
	_local.apply_damage(100)
	await _wait(10)
	_chk("dead -> viewmodel hidden", _local._vm != null and not _local._vm.visible)
	await _cap("death")
	# RespawnAuthority revives the local player; wait it out, then confirm restore.
	var waited := 0
	while _local.get_health() <= 0 and waited < 600:
		await get_tree().process_frame
		waited += 1
	await _wait(20)
	_chk("respawn restores the viewmodel", _local._vm != null and _local._vm.visible)

	var passed := not _results.has(false)
	print("FPS_CHECK ", "PASS" if passed else "FAIL", " (", _results.size(), " checks)")
	get_tree().quit(0 if passed else 1)
