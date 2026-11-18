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

# Playable operators: rigged team_hat soldiers (teal -> Teal team, orange -> Orange team).
# They reuse the SAME 65-bone skeleton + 12 clips as the originals, so they drop in here.
# Backup originals kept at male_tailored.glb / female_tailored.glb (not deleted).
const CHAR_GLB := {
	0: "res://assets/models/characters/teal_soldier.glb",
	1: "res://assets/models/characters/orange_soldier.glb",
}
const WEAPON_GLB := {
	0: "res://assets/models/weapons/smg_teal.glb",
	1: "res://assets/models/weapons/smg_orange.glb",
}
const SKELETON_PATH := "Armature/Skeleton3D"
const HAND_BONE := "hand_r"
const MODEL_Y := -0.9           # feet to capsule centre
const EYE_HEIGHT := 0.62        # local-player eye height above the capsule centre
# SMG mesh is modelled along +X (barrel) / +Y up (gen_weapons.py, exported Y-up),
# so in Godot local space the muzzle tip sits at roughly this point.
const MUZZLE_LOCAL := Vector3(0.37, 0.012, 0.0)
# First-person weapon rest pose in CAMERA space: lower-right, barrel +X -> -Z forward.
const VM_POS := Vector3(0.145, -0.165, -0.30)
const VM_ROT := Vector3(-0.035, 1.605, 0.0)   # ~(-2deg, 92deg, 0): barrel forward, slight cant
const LOCOMOTION := ["idle", "walk", "run", "jog", "jump", "jump_start",
	"jump_land", "aim", "aim_idle", "fire", "reload", "death"]

var _model: Node3D
var _tree: AnimationTree
var _playback: AnimationNodeStateMachinePlayback
var _cam_arm: Node3D            # first-person head pivot (local player only)
var _fp_cam: Camera3D
var _vm: Node3D                 # first-person viewmodel weapon (child of the camera)
var _muzzle: Node3D
var _flash: Node3D
var _flash_light: OmniLight3D
var _flash_t := 0.0
var _recoil := 0.0              # procedural recoil amount (decays), NOT a skeletal clip
var _ads := 0.0                 # 0 = hip, 1 = aimed (eased toward the aim button)
var _yaw := 0.0
var _pitch := 0.0
var _aiming := false
var _fire_timer := 0.0
var _mouse_sens := 0.0025

func _ready() -> void:
	setup(team_id, global_position)
	_build_model()
	_build_anim_tree()
	_attach_weapon()
	# Local player is first-person: hide our OWN full body + hand weapon from our own
	# camera (shadows kept) so we don't see the inside of our head or a second gun.
	# Remote instances of this same player keep the body visible -- see note in _ready.
	if is_local:
		_set_shadow_only(_model)
	_face_inward()
	# The C++ PlayerBody emits "respawned" after an authoritative rebuild; reset
	# the view + animation then (the body already repositioned itself).
	connect("respawned", _on_respawned)
	if is_local:
		_build_camera()
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _face_inward() -> void:
	# Face the arena centre (teal at -X faces +X, orange at +X faces -X). The MODEL
	# forward is +Z and the CAMERA forward is -Z, so the camera yaw is the model's
	# facing yaw + PI to sit BEHIND the player looking the same way.
	if not spawn_face_inward:
		return
	var inward := 1.0 if team_id == 0 else -1.0
	var face_yaw := atan2(inward, 0.0)
	set_yaw(face_yaw)
	if _model:
		_model.rotation.y = face_yaw
	# First-person: the camera looks the SAME way the body faces (inward). The model
	# forward is +Z and the camera forward is -Z, so the camera yaw mirrors the model.
	_yaw = atan2(-inward, 0.0)
	_pitch = 0.0

func _on_respawned() -> void:
	_fire_timer = 0.0
	_aiming = false
	_recoil = 0.0
	_face_inward()
	if is_local:
		_update_camera()
		if _vm:
			_vm.visible = true
	if _playback:
		_playback.travel("idle")

