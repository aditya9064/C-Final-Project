extends Node3D
## Fireteam: Skirmish — 3D arena environment. MAIN ENTRY POINT for arena
## construction and rendering setup (attached to the root of scenes/arena.tscn).
##
## The whole arena is assembled procedurally in _ready(): every wall, cover piece,
## prop and tree is placed from the numbers in this file, so the gameplay-relevant
## geometry (octagon footprint, 4.50 m walls, 1.28 m / 2.10 m cover, 1.80 m player
## scale) lives in ONE reviewable place instead of being spread over dozens of
## hand-edited .tscn files. Each prop is a small factory function (_barrier,
## _sandbag_pile, _supply_crate, ...) that could later be saved as a PackedScene.
##
## Visual meshes and collision are deliberately SEPARATE: detailed Blender GLBs are
## for looks only, and every piece of cover carries a simple StaticBody3D box or
## cylinder sized to its gameplay dimensions. The raycast cover tests at the bottom
## of this file run against that collision on every launch.
##
## File map (search for the "=====" banners):
##   1. Arena geometry & dimensions   — CONFIG constants, palette, octagon VERTS
##   2. Materials                     — triplanar PBR + flat paint + foliage card
##   3. Mesh / collision helpers      — _box/_cyl/_mesh/_collider_*, stripes, terrain lookup
##   4. Ground & terrain              — slab, sculpted terrain, floor markings
##   5. Walls and boundaries          — octagon walls, corner columns, ivy
##   6. Canopy and central portal     — tensioned fabric roof, posts, beams, portal
##   7. Spawn zones                   — teal (-X) and orange (+X) gateway bays
##   8. Cover, collision and props    — barriers, loophole, sandbags, crates, barrels
##   9. Trees and environment         — CC0 woodland belt, bushes, rocks
##  10. Placeholder test humans       — 1.80 m scale references (temporary)
##  11. Lighting                      — sun, canopy fill, warm sky fill
##  12. Rendering-quality presets     — `-- --preset performance` vs default high
##  13. Cameras, capture, benchmark   — fixed QA views, `--view`, `--bench`
##  14. Cover raycast tests           — five automated PASS/FAIL checks
##
## Supporting files: assets/models/*.glb + assets/models/nature/*.gltf (meshes),
## textures/ (CC0 PBR sets), pipeline/*.py (Blender generators), qa/ (scripts +
## evidence). This script only CONSUMES those; it never writes assets.
##
## Run normally  -> free-fly inspection cam (WASD+mouse, Q/E down/up, Shift fast;
##                  1 tactical, 2 reset, 3 top-down, Esc releases the mouse)
## Run with `-- --view <name>`  -> fixed camera, writes arena_<name>.png, quits
## Run with `-- --bench [--view <name>]` -> 300-frame FPS benchmark, quits
## Run with `-- --preset performance`     -> lower-cost rendering preset
## Cover raycast tests print COVER_TEST [PASS|FAIL] lines on every run.

# =============================================================================
# 1. Arena geometry & dimensions
# =============================================================================
# Player scale — the single source of truth for everything height-related.
# Cover heights were chosen against these: 1.28 m low cover hides a crouching
# player (eye 0.92) but not a standing one (eye 1.63); 2.10 m tall cover hides a
# standing player including the top of the head (1.80). Units are metres.
const PLAYER_HEIGHT := 1.80
const STAND_EYE := 1.63
const CROUCH_HEIGHT := 1.10
const CROUCH_EYE := 0.92
const CAP_RADIUS := 0.32

# Palette — hex values taken from the concept-art references (art/references/).
# Team colours: teal = -X side, orange = +X side, used on gateways, stripes, banners.
const C_CONCRETE := Color("b7aa98")
const C_STEEL := Color("34383d")
const C_TEAL := Color("0faaa6")
const C_ORANGE := Color("ff8b29")
const C_CANOPY := Color("f79232")
const C_SAND := Color("ad9873")
const C_OLIVE := Color("626a43")
const C_PAINT := Color("f1eee6")

# Arena footprint — a chamfered octagon, 42 m (X) x 34 m (Z) with 5 m corner
# chamfers, centred on the world origin. Vertices are (X, Z) in metres, listed
# counter-clockwise when viewed from above; consecutive pairs become wall segments
# and the eight vertices get corner columns. The arena centre (0,0) is the canopy;
# the spawn gateways sit on the +-X walls (x = +-21), one per team.
const VERTS := [
	Vector2(-16, -17), Vector2(16, -17), Vector2(21, -12), Vector2(21, 12),
	Vector2(16, 17), Vector2(-16, 17), Vector2(-21, 12), Vector2(-21, -12),
]
const WALL_H := 4.5      # perimeter wall height (m)
const WALL_T := 0.45     # perimeter wall thickness (m)

var _mat := {}            # name -> Material cache
var _sun: DirectionalLight3D
var _cam: Camera3D
var _cap := false
var _frames := 0
var _shot_name := "view"
var _tests_done := false
var _bench := false
var _bench_t0 := 0.0

func _ready() -> void:
	_build_materials()
	_build_ground_and_environment()
	_build_floor_markings()
	_build_outer_walls()
	_build_canopy_and_portal()
	_build_spawns()
	_build_obstacles()
	_build_outer_trees()
	_build_test_humans()
	_build_lighting()
	_setup_camera()
	_apply_preset()

func _physics_process(_d: float) -> void:
	if not _tests_done:
		_tests_done = true
		_run_cover_tests()

# =============================================================================
# 2. Materials
# =============================================================================
# Two families, both cached in _mat by name:
#  - _pbr(): CC0 texture sets (textures/<dir>/color|normal|rough|ao.jpg) applied
#    with WORLD-space triplanar mapping. Design decision: the Blender meshes carry
#    no UVs, and triplanar mapping means the same concrete texture reads at the
#    same scale on a 42 m wall and a 0.7 m barrier with no stretching.
#  - _flat(): untextured coloured materials for paint, steel, fabric, foliage tints.
func _pbr(key: String, dir: String, tint: Color, uv := 0.25, metallic := 0.0, spec := 0.15) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	var base := "res://textures/%s/" % dir
	m.albedo_texture = load(base + "color.jpg")
	m.albedo_color = tint
	m.metallic_specular = spec
	m.metallic = metallic
	var n := base + "normal.jpg"
	if ResourceLoader.exists(n):
		m.normal_enabled = true
		m.normal_texture = load(n)
	var r := base + "rough.jpg"
	if ResourceLoader.exists(r):
		m.roughness_texture = load(r)
	var ao := base + "ao.jpg"
	if ResourceLoader.exists(ao):
		m.ao_enabled = true
		m.ao_texture = load(ao)
		m.ao_light_affect = 0.6
	m.uv1_triplanar = true
	m.uv1_world_triplanar = true
	m.uv1_scale = Vector3(uv, uv, uv)
	_mat[key] = m
	return m

func _flat(key: String, col: Color, rough := 0.8, metallic := 0.0, emis := 0.0) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = col
	m.roughness = rough
	m.metallic = metallic
	m.metallic_specular = 0.5
	if emis > 0.0:
		m.emission_enabled = true
		m.emission = col
		m.emission_energy_multiplier = emis
	_mat[key] = m
	return m

func _build_materials() -> void:
	_pbr("concrete", "concrete", Color(0.82, 0.74, 0.60), 0.22, 0.0, 0.1)    # warm beige concrete
	_pbr("floor", "concrete", Color(0.84, 0.77, 0.64), 0.10, 0.0, 0.08)
	_pbr("olive", "metal", C_OLIVE, 0.4, 0.15, 0.2)
	_pbr("sand", "fabric", C_SAND, 0.5, 0.0, 0.15)
	_flat("steel", C_STEEL, 0.6, 0.6)
	_flat("canopy", C_CANOPY, 0.85, 0.0)
	_flat("teal", C_TEAL, 0.5)
	_flat("orange", C_ORANGE, 0.5)
	_flat("paint", C_PAINT, 0.7)
	_flat("barrel", Color("3c4a3a"), 0.5, 0.5)
	_flat("trunk", Color("4a3524"), 0.95)
	_flat("leaf_a", Color("3f6b2e"), 0.9)
	_flat("leaf_b", Color("4f7d36"), 0.9)
	_flat("leaf_c", Color("355e28"), 0.9)
	_flat("hill", Color("5f6e45"), 1.0)
	_flat("grass", Color("5a7a3a"), 0.95)
	_flat("light", Color(1, 0.96, 0.85), 0.4, 0.0, 2.2)
	_flat("ivy", Color("3a5a2c"), 0.9)
	_flat("joint", Color("8a8070"), 0.9)
	_flat("bark", Color("4a3524"), 0.95)
	# reference prop palette: deeper olive equipment cases, dark reinforced crates,
	# pallet wood, olive-tan tarp cloth, matte woven canopy fabric
	_flat("olive_dark", Color("4e5a3c"), 0.72)
	_flat("crate_dark", Color("3c3f41"), 0.62, 0.15)
	_flat("wood", Color("8a6a44"), 0.9)
	_flat("tarp", Color("6d7452"), 0.95)
	# deep enough that direct sun + ACES still reads orange (sunlit ~#F79232 target,
	# shaded ~#D97829) instead of washing to pale yellow
	var fab := _flat("canopy_fab", Color("cf6a1a"), 1.0, 0.0)
	fab.cull_mode = BaseMaterial3D.CULL_DISABLED
	fab.metallic_specular = 0.03
	_flat("bushgreen", Color("4a6a2c"), 0.95)
	_pbr("concrete2", "concrete", Color(0.86, 0.78, 0.64), 0.18, 0.0, 0.1)   # recessed panels: same warm beige family
	_pbr("basegrime", "concrete", Color("6b6152"), 0.3, 0.0, 0.08)
	var terr := StandardMaterial3D.new()
	terr.vertex_color_use_as_albedo = true
	terr.roughness = 1.0
	terr.metallic_specular = 0.08
	_mat["terrain"] = terr
	_leaf_material()

