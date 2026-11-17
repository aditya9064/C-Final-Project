extends PlayerBody
# Player #6 rig/controller (Koushik). Extends the C++ PlayerBody: it adds the
# VISUALS and INPUT only -- it never defines _physics_process, so the C++ side
# keeps doing movement/collision. Input is read in _process and pushed into the
# model via set_move_intent/set_running/request_* ; the C++ tick consumes it.
#
# Builds: the selected character model, an AnimationTree state machine over the
# 12 clips, a third-person spring-arm camera (local player only), and the SMG
# parented to the hand_r bone.

@export var team_id: int = 0       # 0 = teal (male), 1 = orange (female)
@export var is_local: bool = true  # local player gets the camera + input
@export var forced_anim: String = ""  # non-empty pins an animation (headless review only)
@export var spawn_face_inward: bool = true  # false = keep +Z facing (showcase renders)

const CHAR_GLB := {
	0: "res://assets/models/characters/male_tailored.glb",
	1: "res://assets/models/characters/female_tailored.glb",
}
const WEAPON_GLB := {
	0: "res://assets/models/weapons/smg_teal.glb",
	1: "res://assets/models/weapons/smg_orange.glb",
}
const SKELETON_PATH := "Armature/Skeleton3D"
const HAND_BONE := "hand_r"
const MODEL_Y := -0.9           # feet to capsule centre
const LOCOMOTION := ["idle", "walk", "run", "jog", "jump", "jump_start",
	"jump_land", "aim", "aim_idle", "fire", "reload", "death"]

var _model: Node3D
var _tree: AnimationTree
var _playback: AnimationNodeStateMachinePlayback
var _cam_arm: SpringArm3D
var _yaw := 0.0
var _pitch := -0.2
var _aiming := false
var _fire_timer := 0.0
var _mouse_sens := 0.0025

func _ready() -> void:
	setup(team_id, global_position)
	_build_model()
	_build_anim_tree()
	_attach_weapon()
	# Face the arena centre at spawn (teal at -X faces +X, orange at +X faces -X).
	if spawn_face_inward:
		var inward := 1.0 if team_id == 0 else -1.0
		_yaw = atan2(inward, 0.0)
		set_yaw(_yaw)
		if _model:
			_model.rotation.y = _yaw
	if is_local:
		_build_camera()
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _build_model() -> void:
	var ps := load(CHAR_GLB.get(team_id, CHAR_GLB[0])) as PackedScene
	_model = ps.instantiate()
	_model.position = Vector3(0, MODEL_Y, 0)
	add_child(_model)

func _build_anim_tree() -> void:
	var ap := _model.get_node("AnimationPlayer") as AnimationPlayer
	var sm := AnimationNodeStateMachine.new()
	var i := 0
	for clip in LOCOMOTION:
		if not ap.has_animation(clip):
			continue
		var node := AnimationNodeAnimation.new()
		node.animation = clip
		sm.add_node(clip, node, Vector2(200 * (i % 4), 120 * (i / 4)))
		i += 1
	# Full-mesh transitions so travel() always has a direct, blended path.
	for a in LOCOMOTION:
		for b in LOCOMOTION:
			if a == b or not ap.has_animation(a) or not ap.has_animation(b):
				continue
			var t := AnimationNodeStateMachineTransition.new()
			t.xfade_time = 0.15
			t.switch_mode = AnimationNodeStateMachineTransition.SWITCH_MODE_IMMEDIATE
			t.advance_mode = AnimationNodeStateMachineTransition.ADVANCE_MODE_DISABLED
			sm.add_transition(a, b, t)
	_tree = AnimationTree.new()
	_tree.tree_root = sm
	add_child(_tree)
	_tree.anim_player = _tree.get_path_to(ap)
	_tree.active = true
	_playback = _tree.get("parameters/playback")
	_playback.start("idle")

