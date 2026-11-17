extends Node3D
# Headless gameplay smoke gate for Increment 1 (issue #6 extension skeleton).
# Builds a floor + a GameWorldNode + a PlayerBody (all C++ extension classes),
# runs the physics loop, and asserts: extension loaded, player spawns at its
# team X, gravity settles it on the floor, movement intent moves it, and the
# match phase transitions. Prints GAMEPLAY_SMOKE PASS/FAIL and quits.

var _gw: Node = null
var _p: CharacterBody3D = null
var _frames := 0
var _ok := true
var _rest_y := 0.0
var _peak_y := -999.0
const SPAWN_X := -18.0  # teal side, from build_arena.gd

func _fail(m: String) -> void:
	_ok = false
	print("  FAIL: ", m)

func _ready() -> void:
	if not ClassDB.class_exists("PlayerBody") or not ClassDB.class_exists("GameWorldNode"):
		print("GAMEPLAY_SMOKE FAIL: extension classes not registered (did not load)")
		get_tree().quit(1); return

	# Floor: top surface at y = 0.
	var floor_body := StaticBody3D.new()
	var fcs := CollisionShape3D.new()
	var fbox := BoxShape3D.new(); fbox.size = Vector3(80, 1, 80)
	fcs.shape = fbox
	floor_body.add_child(fcs)
	floor_body.position = Vector3(0, -0.5, 0)
	add_child(floor_body)

	# Match flow (C++ GameState).
	_gw = ClassDB.instantiate("GameWorldNode")
	add_child(_gw)
	print("SMOKE_PHASE_BEFORE: ", _gw.get_phase(), " (0=Lobby)")
	_gw.start_match()

	# Player body (C++ adapter owning fireteam::Player), dropped 3 m above floor.
	_p = ClassDB.instantiate("PlayerBody")
	var pcs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new(); cap.height = 1.8; cap.radius = 0.35
	pcs.shape = cap
	_p.add_child(pcs)
	add_child(_p)
	_p.setup(0, Vector3(SPAWN_X, 3.0, 0.0))  # 0 = teal

func _physics_process(_delta: float) -> void:
	if _p == null: return
	_frames += 1

	if _frames == 50:
		var pos: Vector3 = _p.global_position
		print("SMOKE_SETTLE: pos=", pos)
		if abs(pos.x - SPAWN_X) > 0.01: _fail("spawn X drifted")
		if pos.y > 2.0: _fail("gravity did not settle player on floor")
		if pos.y < 0.3: _fail("player fell through floor")
		_rest_y = pos.y
		_p.request_jump()  # test the jump path

	# Track the jump arc over the frames right after the jump request.
	if _frames > 50 and _frames <= 75:
		_peak_y = max(_peak_y, _p.global_position.y)
	if _frames == 76:
		print("SMOKE_JUMP: rest_y=", _rest_y, " peak_y=", _peak_y)
		if _peak_y <= _rest_y + 0.2: _fail("jump did not raise the player")
		_p.set_move_intent(Vector2(1, 0))  # now drive +X

	if _frames == 120:
		var pos2: Vector3 = _p.global_position
		print("SMOKE_MOVE: pos=", pos2)
		if pos2.x <= SPAWN_X + 0.3: _fail("movement intent did not move player")
		if _gw.get_phase() != 1: _fail("match phase is not InGame")
		print("SMOKE_STATE: ammo=", _p.get_ammo(), " health=", _p.get_health(),
			" team=", _p.get_team(), " phase=", _gw.get_phase(), " match_time=", _gw.get_match_time())
		print("GAMEPLAY_SMOKE ", "PASS" if _ok else "FAIL")
		get_tree().quit(0 if _ok else 1)