func _leaf_material() -> void:
	# alpha-scissor foliage card material shared by trees, shrubs and ivy. Backlight
	# keeps leaves from reading as flat black when the sun is behind them.
	var m := StandardMaterial3D.new()
	# natural leaf-cluster texture (from the CC0 MegaKit) instead of the old arrow
	# sprigs, so the ivy strands read as overlapping real leaves, not game sprites
	m.albedo_texture = load("res://textures/foliage/ivy_leaf.png")
	m.albedo_color = Color(0.82, 0.9, 0.78)          # pull the green toward natural, not neon
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	m.alpha_scissor_threshold = 0.3
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	m.roughness = 0.9
	m.metallic_specular = 0.1
	# soft green transmission so the shadow side of the vines still reads green
	m.backlight_enabled = true
	m.backlight = Color(0.22, 0.34, 0.16)
	m.emission_enabled = true
	m.emission = Color(0.06, 0.12, 0.05)
	m.emission_energy_multiplier = 0.6
	m.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	_mat["leaf_card"] = m

# =============================================================================
# 3. Mesh / collision helpers
# =============================================================================
# Convention used everywhere below: `pos` is the CENTRE of the shape in world
# metres (so a box of height h standing on the floor is placed at y = h/2), `yaw`
# is rotation about +Y in radians. `collide = true` wraps the mesh in a
# StaticBody3D with a matching BoxShape3D/CylinderShape3D; decorative details pass
# collide = false so they never affect movement or hitscan.
func _box(size: Vector3, pos: Vector3, mat_key: String, yaw := 0.0, collide := true, parent: Node = null) -> Node3D:
	var p: Node = parent if parent else self
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	mi.material_override = _mat.get(mat_key)
	if collide:
		var body := StaticBody3D.new()
		body.position = pos
		body.rotation.y = yaw
		body.add_child(mi)
		var cs := CollisionShape3D.new()
		var sh := BoxShape3D.new()
		sh.size = size
		cs.shape = sh
		body.add_child(cs)
		p.add_child(body)
		return body
	mi.position = pos
	mi.rotation.y = yaw
	p.add_child(mi)
	return mi

func _cyl(radius: float, h: float, pos: Vector3, mat_key: String, collide := true, sides := 20, parent: Node = null) -> Node3D:
	var p: Node = parent if parent else self
	var mi := MeshInstance3D.new()
	var cm := CylinderMesh.new()
	cm.top_radius = radius
	cm.bottom_radius = radius
	cm.height = h
	cm.radial_segments = sides
	mi.mesh = cm
	mi.material_override = _mat.get(mat_key)
	if collide:
		var body := StaticBody3D.new()
		body.position = pos
		body.add_child(mi)
		var cs := CollisionShape3D.new()
		var sh := CylinderShape3D.new()
		sh.radius = radius
		sh.height = h
		cs.shape = sh
		body.add_child(cs)
		p.add_child(body)
		return body
	mi.position = pos
	p.add_child(mi)
	return mi

func _sphere(radius: float, pos: Vector3, mat_key: String, squash := 1.0, parent: Node = null) -> MeshInstance3D:
	var p: Node = parent if parent else self
	var mi := MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = radius
	sm.height = radius * 2.0
	mi.mesh = sm
	mi.material_override = _mat.get(mat_key)
	mi.position = pos
	mi.scale = Vector3(1, squash, 1)
	p.add_child(mi)
	return mi

func _rot(off: Vector3, yaw: float) -> Vector3:
	return Vector3(off.x * cos(yaw) - off.z * sin(yaw), off.y, off.x * sin(yaw) + off.z * cos(yaw))

# --- flat painted shapes (decals as thin geometry, no z-fighting) ---------------
func _flat_poly(pts: Array, mat_key: String, two_sided := true) -> MeshInstance3D:
	# planar polygon (triangle fan) in WORLD coordinates; used for diagonal paint
	# stripes, chamfer fills and markings.
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in range(1, pts.size() - 1):
		for v in [pts[0], pts[i], pts[i + 1]]:
			st.add_vertex(v)
	st.generate_normals()
	var mi := MeshInstance3D.new()
	mi.mesh = st.commit()
	var m: Material = _mat[mat_key]
	if two_sided:
		m = m.duplicate()
		m.cull_mode = BaseMaterial3D.CULL_DISABLED
	mi.material_override = m
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	return mi

func _rotg(off: Vector3, yaw: float) -> Vector3:
	# rotation matching Node3D.rotation.y (local +X -> (cos, 0, -sin)); use this when
	# positioning parts relative to a node that is itself rotated by `yaw`.
	return Vector3(off.x * cos(yaw) + off.z * sin(yaw), off.y, -off.x * sin(yaw) + off.z * cos(yaw))

func _beam(a: Vector3, b: Vector3, thick: float, mat_key: String) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var bx := BoxMesh.new()
	bx.size = Vector3(thick, thick, a.distance_to(b))
	mi.mesh = bx
	mi.material_override = _mat[mat_key]
	var mid := (a + b) * 0.5
	add_child(mi)
	mi.look_at_from_position(mid, b, Vector3.UP if absf((b - a).normalized().y) < 0.99 else Vector3.RIGHT)
	return mi

func _stripe(c: Vector3, yaw: float, nz: float, x0: float, y0: float, y1: float, w: float, slant: float, mat_key: String) -> void:
	# A diagonal paint stripe (parallelogram) on a vertical face. Local frame:
	# x along the face, y up, z = face normal offset (nz); rotated by yaw about c.
	var pts := [
		c + _rot(Vector3(x0, y0, nz), yaw),
		c + _rot(Vector3(x0 + w, y0, nz), yaw),
		c + _rot(Vector3(x0 + w + slant, y1, nz), yaw),
		c + _rot(Vector3(x0 + slant, y1, nz), yaw),
	]
	_flat_poly(pts, mat_key)

# --- terrain height lookup (so exterior props sit ON the sculpted ground) ------
var _terr_h := {}
const TERR_CELL := 1.25
func _build_terrain_lookup(inst: Node3D) -> void:
	var meshes: Array = []
	_collect_meshes(inst, meshes)
	for mi in meshes:
		if not mi.mesh:
			continue
		var xf: Transform3D = mi.global_transform if mi.is_inside_tree() else inst.transform * mi.transform
		for s in mi.mesh.get_surface_count():
			var verts: PackedVector3Array = mi.mesh.surface_get_arrays(s)[Mesh.ARRAY_VERTEX]
			for v in verts:
				var p: Vector3 = xf * v
				var k := Vector2i(roundi(p.x / TERR_CELL), roundi(p.z / TERR_CELL))
				_terr_h[k] = p.y

func _terrain_height(x: float, z: float) -> float:
	var k := Vector2i(roundi(x / TERR_CELL), roundi(z / TERR_CELL))
	if _terr_h.has(k):
		return _terr_h[k]
	# fall back to nearest neighbour ring, then the flat apron level
	for r in range(1, 3):
		for dx in range(-r, r + 1):
			for dz in range(-r, r + 1):
				var kk := k + Vector2i(dx, dz)
				if _terr_h.has(kk):
					return _terr_h[kk]
	return -0.15

# --- Blender-generated GLB meshes (visual) + separate collision ---------------
# GLB imports as a PackedScene that carries real PBR materials from Blender.
# Pass mat_key == "" to keep those imported materials (multi-material props like
# the crate, canopy and trees); pass a key to override every surface with one of
# our triplanar textured materials (concrete, metal, fabric, …).
var _mesh_cache := {}
var _veg_dbg := false

func _load_scene(name: String) -> PackedScene:
	# Project-generated props are .glb; the nature kit is .gltf with a shared
	# textures/ folder (see pipeline/convert_nature.py), so try both.
	if not _mesh_cache.has(name):
		var path := "res://assets/models/%s.glb" % name
		if not ResourceLoader.exists(path):
			path = "res://assets/models/%s.gltf" % name
		_mesh_cache[name] = load(path)
	return _mesh_cache[name]

func _collect_meshes(n: Node, out: Array) -> void:
	if n is MeshInstance3D:
		out.append(n)
	for c in n.get_children():
		_collect_meshes(c, out)

func _mesh(name: String, pos: Vector3, mat_key: String, yaw := 0.0, scale := 1.0, two_sided := false) -> Node3D:
	var inst: Node3D = _load_scene(name).instantiate()
	inst.position = pos
	inst.rotation.y = yaw
	inst.scale = Vector3(scale, scale, scale)
	add_child(inst)
	if mat_key != "":
		var m: Material = _mat.get(mat_key)
		if m:
			var mm: Material = m
			if two_sided:
				mm = m.duplicate()
				mm.cull_mode = BaseMaterial3D.CULL_DISABLED
			var meshes: Array = []
			_collect_meshes(inst, meshes)
			for mi in meshes:
				mi.material_override = mm
	return inst

func _collider_box(size: Vector3, pos: Vector3, yaw := 0.0) -> void:
	var body := StaticBody3D.new()
	body.position = pos
	body.rotation.y = yaw
	var cs := CollisionShape3D.new()
	var sh := BoxShape3D.new()
	sh.size = size
	cs.shape = sh
	body.add_child(cs)
	add_child(body)

func _collider_cyl(radius: float, h: float, pos: Vector3) -> void:
	var body := StaticBody3D.new()
	body.position = pos
	var cs := CollisionShape3D.new()
	var sh := CylinderShape3D.new()
	sh.radius = radius
	sh.height = h
	cs.shape = sh
	body.add_child(cs)
	add_child(body)

# =============================================================================
# 4. Ground & terrain
# =============================================================================
# Three layers: an invisible safety floor far below everything (so nothing can
# fall forever), the sculpted vertex-coloured terrain.glb from pipeline/
# gen_terrain.py (flat under the arena, rolling hills outside), and the 46 x 38 m
# concrete slab the arena stands on. Floor markings (slab joints, the white centre
# ring R = 8.2 m and its orange arc) are thin flat meshes raised a few mm above
# the slab to avoid z-fighting; none of them collide.
func _build_ground_and_environment() -> void:
	_box(Vector3(600, 0.4, 600), Vector3(0, -1.0, 0), "hill", 0.0, true)        # safety floor, below terrain
	# sculpted vertex-coloured terrain (grass + dirt + distant hills), flat near
	# the arena so the concrete slab sits flush
	var terr := _mesh("terrain", Vector3(0, 0, 0), "terrain")
	_build_terrain_lookup(terr)                                                   # for grounding exterior props
	_box(Vector3(46, 0.3, 38), Vector3(0, -0.02, 0), "floor", 0.0, true)        # arena slab