func _attach_weapon() -> void:
	var skel := _model.get_node(SKELETON_PATH) as Skeleton3D
	if skel == null:
		return
	var att := BoneAttachment3D.new()
	att.bone_name = HAND_BONE
	skel.add_child(att)
	var wps := load(WEAPON_GLB.get(team_id, WEAPON_GLB[0])) as PackedScene
	var smg := wps.instantiate()
	# Starting offset; tuned by screenshot. Grip sits in the palm, barrel forward.
	smg.position = Vector3(0.0, 0.02, 0.0)
	smg.rotation = Vector3(0, 0, 0)
	att.add_child(smg)

func _build_camera() -> void:
	_cam_arm = SpringArm3D.new()
	_cam_arm.position = Vector3(0, 1.4, 0)
	_cam_arm.spring_length = 4.0
	_cam_arm.add_excluded_object(get_rid())
	add_child(_cam_arm)
	var cam := Camera3D.new()
	cam.position = Vector3(0.6, 0, 0)  # slight over-the-shoulder
	_cam_arm.add_child(cam)
	cam.current = true

func _input(event: InputEvent) -> void:
	if not is_local:
		return
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		_yaw -= event.relative.x * _mouse_sens
		_pitch = clamp(_pitch - event.relative.y * _mouse_sens, -1.2, 0.5)
	elif event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _process(delta: float) -> void:
	if _fire_timer > 0.0:
		_fire_timer -= delta
	if is_local:
		_read_input()
		_update_camera()
	_update_animation()

func _read_input() -> void:
	# Movement relative to the CAMERA's actual orientation (no hand-rolled trig:
	# read the spring arm's basis so the sign is always right). x = right, y = fwd.
	var iv := Vector2(
		Input.get_action_strength("move_right") - Input.get_action_strength("move_left"),
		Input.get_action_strength("move_forward") - Input.get_action_strength("move_back"))
	var cam_fwd := Vector2.ZERO
	var cam_right := Vector2.ZERO
	if _cam_arm:
		var b := _cam_arm.global_transform.basis
		var f := Vector3(-b.z.x, 0, -b.z.z).normalized()   # camera forward on ground
		var r := Vector3(b.x.x, 0, b.x.z).normalized()      # camera right on ground
		cam_fwd = Vector2(f.x, f.z)
		cam_right = Vector2(r.x, r.z)
	var world := cam_right * iv.x + cam_fwd * iv.y
	set_move_intent(world)
	set_running(Input.is_action_pressed("run"))
	_aiming = Input.is_action_pressed("aim")

	# Facing: toward the camera's aim while aiming/firing, else toward movement.
	var face := Vector2.ZERO
	if (_aiming or _fire_timer > 0.0) and cam_fwd.length() > 0.01:
		face = cam_fwd
	elif world.length() > 0.01:
		face = world
	if face.length() > 0.01:
		var ty := atan2(face.x, face.y)
		set_yaw(ty)
		_model.rotation.y = lerp_angle(_model.rotation.y, ty, 0.3)

	if Input.is_action_just_pressed("jump"):
		request_jump()
	# Semi-auto: one round per press (Khushal's Rifle cooldown lands later, #8/#9).
	if Input.is_action_just_pressed("fire") and request_fire():
		_fire_timer = 0.25
	if Input.is_action_just_pressed("reload"):
		request_reload()

func _update_camera() -> void:
	if _cam_arm:
		_cam_arm.rotation = Vector3(_pitch, _yaw, 0)

func _update_animation() -> void:
	if _playback == null:
		return
	if forced_anim != "":
		if _playback.get_current_node() != forced_anim:
			_playback.travel(forced_anim)
		return
	var state := "idle"
	if get_health() <= 0:
		state = "death"
	elif not is_on_floor():
		state = "jump"
	elif _fire_timer > 0.0:
		state = "fire"
	elif is_reloading():
		state = "reload"
	elif _aiming:
		state = "aim_idle"
	else:
		var spd := get_planar_velocity().length()
		if spd > get_run_speed() * 0.9:
			state = "run"
		elif spd > 0.2:
			state = "walk"
	if _playback.get_current_node() != state:
		_playback.travel(state)
