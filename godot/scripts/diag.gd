extends Node3D
# Headless diagnostic: character height, gun length, animation loop modes.

func _ready() -> void:
	var c := (load("res://assets/models/characters/male_tailored.glb") as PackedScene).instantiate()
	add_child(c)
	var aabb := _merged_aabb(c)
	print("CHAR native AABB size: ", aabb.size, "  height(Y)=", aabb.size.y)
	var skel := _find(c, "Skeleton3D") as Skeleton3D
	if skel:
		var hi := skel.find_bone("Head")
		var gt := skel.global_transform * skel.get_bone_global_pose(hi)
		print("Head bone global Y: ", gt.origin.y)
	var ap := _find(c, "AnimationPlayer") as AnimationPlayer
	if ap:
		for n in ap.get_animation_list():
			var a := ap.get_animation(n)
			print("CLIP ", n, " loop_mode=", a.loop_mode, " len=", a.length)
	var g := (load("res://assets/models/weapons/smg_teal.glb") as PackedScene).instantiate()
	add_child(g)
	var gab := _merged_aabb(g)
	print("GUN AABB size: ", gab.size, "  length(max)=", max(gab.size.x, max(gab.size.y, gab.size.z)))
	get_tree().quit()

func _merged_aabb(n: Node) -> AABB:
	var out := AABB()
	var first := true
	for mi in _all_mesh(n):
		var a: AABB = mi.get_aabb()
		a = mi.global_transform * a
		if first:
			out = a; first = false
		else:
			out = out.merge(a)
	return out

func _all_mesh(n: Node, acc: Array = []) -> Array:
	if n is MeshInstance3D:
		acc.append(n)
	for ch in n.get_children():
		_all_mesh(ch, acc)
	return acc

func _find(n: Node, cls: String) -> Node:
	if n.get_class() == cls: return n
	for ch in n.get_children():
		var r := _find(ch, cls)
		if r: return r
	return null