func _build_floor_markings() -> void:
	for gx in range(-6, 7):
		_box(Vector3(0.03, 0.02, 34), Vector3(gx * 3.5, 0.145, 0), "joint", 0.0, false)
	for gz in range(-5, 6):
		_box(Vector3(42, 0.02, 0.03), Vector3(0, 0.145, gz * 3.5), "joint", 0.0, false)
	# continuous painted white ring (flat annulus) around the canopy, R = 8.2 m
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var n := 140
	var ri := 8.08
	var ro := 8.34
	for i in n:
		var a0 := TAU * i / float(n)
		var a1 := TAU * (i + 1) / float(n)
		var p0i := Vector3(cos(a0) * ri, 0, sin(a0) * ri)
		var p0o := Vector3(cos(a0) * ro, 0, sin(a0) * ro)
		var p1i := Vector3(cos(a1) * ri, 0, sin(a1) * ri)
		var p1o := Vector3(cos(a1) * ro, 0, sin(a1) * ro)
		for tri in [[p0i, p0o, p1o], [p0i, p1o, p1i]]:
			for v in tri:
				st.add_vertex(v)
	st.generate_normals()
	var ring := MeshInstance3D.new()
	ring.mesh = st.commit()
	ring.material_override = _mat["paint"]
	ring.position = Vector3(0, 0.152, 0)
	add_child(ring)
	# one continuous orange accent arc on the ring (≈70°), as in the references
	var sto := SurfaceTool.new()
	sto.begin(Mesh.PRIMITIVE_TRIANGLES)
	var seg := 28
	var a_start := PI * 0.12
	var a_sweep := deg_to_rad(70.0)
	for i in seg:
		var a0 := a_start + a_sweep * i / float(seg)
		var a1 := a_start + a_sweep * (i + 1) / float(seg)
		var q0i := Vector3(cos(a0) * ri, 0, sin(a0) * ri)
		var q0o := Vector3(cos(a0) * ro, 0, sin(a0) * ro)
		var q1i := Vector3(cos(a1) * ri, 0, sin(a1) * ri)
		var q1o := Vector3(cos(a1) * ro, 0, sin(a1) * ro)
		for tri in [[q0i, q0o, q1o], [q0i, q1o, q1i]]:
			for v in tri:
				sto.add_vertex(v)
	sto.generate_normals()
	var arc := MeshInstance3D.new()
	arc.mesh = sto.commit()
	arc.material_override = _mat["orange"]
	arc.position = Vector3(0, 0.154, 0)
	add_child(arc)
	# sparse grass tufts (small clusters of thin blades), mostly near the walls
	for i in 46:
		var gx := randf_range(-20, 20)
		var gz := randf_range(-16, 16)
		var edge: float = max(abs(gx) / 20.0, abs(gz) / 16.0)
		if Vector2(gx, gz).length() > 9.5 and randf() < edge:
			_grass_tuft(Vector3(gx, 0.15, gz))

func _grass_tuft(c: Vector3) -> void:
	for b in 5:
		var a := randf_range(0, TAU)
		var hgt := randf_range(0.14, 0.26)
		var blade := _box(Vector3(0.03, hgt, 0.03), c + Vector3(cos(a) * 0.09, hgt * 0.5, sin(a) * 0.09), "grass", a, false)
		blade.rotation.x = randf_range(-0.2, 0.2)

# =============================================================================
# 5. Walls and boundaries
# =============================================================================
# One collided 4.50 m x 0.45 m concrete panel per octagon edge (VERTS[i] ->
# VERTS[i+1]), plus purely decorative detail on the INNER face: a recessed
# panel, steel coping, grime band, cast seams and an emissive light strip. The
# eight corners get 0.7 m columns so the chamfers read as solid construction.
# Ivy is placed only on alternate inner segments away from the spawn walls.
func _build_outer_walls() -> void:
	for i in VERTS.size():
		var a: Vector2 = VERTS[i]
		var b: Vector2 = VERTS[(i + 1) % VERTS.size()]
		var mid := (a + b) * 0.5
		var d := b - a
		var ln := d.length() + WALL_T
		var yaw := -atan2(d.y, d.x)
		# main panel + a slightly recessed inner panel for a cast-concrete read
		_box(Vector3(ln, WALL_H, WALL_T), Vector3(mid.x, WALL_H * 0.5, mid.y), "concrete", yaw)
		var inset := _rot(Vector3(0, 0, -WALL_T * 0.5 + 0.02), yaw)
		_box(Vector3(ln - 0.6, WALL_H - 0.8, 0.06), Vector3(mid.x + inset.x, WALL_H * 0.5 + 0.1, mid.y + inset.z), "concrete2", yaw, false)
		# steel coping cap + dirt/grime band along the base
		_box(Vector3(ln + 0.1, 0.16, WALL_T + 0.2), Vector3(mid.x, WALL_H + 0.08, mid.y), "steel", yaw, false)
		var baseo := _rot(Vector3(0, 0, -WALL_T * 0.5 - 0.03), yaw)
		_box(Vector3(ln, 0.55, 0.05), Vector3(mid.x + baseo.x, 0.27, mid.y + baseo.z), "basegrime", yaw, false)
		# vertical panel seams
		var n := int(ln / 2.5)
		for s in range(1, n):
			var t := s / float(n) - 0.5
			var off := _rot(Vector3(t * ln, 0, -WALL_T * 0.5 - 0.02), yaw)
			_box(Vector3(0.05, WALL_H - 0.3, 0.04), Vector3(mid.x + off.x, WALL_H * 0.5, mid.y + off.z), "joint", yaw, false)
		# metal light housing + emissive strip
		var lo := _rot(Vector3(0, 1.4, -WALL_T * 0.5 - 0.1), yaw)
		_box(Vector3(min(ln * 0.4, 3.0) + 0.1, 0.26, 0.12), Vector3(mid.x + lo.x, lo.y, mid.y + lo.z), "steel", yaw, false)
		_box(Vector3(min(ln * 0.4, 3.0), 0.14, 0.1), Vector3(mid.x + lo.x, lo.y, mid.y + lo.z), "light", yaw, false)
		# ivy as a CONNECTED draping band along the top of selected inner segments:
		# a continuous row of tightly overlapping strands (merging into one leafy
		# mass) plus a staggered second depth layer, rather than isolated sprigs.
		# Kept off the spawn segments.
		if i % 2 == 0 and abs(mid.x) < 17:
			var span := 0.74                                  # fraction of segment covered
			var count: int = max(8, int(ln * 1.6))
			for k in count:
				var tt := (k / float(count - 1) - 0.5) * span
				# front row: long, dense, overlapping
				var io := _rot(Vector3(tt * ln + randf_range(-0.05, 0.05), WALL_H - 0.05, -WALL_T * 0.5 - 0.06), yaw)
				_foliage("ivy", Vector3(mid.x + io.x, io.y, mid.y + io.z), yaw + randf_range(-0.18, 0.18), randf_range(1.3, 2.0))
				# back row: shorter, staggered, fills the gaps so no concrete shows through
				if k % 2 == 0:
					var io2 := _rot(Vector3((tt + 0.03) * ln, WALL_H + 0.02, -WALL_T * 0.5 + 0.02), yaw)
					_foliage("ivy", Vector3(mid.x + io2.x, io2.y, mid.y + io2.z), yaw + randf_range(-0.25, 0.25), randf_range(0.9, 1.4))
	# corner reinforcement columns + dense ivy at the octagon corners
	for i in VERTS.size():
		var v: Vector2 = VERTS[i]
		var vn := v.normalized()
		_box(Vector3(0.7, WALL_H + 0.1, 0.7), Vector3(v.x, (WALL_H + 0.1) * 0.5, v.y), "concrete", atan2(vn.y, vn.x))
		for k in 9:
			var inward := Vector2(v.x, v.y) - vn * 0.5
			_foliage("ivy", Vector3(inward.x + randf_range(-0.4, 0.4), WALL_H - 0.05, inward.y + randf_range(-0.4, 0.4)),
					randf_range(0, TAU), randf_range(1.2, 2.0))