func _build_model() -> void:
	var path: String = CHAR_GLB.get(team_id, CHAR_GLB[0])
	var ps := load(path) as PackedScene
	if ps == null:
		# Fallback to the original tailored characters if a rigged soldier GLB is
		# missing/unverified (keeps the existing working character system available).
		var fb := "res://assets/models/characters/%s" % \
			("male_tailored.glb" if team_id == 0 else "female_tailored.glb")
		push_warning("character GLB '%s' failed to load; falling back to %s" % [path, fb])
		ps = load(fb) as PackedScene
	_model = ps.instantiate()
	_model.position = Vector3(0, MODEL_Y, 0)
	add_child(_model)

# Hide a subtree from the local camera but keep its shadow (first-person self-hide).
func _set_shadow_only(n: Node) -> void:
	if n is GeometryInstance3D:
		(n as GeometryInstance3D).cast_shadow = \
			GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	for c in n.get_children():
		_set_shadow_only(c)

func _set_cast_off(n: Node) -> void:
	if n is GeometryInstance3D:
		(n as GeometryInstance3D).cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	for c in n.get_children():
		_set_cast_off(c)

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
	# Solved SMG mount from the approved showcase scenes (duo.gd / preview.gd,
	# commit 655874d "fix gun yaw"): grip in palm, barrel forward + level, optic up.
	smg.position = Vector3.ZERO
	smg.rotation_degrees = Vector3(-0.89, -94.88, -78.76)
	att.add_child(smg)

func _build_camera() -> void:
	# First-person head pivot at eye height; _yaw/_pitch drive it (see _update_camera).
	_cam_arm = Node3D.new()
	_cam_arm.position = Vector3(0, EYE_HEIGHT, 0)
	add_child(_cam_arm)
	_fp_cam = Camera3D.new()
	_fp_cam.fov = 78.0
	_fp_cam.near = 0.02            # close near-plane so the viewmodel never clips the lens
	_cam_arm.add_child(_fp_cam)
	_fp_cam.current = true
	_build_viewmodel()

func _build_viewmodel() -> void:
	var wps := load(WEAPON_GLB.get(team_id, WEAPON_GLB[0])) as PackedScene
	if wps == null:
		return
	_vm = wps.instantiate()
	_vm.position = VM_POS
	_vm.rotation = VM_ROT
	_fp_cam.add_child(_vm)
	# The viewmodel is a screen-space prop -- don't let it cast a second gun shadow
	# into the world (the full-body hand SMG already casts the player's weapon shadow).
	_set_cast_off(_vm)
	# Muzzle marker at the barrel tip -- the shot/flash origin.
	_muzzle = Node3D.new()
	_muzzle.position = MUZZLE_LOCAL
	_vm.add_child(_muzzle)
	# Muzzle flash: a pulsed light + an unshaded billboard quad, hidden until a shot.
	_flash = Node3D.new()
	_flash_light = OmniLight3D.new()
	_flash_light.light_color = Color(1.0, 0.82, 0.45)
	_flash_light.light_energy = 0.0
	_flash_light.omni_range = 3.5
	_flash.add_child(_flash_light)
	var quad := MeshInstance3D.new()
	var qm := QuadMesh.new()
	qm.size = Vector2(0.17, 0.17)
	var fmat := StandardMaterial3D.new()
	fmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	fmat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	fmat.albedo_color = Color(1.0, 0.88, 0.5, 0.9)
	fmat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	qm.material = fmat
	quad.mesh = qm
	_flash.add_child(quad)
	_flash.visible = false
	_muzzle.add_child(_flash)

# Fired when a shot is actually authorised by the C++ Player (request_fire()==true).
func _on_shot() -> void:
	_recoil = min(_recoil + 0.6, 1.2)
	_flash_t = 0.05
	if _flash:
		_flash.visible = true
	if _flash_light:
		_flash_light.light_energy = 3.0
	_fire_ray()

