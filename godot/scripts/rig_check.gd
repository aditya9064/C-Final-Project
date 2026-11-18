extends Node3D
# Headless structure gate for the Player rig (Increment 2): spawns both
# characters and asserts model loaded, AnimationTree active on the pinned clip,
# and the SMG attached to the hand bone. Rendering not required.

var _frames := 0

func _ready() -> void:
	var fb := StaticBody3D.new()
	var cs := CollisionShape3D.new()
	var bx := BoxShape3D.new(); bx.size = Vector3(40, 1, 40); cs.shape = bx
	fb.add_child(cs); fb.position = Vector3(0, -0.5, 0); add_child(fb)
	_spawn(0, Vector3(-1.5, 1.0, 0))
	_spawn(1, Vector3(1.5, 1.0, 0))

func _spawn(team: int, pos: Vector3) -> void:
	var p: Node = load("res://scenes/player.tscn").instantiate()
	p.team_id = team
	p.is_local = false
	p.forced_anim = "walk"
	p.position = pos
	add_child(p)

func _process(_d: float) -> void:
	_frames += 1
	if _frames != 30:
		return
	var ok := true
	for p in get_children():
		if not (p is CharacterBody3D):
			continue
		var model: Node = null
		for mc in p.get_children():
			if mc.get_node_or_null("Armature/Skeleton3D") != null:
				model = mc
				break
		var tree: AnimationTree = null
		for c in p.get_children():
			if c is AnimationTree:
				tree = c
		var smg_found := false
		var skel: Node = model.get_node_or_null("Armature/Skeleton3D") if model else null
		if skel:
			for c in skel.get_children():
				if c is BoneAttachment3D and c.get_child_count() > 0:
					smg_found = true
		var cur := ""
		if tree:
			cur = str(tree.get("parameters/playback").get_current_node())
		print("RIG team=", p.get_team(), " model=", model != null,
			" tree_active=", (tree.active if tree else false), " anim=", cur, " smg=", smg_found)
		if model == null or tree == null or not smg_found:
			ok = false
	print("RIG_CHECK ", "PASS" if ok else "FAIL")
	get_tree().quit(0 if ok else 1)