# =============================================================================
# 6. Canopy and central portal
# =============================================================================
# The centrepiece at (0,0). Key dimensions (metres): four steel posts 0.46 m
# square at X,Z = +-4.65 (collided), eave/beam height 3.75 m = the minimum head
# clearance under the roof, apex 5.0 m, fabric overhang 0.65 m beyond the posts
# -> ~10.6 m square footprint. The fabric is generated here as a 28x28 grid
# (_build_fabric_roof) rather than loaded from a GLB so sag, ridges and the eave
# scallops stay editable from these few numbers. Below the roof sits the concrete
# portal: two 0.8 m pillars with a 2.4 m walk-through opening and a lintel at
# 2.9 m — kept clear so it is a real line-of-sight break, not a block.
func _build_canopy_and_portal() -> void:
	# HERO: tensioned-fabric pavilion as in the references — four dark posts with
	# base/connection plates, perimeter eave beams, four diagonal ridge beams that
	# show above and below the fabric, an apex hub, under-mounted light bars, and a
	# sagging four-quadrant orange fabric with a raised centre and scalloped eaves.
	var post := 4.65          # post centre offset (footprint ~10.6 m with the eave overhang)
	var under := 3.75         # eave / beam height = minimum clearance
	var ridge := 5.0          # apex height
	var half := post + 0.65   # fabric overhangs the posts
	for sx in [-post, post]:
		for sz in [-post, post]:
			_box(Vector3(0.46, under, 0.46), Vector3(sx, under * 0.5, sz), "steel")
			_box(Vector3(0.84, 0.08, 0.84), Vector3(sx, 0.04, sz), "steel", 0.0, false)          # base plate
			_box(Vector3(0.72, 0.3, 0.72), Vector3(sx, under + 0.05, sz), "steel", 0.0, false)    # connection plate
			for k in 4:                                                                           # plate bolts
				var a := k * PI / 2.0 + PI / 4.0
				_cyl(0.035, 0.05, Vector3(sx + cos(a) * 0.27, under + 0.22, sz + sin(a) * 0.27), "steel", false, 8)
	for s in [-1, 1]:                                                                              # eave beams
		_box(Vector3(post * 2 + 0.46, 0.24, 0.24), Vector3(0, under, s * post), "steel", 0.0, false)
		_box(Vector3(0.24, 0.24, post * 2), Vector3(s * post, under, 0), "steel", 0.0, false)
	var apex := Vector3(0, ridge, 0)
	for sx in [-post, post]:
		for sz in [-post, post]:
			var a := Vector3(sx, under + 0.1, sz)
			_beam(a, apex + Vector3(0, 0.02, 0), 0.2, "steel")                                    # ridge beam
			var m := (a + apex) * 0.5
			var ly := atan2(sx, sz)
			_box(Vector3(0.14, 0.08, 1.5), m + Vector3(0, -0.26, 0), "steel", ly, false)          # light housing
			_box(Vector3(0.1, 0.05, 1.4), m + Vector3(0, -0.3, 0), "light", ly, false)             # light bar
	_box(Vector3(0.62, 0.34, 0.62), apex + Vector3(0, 0.02, 0), "steel", PI / 4.0, false)        # apex hub
	_build_fabric_roof(half, under + 0.1, ridge - 0.02)
	# central concrete portal — pillars + stepped lintel, real 2.4 m walk-through,
	# kept CLEAR in front/behind so the opening reads from ground level
	var ph := 2.9
	var pt := 0.8
	var door := 2.4
	var pc := door * 0.5 + pt * 0.5
	for sx in [-pc, pc]:
		_box(Vector3(pt, ph, pt + 0.3), Vector3(sx, ph * 0.5, 0), "concrete")          # thick pillars
		_box(Vector3(pt + 0.2, 0.3, pt + 0.5), Vector3(sx, 0.15, 0), "concrete")        # pillar bases
		_box(Vector3(pt + 0.2, 0.25, pt + 0.5), Vector3(sx, ph - 0.1, 0), "concrete2")  # capitals
		for sz in [-1, 1]:                                                              # cast seams
			_box(Vector3(pt + 0.02, 0.03, 0.02), Vector3(sx, ph * 0.5, sz * (pt * 0.5 + 0.15)), "basegrime", 0.0, false)
	_box(Vector3(door + pt * 2 + 0.2, 0.55, pt + 0.3), Vector3(0, ph + 0.275, 0), "concrete")
	_box(Vector3(door + pt * 2 - 0.3, 0.3, pt + 0.1), Vector3(0, ph + 0.7, 0), "concrete2")
	for sx in [-pc, pc]:                                                                # angled buttresses
		_box(Vector3(0.5, 1.2, 0.5), Vector3(sx + sign(sx) * 0.55, 0.45, 0), "concrete2", 0.0)
	# team banners hanging from the eave beams (teal on -X, orange on +X)
	for sz in [-1.5, 1.5]:
		_banner(Vector3(-post + 0.55, under, sz), "teal")
		_banner(Vector3(post - 0.55, under, sz), "orange")

func _banner(top: Vector3, team_key: String) -> void:
	# vertical fabric banner with a white diagonal stripe + hem, on a hanger bar
	var h := 2.1
	_box(Vector3(0.1, 0.1, 1.3), top + Vector3(0, -0.05, 0), "steel", 0.0, false)
	var c := top + Vector3(0, -h * 0.5 - 0.1, 0)
	_box(Vector3(0.05, h, 1.15), c, team_key, 0.0, false, null)
	_box(Vector3(0.06, 0.25, 1.15), top + Vector3(0, -h - 0.1, 0), "paint", 0.0, false)
	for f in [-1, 1]:
		_stripe(c, PI / 2.0, f * 0.03, -0.5, -0.65, 0.65, 0.26, 0.7, "paint")
	# small pleats near the top so it reads as hung cloth
	for k in 3:
		_box(Vector3(0.07, 0.3, 0.03), c + Vector3(0, h * 0.5 - 0.2, -0.3 + k * 0.3), team_key, 0.0, false)

func _build_fabric_roof(half: float, eave: float, ridge: float) -> void:
	# Hip-roof fabric surface on a 28x28 grid: straight along the four diagonal
	# ridges (where the beams are), with catenary-like sag inside each quadrant and
	# scalloped eaves between the posts. Two-sided matte fabric material.
	var n := 28
	var sagmax := 0.42
	var grid := []
	for i in n + 1:
		var row := []
		for j in n + 1:
			var x := (i / float(n) - 0.5) * 2.0 * half
			var z := (j / float(n) - 0.5) * 2.0 * half
			var ax := absf(x) / half
			var az := absf(z) / half
			var mx := maxf(ax, az)
			var r := 1.0 - mx                                            # 1 apex .. 0 eave
			var t := 0.0 if mx < 1e-4 else minf(ax, az) / mx             # 1 on ridge diagonals
			var sag := sagmax * (1.0 - t) * clampf(1.0 - r * 1.15, 0.0, 1.0)
			row.append(Vector3(x, eave + (ridge - eave) * r - sag, z))
		grid.append(row)
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in n:
		for j in n:
			var p00: Vector3 = grid[i][j]
			var p10: Vector3 = grid[i + 1][j]
			var p11: Vector3 = grid[i + 1][j + 1]
			var p01: Vector3 = grid[i][j + 1]
			for tri in [[p00, p10, p11], [p00, p11, p01]]:
				for v in tri:
					st.add_vertex(v)
	st.generate_normals()
	var mi := MeshInstance3D.new()
	mi.mesh = st.commit()
	mi.material_override = _mat["canopy_fab"]
	add_child(mi)
	# tension cable along the scalloped eave (thin, dark)
	for i in n:
		for e in 4:
			var a: Vector3
			var b: Vector3
			match e:
				0: a = grid[i][0]; b = grid[i + 1][0]
				1: a = grid[i][n]; b = grid[i + 1][n]
				2: a = grid[0][i]; b = grid[0][i + 1]
				_: a = grid[n][i]; b = grid[n][i + 1]
			_beam(a, b, 0.045, "steel")

# =============================================================================
# 7. Spawn zones
# =============================================================================
# Teal team spawns at x = -18, orange at x = +18 (both at z = 0), each facing the
# arena centre. Each gateway is a recessed bay carved OUTWARD into the perimeter
# (opening 5.5 m wide x 3.4 m high, 2.1 m deep) so it adds depth without eating
# play space or opening a path outside. The bay's side/back walls collide; the
# painted frame, chamfer fills, floor marking and light are decorative. Gameplay
# code should spawn players inside the bay footprint (|z| < 2.7, x between the
# back wall and the opening) facing +-X.
func _build_spawns() -> void:
	_spawn(Vector3(-18, 0, 0), "teal", 1)
	_spawn(Vector3(18, 0, 0), "orange", -1)

func _spawn(c: Vector3, team_key: String, facing: int) -> void:
	# A genuine recessed entrance bay. The opening faces the arena centre (+facing·X);
	# the recess is carved OUTWARD (-facing·X) into the perimeter so it adds depth
	# without opening a path outside or eating play space.
	var w := 5.5                       # opening width (Z)
	var oh := 3.4                      # opening height
	var depth := 2.1
	var t := 0.4
	var dout := -facing
	var backx := c.x + dout * depth
	var midx := c.x + dout * depth * 0.5
	var frontx := c.x + facing * 0.1
	# recessed side return walls
	for sz in [-(w * 0.5 + t * 0.5), w * 0.5 + t * 0.5]:
		_box(Vector3(depth, oh, t), Vector3(midx, oh * 0.5, sz), "concrete2")
	# recessed back wall (closes the bay — no exit) and ceiling
	_box(Vector3(t, oh, w + t * 2.0), Vector3(backx, oh * 0.5, 0), "concrete2")
	_box(Vector3(depth + t, t, w + t * 2.0), Vector3(midx, oh + t * 0.5, 0), "concrete")
	# proud entrance frame: jambs + lintel at the opening plane
	for sz in [-(w * 0.5 + t * 0.5), w * 0.5 + t * 0.5]:
		_box(Vector3(t * 1.4, oh + t, t * 1.4), Vector3(frontx, (oh + t) * 0.5, sz), "concrete")
	_box(Vector3(t * 1.4, t * 1.5, w + t * 3.2), Vector3(frontx, oh + t * 0.25, 0), "concrete")
	# thick team-coloured entrance frame (jambs + header) wrapping the opening edge,
	# with a white trim line — reads instantly as the team gateway
	# Team-coloured portal frame with CHAMFERED corners (octagonal opening as in the
	# references), white diagonal stripes on the jambs, and a white top line.
	var fx := frontx + facing * 0.42                  # proud of the concrete frame (whose face is at +0.38)
	var fy := -facing * PI / 2.0                      # yaw whose local +z normal faces the arena
	var jw := 0.6                                     # jamb face width
	for s in [-1, 1]:
		var jc := Vector3(fx, (oh + 0.2) * 0.5, s * (w * 0.5 + jw * 0.5))
		_box(Vector3(0.22, oh + 0.2, jw), jc, team_key, 0.0, false)
		_stripe(Vector3(fx, oh * 0.5, s * (w * 0.5 + jw * 0.5)), fy, 0.13, -jw * 0.5 + 0.03, -0.7, 0.7, 0.2, jw - 0.26, "paint")
	_box(Vector3(0.22, 0.6, w + jw * 2.0), Vector3(fx, oh + 0.1, 0), team_key, 0.0, false)
	_box(Vector3(0.1, 0.14, w + jw * 2.0), Vector3(fx + facing * 0.08, oh + 0.46, 0), "paint", 0.0, false)
	var nx := facing * 0.125
	for s in [-1, 1]:                                  # top chamfers (big) + bottom chamfers (small)
		var zi: float = s * w * 0.5
		_flat_poly([Vector3(fx + nx, oh - 0.2, zi), Vector3(fx + nx, oh - 0.2 - 0.75, zi), Vector3(fx + nx, oh - 0.2, zi - s * 0.75)], team_key)
		_flat_poly([Vector3(fx + nx, 0.15, zi), Vector3(fx + nx, 0.15 + 0.42, zi), Vector3(fx + nx, 0.15, zi - s * 0.42)], team_key)
	# back wall: concrete with a team paint splash (not a flat solid-colour panel)
	var bwx := backx + facing * (t * 0.5)
	_stripe(Vector3(bwx, oh * 0.5, 0), fy, 0.02, -1.9, -1.3, 1.1, 0.7, 1.1, team_key)
	_stripe(Vector3(bwx, oh * 0.5, 0), fy, 0.02, -0.9, -1.3, 1.1, 0.18, 1.1, "paint")
	_stripe(Vector3(bwx, oh * 0.5, 0), fy, 0.02, 0.6, -1.3, 0.2, 0.5, 0.6, team_key)
	# linear overhead light in a steel housing, recessed into the ceiling
	_box(Vector3(depth * 0.7 + 0.1, 0.12, w * 0.55 + 0.1), Vector3(midx, oh - 0.1, 0), "steel", 0.0, false)
	_box(Vector3(depth * 0.7, 0.16, w * 0.55), Vector3(midx, oh - 0.16, 0), "light", 0.0, false)
	# painted team floor with the white inset rectangle (reference)
	_box(Vector3(depth + 2.0, 0.02, w - 0.3), Vector3(midx + facing * 1.0, 0.151, 0), team_key, 0.0, false)
	var pl := depth + 2.0
	for o in [[Vector3(pl * 0.7, 0.02, 0.12), Vector3(0, 0, (w - 0.3) * 0.36)], [Vector3(pl * 0.7, 0.02, 0.12), Vector3(0, 0, -(w - 0.3) * 0.36)], [Vector3(0.12, 0.02, (w - 0.3) * 0.72), Vector3(pl * 0.35, 0, 0)], [Vector3(0.12, 0.02, (w - 0.3) * 0.72), Vector3(-pl * 0.35, 0, 0)]]:
		_box(o[0], Vector3(midx + facing * 1.0, 0.16, 0) + o[1], "paint", 0.0, false)
	# olive equipment cases staged inside the bay (replaces the abstract spheres)
	for s in [-1, 1]:
		var cp := Vector3(backx + facing * 0.9, 0, s * 1.6)
		_mesh("case", cp, "", 0.2 * s)
		_collider_box(Vector3(1.1, 0.6, 0.6), cp + Vector3(0, 0.3, 0))