# Shot ray from the CROSSHAIR (viewport centre), excluding our own body. Hit
# detection + damage are combat's lane (#8/#9); here we only show an aligned tracer.
func _fire_ray() -> void:
	if _fp_cam == null or _muzzle == null:
		return
	var center := get_viewport().get_visible_rect().size * 0.5
	var origin := _fp_cam.project_ray_origin(center)
	var dir := _fp_cam.project_ray_normal(center)
	var space := get_world_3d().direct_space_state
	var q := PhysicsRayQueryParameters3D.create(origin, origin + dir * 200.0)
	q.exclude = [get_rid()]
	var hit := space.intersect_ray(q)
	var endp: Vector3 = hit.position if hit else origin + dir * 200.0
	_spawn_tracer(_muzzle.global_position, endp)

func _spawn_tracer(a: Vector3, b: Vector3) -> void:
	var im := ImmediateMesh.new()
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.albedo_color = Color(1.0, 0.9, 0.55)
	im.surface_begin(Mesh.PRIMITIVE_LINES, m)
	im.surface_add_vertex(a)
	im.surface_add_vertex(b)
	im.surface_end()
	var mi := MeshInstance3D.new()
	mi.mesh = im
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	get_tree().current_scene.add_child(mi)
	var tw := create_tween()
	tw.tween_interval(0.04)
	tw.tween_callback(mi.queue_free)

# Procedural viewmodel motion (recoil kick + reload dip). These are positional
# offsets, NOT skeletal animation clips -- the SMG mesh has no bones.
func _update_viewmodel(delta: float) -> void:
	if _vm == null:
		return
	_vm.visible = get_health() > 0
	if _flash_t > 0.0:
		_flash_t -= delta
		if _flash_t <= 0.0:
			if _flash:
				_flash.visible = false
			if _flash_light:
				_flash_light.light_energy = 0.0
	_recoil = move_toward(_recoil, 0.0, delta * 5.0)
	# Aim-down-sight: ease the weapon from hip rest toward a centred/sighted pose.
	_ads = move_toward(_ads, 1.0 if _aiming else 0.0, delta * 9.0)
	var dip := 0.0
	if is_reloading():
		dip = sin(get_reload_progress() * PI) * 0.13   # lower, then raise back
	# ADS pose sits the gun lower-centre so the optic rides just under the crosshair
	# (the lens is opaque) and the target stays visible above it.
	var target: Vector3 = VM_POS.lerp(Vector3(0.0, -0.17, -0.26), _ads)
	target.z += _recoil * 0.05        # kick back toward the camera
	target.y -= dip
	_vm.position = _vm.position.lerp(target, 0.45)
	_vm.rotation = VM_ROT + Vector3(_recoil * 0.12, 0.0, 0.0)  # muzzle rises on recoil
	if _fp_cam:
		_fp_cam.fov = lerp(78.0, 62.0, _ads)                   # slight zoom while aiming

func _input(event: InputEvent) -> void:
	if not is_local:
		return
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		_yaw -= event.relative.x * _mouse_sens
		# First-person: near-symmetric look range (~+/-77 deg) so you can look up.
		_pitch = clamp(_pitch - event.relative.y * _mouse_sens, -1.35, 1.35)
	elif event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _process(delta: float) -> void:
	if _fire_timer > 0.0:
		_fire_timer -= delta
	if is_local:
		_read_input()
		_update_camera()
		_update_viewmodel(delta)
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

	# First-person: the body always faces where the camera looks, so shots leave
	# along the crosshair and strafing keeps us facing forward.
	if cam_fwd.length() > 0.01:
		var ty := atan2(cam_fwd.x, cam_fwd.y)
		set_yaw(ty)
		_model.rotation.y = ty

	if Input.is_action_just_pressed("jump"):
		request_jump()
	# Semi-auto: one round per press (Khushal's Rifle cooldown lands later, #8/#9).
	if Input.is_action_just_pressed("fire") and request_fire():
		_fire_timer = 0.25
		_on_shot()
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