# =============================================================================
# 8. Cover, collision and props
# =============================================================================
# Layout is mirror-symmetric about the centre so neither team gets an advantage.
# Cover classes and the collision each carries (centre positions in metres):
#   tall barrier   2.8 x 0.85 x 2.10 m  at (+-12, +-9)        — full standing cover
#   low barrier    3.0 x 0.65 x 1.28 m  at (+-8, +-4), (0, +-10) — crouch cover only
#   L barrier      1.45 m high          at (+-15, +-8)         — two boxes
#   loophole wall  3.0 x 0.7 x 2.10 m   at (-5,-9.5), (5,9.5)  — four boxes, real slot
#   sandbags       ~1.26 m              at (+-6.7, +-5.5) + two curved arcs
#   crates/barrels/tarp pallet          near the walls         — box / cylinder colliders
# The positions used by the raycast tests (section 14) are the low barrier at
# (0, 10) and the tall barrier at (12, 9); do not move those without updating
# the tests. Visual meshes are Blender GLBs; collision is always the simple
# box/cylinder sized to the stated gameplay dimensions, never the mesh itself.
func _build_obstacles() -> void:
	for p in [Vector2(-12, -9), Vector2(-12, 9), Vector2(12, -9), Vector2(12, 9)]:
		var yaw := 0.0 if abs(p.y) > abs(p.x) else PI / 2
		_barrier(Vector3(p.x, 0, p.y), 2.8, 0.85, 2.10, yaw, "teal" if p.x < 0 else "orange")
	for p in [Vector2(-8, -4), Vector2(-8, 4), Vector2(8, -4), Vector2(8, 4), Vector2(0, -10), Vector2(0, 10)]:
		var yaw := 0.0 if abs(p.y) >= abs(p.x) else PI / 2
		var st := "" if abs(p.x) < 1.0 else ("teal" if p.x < 0 else "orange")
		_barrier(Vector3(p.x, 0, p.y), 3.0, 0.65, 1.28, yaw, st)
	for p in [Vector2(-15, -8), Vector2(-15, 8), Vector2(15, -8), Vector2(15, 8)]:
		var stripe := "teal" if p.x < 0 else "orange"
		_barrier_L(Vector3(p.x, 0, p.y), 1.45, stripe)
	for p in [Vector2(-6.7, -5.5), Vector2(-6.7, 5.5), Vector2(6.7, -5.5), Vector2(6.7, 5.5)]:
		_sandbag_pile(Vector3(p.x, 0, p.y))
	for p in [Vector2(-16.5, 2.5), Vector2(16.5, -2.5), Vector2(-9, 13), Vector2(9, -13)]:
		_crate_cluster(Vector3(p.x, 0, p.y))
	for p in [Vector2(-17, -8), Vector2(17, 8), Vector2(-3, 14), Vector2(3, -14)]:
		_barrel_cluster(Vector3(p.x, 0, p.y))
	# reference cover that was missing — placed in open floor, away from the tested
	# cover positions and the main lanes (existing cover is untouched)
	for p in [Vector2(-5, -9.5), Vector2(5, 9.5)]:
		_barrier_loophole(Vector3(p.x, 0, p.y), "teal" if p.x < 0 else "orange")
	_tarp_pallet(Vector3(-6.2, 0, 12.4), 0.3)        # beside the olive crate cluster, off the hero sightline
	_sandbag_arc(Vector3(-13.5, 0, 13.5), -PI / 4.0)
	_sandbag_arc(Vector3(13.5, 0, -13.5), PI * 0.75)
	# the two near-wall crate clusters use the dark reinforced style (reference mix)
	_crate_cluster(Vector3(-13.5, 0, -5.5), "dark")
	_crate_cluster(Vector3(13.5, 0, 5.5), "dark")

func _barrier_loophole(c: Vector3, stripe: String) -> void:
	# 2.10 m standing cover with a REAL horizontal firing slot (1.3 x 0.42 m, sill at
	# 1.35 m), built from four solid concrete blocks that each carry collision —
	# rays and bullets pass only through the actual slot. Axis-aligned along X.
	var ln := 3.0
	var t := 0.7
	var h := 2.10
	var sw := 1.3
	var sill := 1.35
	var sh := 0.42
	var pw := (ln - sw) * 0.5
	for side in [-1, 1]:
		_box(Vector3(pw, h, t), c + Vector3(side * (sw * 0.5 + pw * 0.5), h * 0.5, 0), "concrete")
	_box(Vector3(sw + 0.02, sill, t), c + Vector3(0, sill * 0.5, 0), "concrete")
	var top := h - (sill + sh)
	_box(Vector3(sw + 0.02, top, t), c + Vector3(0, sill + sh + top * 0.5, 0), "concrete")
	for face in [-1, 1]:
		_box(Vector3(sw + 0.1, 0.05, 0.03), c + Vector3(0, sill - 0.02, face * (t * 0.5 + 0.015)), "concrete2", 0.0, false)   # sill lip
		_stripe(c, 0.0, face * (t * 0.5 + 0.012), -ln * 0.5 + 0.12, 0.06, h - 0.08, 0.34, 0.5, stripe)

func _tarp_pallet(c: Vector3, yaw := 0.0) -> void:
	# wooden pallet + stacked cargo under a draped olive tarp (reference centre prop)
	for k in 5:
		_box(Vector3(1.3, 0.04, 0.16), c + _rotg(Vector3(0, 0.14, -0.44 + k * 0.22), yaw), "wood", yaw, false)
	for sx in [-0.55, 0.0, 0.55]:
		_box(Vector3(0.1, 0.1, 1.1), c + _rotg(Vector3(sx, 0.05, 0), yaw), "wood", yaw, false)
	_box(Vector3(1.2, 0.95, 1.0), c + _rotg(Vector3(0, 0.16 + 0.475, 0), yaw), "crate_dark", yaw, false)
	_box(Vector3(1.26, 0.72, 1.06), c + _rotg(Vector3(0, 0.8, 0), yaw), "tarp", yaw, false)
	_box(Vector3(1.34, 0.26, 1.14), c + _rotg(Vector3(0, 1.26, 0), yaw), "tarp", yaw, false)
	for s in [-1, 1]:                                                      # drooping side flaps
		var f := _box(Vector3(0.05, 0.78, 1.0), c + _rotg(Vector3(s * 0.7, 0.72, 0), yaw), "tarp", yaw, false)
		f.rotation.z = -s * 0.1
		var g := _box(Vector3(1.0, 0.78, 0.05), c + _rotg(Vector3(0, 0.72, s * 0.6), yaw), "tarp", yaw, false)
		g.rotation.x = s * 0.1
	_collider_box(Vector3(1.36, 1.42, 1.16), c + Vector3(0, 0.71, 0), yaw)

func _sandbag_arc(c: Vector3, dir: float) -> void:
	# curved sandbag wall: three piles along an arc (reference top-down), 1.25 m cover
	var r := 2.4
	for k in 3:
		var a := dir + (k - 1) * 0.55
		var p := c + Vector3(cos(a) * r, 0, sin(a) * r)
		var gy := -a - PI / 2.0                       # pile's long axis along the arc tangent
		_mesh("sandbag_pile", p, "sand", gy)
		_collider_box(Vector3(2.3, 1.26, 0.9), p + Vector3(0, 0.63, 0), gy)

func _barrier(c: Vector3, ln: float, t: float, h: float, yaw: float, stripe: String) -> void:
	# beveled cast-concrete block (Blender mesh, chamfered edges + surface noise)
	# with box collision; steel coping cap and optional team stripe on top.
	var glb := "barrier_tall" if h > 1.5 else "barrier_low"
	_mesh(glb, c, "concrete", yaw)
	_collider_box(Vector3(ln, h, t), c + Vector3(0, h * 0.5, 0), yaw)
	if stripe != "":
		# ONE diagonal team stripe per face (as in the references); bare beveled
		# concrete everywhere else
		var sw := 0.44 if h > 1.5 else 0.36
		for face in [-1, 1]:
			_stripe(c, yaw, face * (t * 0.5 + 0.012), -ln * 0.22, 0.06, h - 0.08, sw, h * 0.32, stripe)
	# recessed grip slots on tall barriers (dark insets near the top)
	if h > 1.5:
		for gx in [-ln * 0.27, ln * 0.27]:
			for face in [-1, 1]:
				var go := _rot(Vector3(gx, h - 0.42, face * (t * 0.5 + 0.005)), yaw)
				_box(Vector3(0.5, 0.15, 0.04), c + go, "basegrime", yaw, false)

func _barrier_L(c: Vector3, h: float, stripe: String) -> void:
	_mesh("barrier_L", c, "concrete", 0.0)
	_collider_box(Vector3(2.6, h, 0.7), c + Vector3(0.95, h * 0.5, 0))
	_collider_box(Vector3(0.7, h, 2.0), c + Vector3(-0.6, h * 0.5, 0.95))
	for face in [-1, 1]:
		_stripe(c + Vector3(0.95, 0, 0), 0.0, face * 0.362, -0.55, 0.06, h - 0.08, 0.36, 0.42, stripe)

func _sandbag_pile(c: Vector3) -> void:
	# detailed Blender mesh (wrinkled, stacked) + single crouch-cover collision box
	_mesh("sandbag_pile", c, "sand", randf_range(-0.1, 0.1))
	_collider_box(Vector3(2.3, 1.26, 0.9), c + Vector3(0, 0.63, 0))

func _supply_crate(c: Vector3, style: String, yaw := 0.0, scale := 1.0) -> void:
	# "olive": equipment case in the reference's deeper olive; "dark": reinforced
	# supply crate — dark panels, steel edges and cross-bracing on every side.
	var inst := _mesh("crate", c, "", yaw, scale)
	var meshes: Array = []
	_collect_meshes(inst, meshes)
	for mi in meshes:
		if not mi.mesh:
			continue
		for si in mi.mesh.get_surface_count():
			var sm: Material = mi.mesh.surface_get_material(si)
			var mn := sm.resource_name.to_lower() if sm else ""
			if style == "dark":
				mi.set_surface_override_material(si, _mat["crate_dark"] if "olive" in mn else _mat["steel"])
			elif "olive" in mn:
				mi.set_surface_override_material(si, _mat["olive_dark"])
	if style == "dark":
		var s := 0.95 * scale
		for face in 4:
			var yf := yaw + face * PI / 2.0
			for d in [-1, 1]:
				var bar := _box(Vector3(s * 1.12, 0.055, 0.03), c + _rotg(Vector3(0, s * 0.5, s * 0.5 + 0.02), yf), "steel", yf, false)
				bar.rotation.z = d * PI / 4.0
	_collider_box(Vector3(0.95, 0.95, 0.95) * scale, c + Vector3(0, 0.475 * scale, 0), yaw)

func _crate_cluster(c: Vector3, style := "olive") -> void:
	_supply_crate(c + Vector3(-0.55, 0, 0), style, 0.12)
	_supply_crate(c + Vector3(0.55, 0, 0.3), style, -0.08)
	_supply_crate(c + Vector3(0, 0.95, 0.1), style, 0.2, 0.62)      # stacked on top

func _barrel_cluster(c: Vector3) -> void:
	for o in [Vector3(0, 0, 0), Vector3(0.62, 0, 0.15), Vector3(0.3, 0, -0.55)]:
		_mesh("barrel", c + o, "barrel", randf_range(0, TAU))     # ridged barrel mesh
		_collider_cyl(0.3, 0.9, c + o + Vector3(0, 0.45, 0))

# =============================================================================
# 9. Trees and environment (outside the walls)
# =============================================================================
# Exterior dressing only — nothing here is reachable in play. Trees, bushes and
# rocks are CC0 Quaternius "Stylized Nature MegaKit" GLBs (assets/models/nature/,
# see CREDITS.txt) placed with a FIXED random seed so every launch produces the
# same forest. TREE_NATIVE records each model's native height so _nature() can
# scale a tree to a requested height in metres. Mid/far belts cast no shadows
# (big fill-rate saving, invisible from inside the arena).
const TREE_SET := ["CommonTree_1", "CommonTree_2", "CommonTree_3", "CommonTree_4", "CommonTree_5"]
const TREE_NATIVE := {"CommonTree_1": 7.26, "CommonTree_2": 7.64, "CommonTree_3": 9.43,
	"CommonTree_4": 9.44, "CommonTree_5": 7.01}

func _build_outer_trees() -> void:
	# A real wooded belt of CC0 Quaternius "Stylized Nature MegaKit" trees (dense
	# broadleaf, baked bark + alpha-leaf materials) in three depth layers so the
	# surroundings read as woodland, not a few props on a field. Placement is seeded
	# and irregular; the ±X spawn approaches (|z|<6 at the sides) are kept clear.
	seed(20260429)
	var clear := func(p: Vector3) -> bool:
		# keep sightlines out of both spawn tunnels (±X walls) open
		return p.x < -20.0 and absf(p.z) < 6.5 or p.x > 20.0 and absf(p.z) < 6.5
	# layer 1 — near belt hugging the wall, crowns reaching over the parapet
	var n_near := 20
	for i in n_near:
		var a := TAU * i / float(n_near) + randf_range(-0.12, 0.12)
		var rad := randf_range(23.0, 29.0)
		var p := Vector3(cos(a) * rad, 0, sin(a) * rad * 0.9)
		if clear.call(p):
			continue
		_nature(TREE_SET[i % TREE_SET.size()], p, randf_range(0, TAU), randf_range(6.5, 10.5))
	# layer 2 — mid belt, fuller and taller, filling gaps behind the near trees
	# (shadows off beyond the near belt: saves fill-rate, barely visible from inside)
	var n_mid := 16
	for i in n_mid:
		var a := TAU * i / float(n_mid) + randf_range(-0.2, 0.2) + 0.2
		var rad := randf_range(32.0, 42.0)
		var p := Vector3(cos(a) * rad, 0, sin(a) * rad)
		_nature(TREE_SET[(i + 2) % TREE_SET.size()], p, randf_range(0, TAU), randf_range(8.0, 12.0), false)
	# layer 3 — distant tree line for a wooded horizon
	var n_far := 18
	for i in n_far:
		var a := TAU * i / float(n_far) + randf_range(-0.25, 0.25)
		var rad := randf_range(48.0, 66.0)
		var p := Vector3(cos(a) * rad, 0, sin(a) * rad)
		_nature(TREE_SET[(i + 1) % TREE_SET.size()], p, randf_range(0, TAU), randf_range(7.0, 11.0), false)
	# shrubs + ferns in irregular clusters near the wall base (natural transition)
	for c in 11:
		var a := randf_range(0, TAU)
		var rad := randf_range(21.0, 27.0)
		var base := Vector3(cos(a) * rad, 0, sin(a) * rad * 0.92)
		if clear.call(base):
			continue
		# Bush_Common_Flowers carries the correct GREEN leaf atlas/UVs (Bush_Common's
		# atlas is autumn-red); its flower surface is recoloured green in _nature()
		# (Fern_1 dropped: 2.7 m native + alpha-blend material read as giant ghosts)
		for k in randi_range(2, 4):
			var off := Vector3(randf_range(-2.2, 2.2), 0, randf_range(-2.2, 2.2))
			var sp := base + off
			if absf(sp.x) < 22.0 and absf(sp.z) < 18.0:      # never inside the wall footprint
				continue
			_nature("Bush_Common_Flowers", sp, randf_range(0, TAU), randf_range(0.9, 1.6))
	# scattered boulders + pebbles for ground detail (irregular, clustered)
	for c in 8:
		var a := randf_range(0, TAU)
		var rad := randf_range(22.0, 34.0)
		var base := Vector3(cos(a) * rad, 0, sin(a) * rad)
		_nature(["Rock_Medium_1", "Rock_Medium_2", "Rock_Medium_3"][randi() % 3], base,
			randf_range(0, TAU), randf_range(0.8, 1.8))
		if randf() < 0.6:
			_nature(["Pebble_Round_1", "Pebble_Round_3", "Pebble_Round_5"][randi() % 3],
				base + Vector3(randf_range(-1.6, 1.6), 0, randf_range(-1.6, 1.6)),
				randf_range(0, TAU), randf_range(0.8, 1.6))

func _nature(name: String, base: Vector3, yaw: float, scale_or_height: float, cast_shadow := true) -> Node3D:
	# Instance a MegaKit GLB keeping its baked (natural) materials. Trees take a
	# target height in metres; props take a raw scale factor.
	var inst: Node3D = _load_scene("nature/" + name).instantiate()
	var s := scale_or_height
	if TREE_NATIVE.has(name):
		s = scale_or_height / float(TREE_NATIVE[name])
	# sit on the sculpted terrain (trunks meet the ground on the hills too); rocks
	# are sunk a little so they read as bedded in, not resting on top
	var gy := _terrain_height(base.x, base.z)
	var sink := 0.12 * s if (name.begins_with("Rock") or name.begins_with("Pebble")) else 0.04
	inst.position = Vector3(base.x, gy - sink, base.z)
	inst.rotation.y = yaw
	inst.scale = Vector3(s, s, s)
	add_child(inst)
	var recolor := name.begins_with("Bush")   # keep green leaves, turn flowers/red atlas green
	if recolor or not cast_shadow:
		var meshes: Array = []
		_collect_meshes(inst, meshes)
		for mi in meshes:
			if not cast_shadow:
				mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			if recolor and mi.mesh:
				for si in mi.mesh.get_surface_count():
					var sm: Material = mi.mesh.surface_get_material(si)
					var mn := sm.resource_name.to_lower() if sm else ""
					if "flower" in mn or "twisted" in mn:
						mi.set_surface_override_material(si, _mat["bushgreen"])
	return inst

func _tree(base: Vector3, variant: String, height: float) -> void:
	# LEGACY (currently unused): the earlier procedural Blender trees tree_a/b/c
	# from pipeline/gen_vegetation.py. Superseded by the MegaKit trees above but
	# kept so those generator outputs remain loadable if someone switches back.
	# Blender branching tree: tapered-branch wood + alpha-cutout leaf cards, as
	# two named objects in the GLB, scaled to the requested height.
	var native := {"tree_a": 8.3, "tree_b": 11.3, "tree_c": 5.1}
	_foliage(variant, base, randf_range(0, TAU), height / float(native.get(variant, 8.3)))

func _foliage(name: String, base: Vector3, yaw: float, scale: float) -> Node3D:
	# Instance a GLB with named "*_wood" + "*_leaves" objects; apply bark to the
	# wood and the shared alpha-scissor leaf card material to the foliage.
	var inst: Node3D = _load_scene(name).instantiate()
	inst.position = base
	inst.rotation.y = yaw
	inst.scale = Vector3(scale, scale, scale)
	add_child(inst)
	var meshes: Array = []
	_collect_meshes(inst, meshes)
	for mi in meshes:
		if not mi.mesh:
			continue
		var nm := str(mi.name).to_lower()
		var any_leaf := false
		# Override PER SURFACE: a GLB may merge wood + leaves into one mesh with
		# several surfaces, so a single material_override would paint the whole
		# tree one colour. Decide each surface from its own material name.
		for si in mi.mesh.get_surface_count():
			var sm: Material = mi.mesh.surface_get_material(si)
			var mn := sm.resource_name.to_lower() if sm else ""
			var is_leaf := ("leaf" in mn) or ("leav" in mn) or ("ivy" in mn) \
				or ("leaf" in nm) or ("leav" in nm) or ("ivy" in nm)
			mi.set_surface_override_material(si, _mat["leaf_card"] if is_leaf else _mat["bark"])
			any_leaf = any_leaf or is_leaf
		# leaf crowns shouldn't self-shadow (dense cards shadowing each other go dark)
		if any_leaf:
			mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return inst

# =============================================================================
# 10. Placeholder test humans (1.80 m) — TEMPORARY, not character art
# =============================================================================
# Primitive mannequins (nodes named TEMP_Character) standing at the exact spots
# the cover tests probe, so a screenshot shows what the raycasts measure. They
# have no collision and no gameplay role; real characters replace them later.
func _build_test_humans() -> void:
	_test_human(Vector3(-4, 0, 13), C_TEAL, false)      # open, scale reference
	_test_human(Vector3(2.2, 0, 10), C_TEAL, false)     # standing behind low cover -> exposed
	_test_human(Vector3(-2.2, 0, 10), C_ORANGE, true)   # crouching behind low cover -> hidden
	_test_human(Vector3(13.6, 0, 9), C_ORANGE, false)   # standing behind tall cover -> hidden
	_test_human(Vector3(10.4, 0, 10.8), C_TEAL, false)  # scale ref beside the 2.10 m tall wall

func _test_human(pos: Vector3, team: Color, crouch: bool) -> void:
	var root := Node3D.new()
	root.name = "TEMP_Character"
	root.position = pos
	add_child(root)
	var skin := StandardMaterial3D.new()
	skin.albedo_color = Color("2b2d31")
	skin.roughness = 0.7
	var accent := StandardMaterial3D.new()
	accent.albedo_color = team
	accent.roughness = 0.5
	var head_m := StandardMaterial3D.new()
	head_m.albedo_color = Color("c8a98a")
	head_m.roughness = 0.6
	var helm_m := StandardMaterial3D.new()
	helm_m.albedo_color = Color("3a4030"); helm_m.roughness = 0.55
	var gun_m := StandardMaterial3D.new()
	gun_m.albedo_color = Color("1a1b1e"); gun_m.roughness = 0.45; gun_m.metallic = 0.3
	var sc := CROUCH_HEIGHT / PLAYER_HEIGHT if crouch else 1.0
	var legh := 0.85 * sc
	var torso := 0.62 * sc
	for sx in [-0.14, 0.14]:
		_prim_box(root, Vector3(0.22, legh, 0.26), Vector3(sx, legh * 0.5, 0), skin)
		_prim_box(root, Vector3(0.26, 0.1, 0.34), Vector3(sx, 0.05, 0.04), gun_m)  # boots
	_prim_box(root, Vector3(0.52, torso, 0.32), Vector3(0, legh + torso * 0.5, 0), skin)
	_prim_box(root, Vector3(0.54, 0.14, 0.34), Vector3(0, legh + torso * 0.7, 0), accent)  # team chest band
	_prim_box(root, Vector3(0.34, torso * 0.7, 0.16), Vector3(0, legh + torso * 0.55, -0.22), helm_m)  # backpack
	for sx in [-0.33, 0.33]:
		_prim_box(root, Vector3(0.16, torso * 0.9, 0.2), Vector3(sx, legh + torso * 0.5, 0), skin)
	var neck := legh + torso
	_prim_sphere(root, 0.13, Vector3(0, neck + 0.12, 0), head_m)                  # head
	_prim_box(root, Vector3(0.3, 0.14, 0.3), Vector3(0, neck + 0.2, 0), helm_m)   # helmet
	_prim_box(root, Vector3(0.34, 0.05, 0.05), Vector3(0, neck + 0.14, 0.17), gun_m)  # helmet brim
	# rifle: receiver + barrel + magazine + stock, held at the shoulder
	var muzzle := 0.78 if crouch else 1.35
	_prim_box(root, Vector3(0.5, 0.09, 0.1), Vector3(0.3, muzzle, 0.2), gun_m)      # receiver
	_prim_box(root, Vector3(0.46, 0.05, 0.05), Vector3(0.62, muzzle, 0.2), gun_m)   # barrel
	_prim_box(root, Vector3(0.08, 0.22, 0.08), Vector3(0.28, muzzle - 0.14, 0.2), gun_m)  # magazine
	_prim_box(root, Vector3(0.2, 0.1, 0.09), Vector3(-0.02, muzzle, 0.2), accent)   # stock (team)

func _prim_box(parent: Node, size: Vector3, pos: Vector3, mat: Material) -> void:
	var mi := MeshInstance3D.new()
	var b := BoxMesh.new()
	b.size = size
	mi.mesh = b
	mi.material_override = mat
	mi.position = pos
	parent.add_child(mi)

func _prim_sphere(parent: Node, r: float, pos: Vector3, mat: Material) -> void:
	var mi := MeshInstance3D.new()
	var s := SphereMesh.new()
	s.radius = r
	s.height = r * 2
	mi.mesh = s
	mi.material_override = mat
	mi.position = pos
	parent.add_child(mi)

# =============================================================================
# 11. Lighting
# =============================================================================
# Sky/ambient/tonemapping/SSAO/glow live on the WorldEnvironment node in
# scenes/arena.tscn (HDRI panorama sky). This function adds the lights: one
# shadow-casting sun at a low afternoon angle (long shadows, as in the
# references), a shadowless omni under the canopy so the orange underside is not
# a black hole, and a weak warm "sky fill" directional light from the opposite
# side so shaded concrete stays sandy instead of crushing to grey.
func _build_lighting() -> void:
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-45, -58, 0)
	sun.light_energy = 1.6
	sun.light_color = Color(1.0, 0.96, 0.88)
	sun.shadow_enabled = true
	sun.light_angular_distance = 0.7
	sun.shadow_bias = 0.03
	sun.shadow_normal_bias = 1.2
	sun.directional_shadow_max_distance = 140.0
	add_child(sun)
	var fill := OmniLight3D.new()
	fill.position = Vector3(0, 3.2, 0)
	fill.light_color = Color(1.0, 0.93, 0.8)
	fill.light_energy = 1.6
	fill.omni_range = 11.0
	fill.shadow_enabled = false
	add_child(fill)
	# soft cool sky fill from the opposite side so shadowed concrete lifts to a
	# warm-neutral instead of crushing to black (no shadow, low energy)
	var skyfill := DirectionalLight3D.new()
	skyfill.rotation_degrees = Vector3(-62, 128, 0)
	skyfill.light_energy = 0.72
	skyfill.light_color = Color(0.98, 0.93, 0.84)   # warm fill: shaded concrete stays sandy, not cold grey
	skyfill.shadow_enabled = false
	add_child(skyfill)
	_sun = sun

# =============================================================================
# 12. Rendering-quality presets
# =============================================================================
# Default ("high"): MSAA 4x, SSAO + SSIL, glow, soft sun shadows to 140 m.
# `-- --preset performance`: MSAA off, SSAO/SSIL/glow off, hard shadows to 55 m.
# Measured on an Intel iGPU at 1600x900: high ~37-39 FPS, performance 88-148 FPS
# (see qa/arena_final_candidate/performance_report.md). Performance is the
# preset that meets the >= 60 FPS target on integrated graphics.
func _apply_preset() -> void:
	var args := OS.get_cmdline_user_args()
	var perf := false
	var pi := args.find("--preset")
	if pi != -1 and pi + 1 < args.size() and args[pi + 1] == "performance":
		perf = true
	var we := get_node_or_null("WorldEnvironment") as WorldEnvironment
	if perf:
		if _sun:
			_sun.shadow_enabled = true
			_sun.directional_shadow_max_distance = 55.0
			_sun.light_angular_distance = 0.0
		if we and we.environment:
			var e := we.environment
			e.ssil_enabled = false
			e.ssao_enabled = false
			e.glow_enabled = false
		get_viewport().msaa_3d = Viewport.MSAA_DISABLED
		print("GRAPHICS_PRESET: performance")
	else:
		get_viewport().msaa_3d = Viewport.MSAA_4X
		print("GRAPHICS_PRESET: high")

# =============================================================================
# 13. Cameras, screenshot capture and benchmark
# =============================================================================
# Named fixed cameras used for QA so every screenshot is reproducible:
#   pos = camera position, tgt = look-at point (metres), fov in degrees, or
#   ortho = true + size for the top-down plan view. Ground-level views use
#   y = 1.63 (standing eye height) so scale reads as a player would see it.
# `-- --view <name>` renders ONE of these, saves arena_<name>.png after 55
# frames (lets shadows/SSAO settle) and quits. `-- --bench [--view <name>]`
# measures 300 frames instead. Without arguments the "overview" camera becomes
# a free-fly inspection camera.
const VIEWS := {
	"overview": {"pos": Vector3(30, 24, 34), "tgt": Vector3(0, 2, 0), "ortho": false, "fov": 52},
	"topdown": {"pos": Vector3(0, 60, 0.001), "tgt": Vector3(0, 0, 0), "ortho": true, "size": 46},
	"canopy_ground": {"pos": Vector3(0, 1.63, 17), "tgt": Vector3(0, 3.2, 0), "ortho": false, "fov": 66},
	"overview_b": {"pos": Vector3(-30, 24, -34), "tgt": Vector3(0, 2, 0), "ortho": false, "fov": 52},
	"teal_spawn": {"pos": Vector3(-10.5, 1.63, 0.2), "tgt": Vector3(-19.5, 1.5, 0), "ortho": false, "fov": 62},
	"orange_spawn": {"pos": Vector3(10.5, 1.63, -0.2), "tgt": Vector3(19.5, 1.5, 0), "ortho": false, "fov": 62},
	"cover_low": {"pos": Vector3(5.5, 1.63, 13.5), "tgt": Vector3(0, 1.0, 10), "ortho": false, "fov": 55},
	"cover_tall": {"pos": Vector3(5.0, 1.63, 14.5), "tgt": Vector3(12.0, 1.1, 9.2), "ortho": false, "fov": 50},
	"firing_port": {"pos": Vector3(-3.2, 1.63, -5.4), "tgt": Vector3(-5.0, 1.45, -9.5), "ortho": false, "fov": 52},
	"cargo": {"pos": Vector3(-9.2, 1.5, 16.2), "tgt": Vector3(-6.4, 0.8, 12.6), "ortho": false, "fov": 58},
	"props": {"pos": Vector3(-13.5, 1.5, 4.5), "tgt": Vector3(-16.5, 0.8, 2.5), "ortho": false, "fov": 52},
	"under_canopy": {"pos": Vector3(6.5, 1.63, 6.5), "tgt": Vector3(0, 2.2, 0), "ortho": false, "fov": 72},
	"tactical": {"pos": Vector3(-4, 14, 22), "tgt": Vector3(-4, 0, 13), "ortho": false, "fov": 42},
	"crate": {"pos": Vector3(-7.4, 1.3, 15.2), "tgt": Vector3(-9, 0.6, 13), "ortho": false, "fov": 48},
	"trees": {"pos": Vector3(-24, 5, 10), "tgt": Vector3(-31, 5, 0), "ortho": false, "fov": 60},
	"landscape": {"pos": Vector3(-42, 9, 40), "tgt": Vector3(-6, 1, 4), "ortho": false, "fov": 55},
	"walls": {"pos": Vector3(-14, 2.4, 8), "tgt": Vector3(-20.5, 3.4, 12), "ortho": false, "fov": 56},
	"cover": {"pos": Vector3(-8.2, 1.63, -4.6), "tgt": Vector3(-12.2, 1.2, -9.0), "ortho": false, "fov": 56},
	"cinematic": {"pos": Vector3(27, 4.0, 31), "tgt": Vector3(-1, 2.2, -1), "ortho": false, "fov": 40},
}

func _setup_camera() -> void:
	_cam = Camera3D.new()
	_cam.current = true
	add_child(_cam)
	var args := OS.get_cmdline_user_args()
	if "--bench" in args:
		_cap = true
		_bench = true
		var bi := args.find("--view")                 # `--bench --view <name>` benches that camera
		if bi != -1 and bi + 1 < args.size():
			_bench_view = args[bi + 1]
		_apply_view(_bench_view)
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		RenderingServer.frame_post_draw.connect(_on_bench)
		return
	if "--view" in args:
		_cap = true
		var idx := args.find("--view")
		if idx + 1 < args.size():
			_shot_name = args[idx + 1]
		_apply_view(_shot_name)
		RenderingServer.frame_post_draw.connect(_on_post_draw)
	else:
		_apply_view("overview")
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _apply_view(name: String) -> void:
	var v: Dictionary = VIEWS.get(name, VIEWS["overview"])
	if v.get("ortho", false):
		_cam.projection = Camera3D.PROJECTION_ORTHOGONAL
		_cam.size = v.get("size", 46)
		_cam.global_position = v["pos"]
		_cam.look_at(v["tgt"], Vector3(0, 0, -1))
	else:
		_cam.projection = Camera3D.PROJECTION_PERSPECTIVE
		_cam.fov = v.get("fov", 60)
		_cam.look_at_from_position(v["pos"], v["tgt"], Vector3.UP)

var _bench_view := "overview"
var _bench_last := 0
var _bench_worst := 0.0
var _bench_slow := 0

func _on_bench() -> void:
	# 40 warm-up frames, then 300 timed frames: average FPS plus frame-time
	# stability (worst frame, frames slower than 33.3 ms).
	_frames += 1
	var now := Time.get_ticks_usec()
	if _frames == 40:
		_bench_t0 = now
	elif _frames > 40 and _frames <= 340:
		var fdt := (now - _bench_last) / 1000.0
		_bench_worst = maxf(_bench_worst, fdt)
		if fdt > 33.3:
			_bench_slow += 1
		if _frames == 340:
			var dt := (now - _bench_t0) / 1_000_000.0
			print("BENCH_FPS: %.1f view=%s (300 frames @1600x900, uncapped) worst_frame=%.1fms frames_over_33ms=%d" % [300.0 / dt, _bench_view, _bench_worst, _bench_slow])
			get_tree().quit()
	_bench_last = now

func _on_post_draw() -> void:
	_frames += 1
	if _frames == 55:
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://arena_%s.png" % _shot_name)
		print("ARENA_CAPTURE view=%s size=%s" % [_shot_name, img.get_size()])
		get_tree().quit()

func _process(delta: float) -> void:
	if _cap or _cam == null:
		return
	if _cam.projection == Camera3D.PROJECTION_ORTHOGONAL:
		return
	var spd := 22.0 if Input.is_key_pressed(KEY_SHIFT) else 9.0
	var dir := Vector3.ZERO
	var b := _cam.global_transform.basis
	if Input.is_key_pressed(KEY_W): dir -= b.z
	if Input.is_key_pressed(KEY_S): dir += b.z
	if Input.is_key_pressed(KEY_A): dir -= b.x
	if Input.is_key_pressed(KEY_D): dir += b.x
	if Input.is_key_pressed(KEY_E): dir += Vector3.UP
	if Input.is_key_pressed(KEY_Q): dir -= Vector3.UP
	if dir != Vector3.ZERO:
		_cam.global_position += dir.normalized() * spd * delta

func _unhandled_input(e: InputEvent) -> void:
	if _cap or _cam == null:
		return
	if e is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED and _cam.projection == Camera3D.PROJECTION_PERSPECTIVE:
		_cam.rotate_y(-e.relative.x * 0.0028)
		_cam.rotate_object_local(Vector3.RIGHT, -e.relative.y * 0.0028)
	elif e is InputEventKey and e.pressed:
		if e.keycode == KEY_ESCAPE:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		elif e.keycode == KEY_1:
			_apply_view("tactical")
		elif e.keycode == KEY_2:
			_apply_view("overview")
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
		elif e.keycode == KEY_3:
			_apply_view("topdown")
	elif e is InputEventMouseButton and e.pressed:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

# =============================================================================
# 14. Cover raycast tests (automated, run once on the first physics frame)
# =============================================================================
# Real PhysicsDirectSpaceState3D rays against the collision built above — the
# same call a hitscan weapon would make. Five checks around the low barrier at
# (0, 10) and the tall barrier at (12, 9):
#   1. standing eye -> standing eye over 1.28 m cover  : NOT blocked (exposed)
#   2. standing eye -> crouching eye behind 1.28 m     : blocked (concealed)
#   3. standing eye -> standing eye behind 2.10 m      : blocked
#   4. standing eye -> top of head (1.75) behind 2.10 m: blocked
#   5. bullet at 0.6 m into the low barrier            : blocked (solid)
# Output: COVER_TEST [PASS|FAIL] lines on stdout; qa/capture_views.sh counts them.
func _ray_blocked(from: Vector3, to: Vector3) -> bool:
	var space := get_world_3d().direct_space_state
	var q := PhysicsRayQueryParameters3D.create(from, to)
	q.collide_with_bodies = true
	var hit := space.intersect_ray(q)
	if hit.is_empty():
		return false
	return hit["position"].distance_to(from) < from.distance_to(to) - 0.15

func _check(label: String, blocked: bool, want: bool) -> void:
	print("COVER_TEST [%s] %s (blocked=%s expected=%s)" % [("PASS" if blocked == want else "FAIL"), label, blocked, want])

func _run_cover_tests() -> void:
	print("===== COVER TESTS (1.80 m player; eye=%.2f crouch_eye=%.2f) =====" % [STAND_EYE, CROUCH_EYE])
	var atk_low := Vector3(0, STAND_EYE, 12.6)
	_check("stand behind 1.28 low cover -> exposed", _ray_blocked(atk_low, Vector3(0, STAND_EYE, 9.0)), false)
	_check("crouch behind 1.28 low cover -> concealed", _ray_blocked(atk_low, Vector3(0, CROUCH_EYE, 9.0)), true)
	var atk_tall := Vector3(9.5, STAND_EYE, 9.0)
	_check("stand behind 2.10 tall cover -> concealed", _ray_blocked(atk_tall, Vector3(13.6, STAND_EYE, 9.0)), true)
	_check("stand head behind 2.10 tall cover -> concealed", _ray_blocked(atk_tall, Vector3(13.6, PLAYER_HEIGHT - 0.05, 9.0)), true)
	_check("bullet into low cover cannot pass through", _ray_blocked(Vector3(0, 0.6, 12.6), Vector3(0, 0.6, 9.0)), true)
	print("===== END COVER TESTS =====")
