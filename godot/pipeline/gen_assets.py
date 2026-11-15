"""
Fireteam: Skirmish — procedural 3D asset generator (Blender 4.2 headless).

Run (GLB export needs factory startup so the glTF add-on loads cleanly):
    blender --background --factory-startup --python gen_assets.py -- <out_models_dir>

Exports coherent, single-piece GLB meshes with real materials to assets/models/,
and saves editable .blend sources to assets/blend/. Godot imports the GLBs and
adds box/cylinder collision matching the gameplay cover dimensions.

Blender is Z-up; glTF export converts to Y-up for Godot. Each prop's origin sits
at the ground (z=0) so Godot can place it directly on the floor.
"""
import bpy, bmesh, math, os, sys, random
from mathutils import Vector, Euler

def resolve_out():
    if "--" in sys.argv:
        a = sys.argv[sys.argv.index("--") + 1:]
        if a:
            return os.path.abspath(a[0])
    sd = os.path.dirname(os.path.abspath(sys.argv[sys.argv.index("--python") + 1])) \
        if "--python" in sys.argv else os.getcwd()
    return os.path.abspath(os.path.join(sd, "..", "assets", "models"))

OUT = resolve_out()
SRC = os.path.abspath(os.path.join(OUT, "..", "blend"))
os.makedirs(OUT, exist_ok=True)
os.makedirs(SRC, exist_ok=True)
print("ASSET_OUT_DIR:", OUT)

# ---------------------------------------------------------------- helpers
def clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for b in list(bpy.data.meshes):
        if b.users == 0:
            bpy.data.meshes.remove(b)
    build_mats()   # materials are purged with their objects; recreate valid refs

def set_active(o):
    bpy.context.view_layer.objects.active = o
    o.select_set(True)

def apply_mods(o):
    set_active(o)
    for m in list(o.modifiers):
        try:
            bpy.ops.object.modifier_apply(modifier=m.name)
        except Exception as e:
            print("apply fail", m.name, e)

def shade_smooth(o):
    set_active(o); bpy.ops.object.shade_smooth()

def mat(name, color, rough=0.85, metal=0.0):
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (color[0], color[1], color[2], 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m

def assign(o, m):
    o.data.materials.clear()
    o.data.materials.append(m)

def clouds(name, scale=0.3):
    t = bpy.data.textures.new(name, type='CLOUDS'); t.noise_scale = scale
    return t

def cube(size, loc, name="c"):
    # primitive_cube_add(size=1) spans -0.5..0.5 (edge 1), so scale by the full
    # edge length to get `size` (scaling by size/2 would make it half as big).
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.active_object; o.name = name
    o.scale = (size[0], size[1], size[2])
    bpy.ops.object.transform_apply(scale=True)
    return o

def bevel(o, w=0.02, seg=2):
    m = o.modifiers.new("bev", 'BEVEL'); m.width = w; m.segments = seg
    m.limit_method = 'ANGLE'

def join(objs, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    set_active(objs[0]); bpy.ops.object.join()
    objs[0].name = name
    return objs[0]

def export(name):
    bpy.ops.object.select_all(action='SELECT')
    try:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(SRC, name + ".blend"))
    except Exception as e:
        print("blend save warn:", e)
    path = os.path.join(OUT, name + ".glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB',
                              use_selection=True, export_apply=True, export_yup=True)
    print("EXPORTED:", path)

# ---------------------------------------------------------------- materials
M = {}
def build_mats():
    M['olive'] = mat("olive", (0.20, 0.24, 0.11), 0.7, 0.1)
    M['steel'] = mat("steel", (0.11, 0.12, 0.14), 0.5, 0.7)
    M['orange'] = mat("orange", (0.72, 0.19, 0.015), 0.82, 0.0)  # deep saturated canopy orange
    M['wood'] = mat("wood", (0.24, 0.15, 0.08), 0.95, 0.0)
    M['leaf1'] = mat("leaf1", (0.20, 0.38, 0.13), 0.9, 0.0)
    M['leaf2'] = mat("leaf2", (0.28, 0.46, 0.18), 0.9, 0.0)
    M['concrete'] = mat("concrete", (0.70, 0.66, 0.58), 0.9, 0.0)
    M['sand'] = mat("sand", (0.62, 0.53, 0.36), 0.95, 0.0)

# ---------------------------------------------------------------- CRATE (solid)
def crate():
    clear()
    s = 0.95
    body = cube((s, s, s), (0, 0, s / 2), "body")
    bevel(body, 0.03, 2); apply_mods(body); assign(body, M['olive'])
    parts = [body]
    # raised frame strips flush on the surface (not a cage) -> coherent
    fr = 0.06; half = s / 2
    for z in (0.06, s - 0.06):                      # horizontal belts
        for ax in range(2):
            size = [s + 0.02, fr, fr]
            if ax == 1: size = [fr, s + 0.02, fr]
            for side in (-1, 1):
                loc = [0, 0, z]
                loc[1 - ax] = side * half
                parts.append(cube(tuple(size), tuple(loc), "belt"))
    # corner reinforcements (solid, flush)
    for cx in (-half, half):
        for cy in (-half, half):
            parts.append(cube((0.14, 0.14, s + 0.02), (cx, cy, s / 2), "cnr"))
    # handles on two sides
    for sy in (-half - 0.015, half + 0.015):
        parts.append(cube((0.3, 0.06, 0.09), (0, sy, s * 0.6), "h"))
    steel_parts = join(parts[1:], "crate_steel")
    assign(steel_parts, M['steel'])
    join([body, steel_parts], "crate")
    export("crate")

# ---------------------------------------------------------------- EQUIPMENT CASE
def case():
    clear()
    w, d, h = 1.10, 0.55, 0.55
    lo = cube((w, d, h * 0.62), (0, 0, h * 0.31), "lo"); bevel(lo, 0.02); apply_mods(lo)
    assign(lo, M['olive'])
    lid = cube((w + 0.02, d + 0.02, h * 0.42), (0, 0, h * 0.62 + h * 0.2), "lid"); bevel(lid, 0.03); apply_mods(lid)
    assign(lid, M['olive'])
    parts = []
    for sx in (-0.33, 0.33):                       # latches
        parts.append(cube((0.12, d + 0.06, 0.1), (sx, 0, h * 0.58), "lat"))
    st = join(parts, "case_steel"); assign(st, M['steel'])
    join([lo, lid, st], "case")
    export("case")

# ---------------------------------------------------------------- BARREL
def barrel():
    clear()
    bpy.ops.mesh.primitive_cylinder_add(vertices=28, radius=0.29, depth=0.9, location=(0, 0, 0.45))
    o = bpy.context.active_object
    me = o.data; bm = bmesh.new(); bm.from_mesh(me)
    for v in bm.verts:
        for band in (0.18, 0.45, 0.72):
            if abs(v.co.z - band) < 0.03:
                v.co.x *= 1.05; v.co.y *= 1.05
    bm.to_mesh(me); bm.free()
    bevel(o, 0.012, 2); apply_mods(o); shade_smooth(o)
    parts = [o]
    for z in (0.02, 0.88):
        bpy.ops.mesh.primitive_torus_add(major_radius=0.29, minor_radius=0.028, location=(0, 0, z))
        parts.append(bpy.context.active_object)
    parts.append(cube((0.5, 0.5, 0.02), (0, 0, 0.9), "lid"))
    b = join(parts, "barrel"); assign(b, M['steel'])
    shade_smooth(b)
    export("barrel")

# ---------------------------------------------------------------- SANDBAG PILE
def bag(w, l, h, seed):
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = bpy.context.active_object
    o.scale = (w, l, h); bpy.ops.object.transform_apply(scale=True)
    b = o.modifiers.new("b", 'BEVEL'); b.width = min(w, l, h) * 0.45; b.segments = 3
    s = o.modifiers.new("s", 'SUBSURF'); s.levels = 1
    d = o.modifiers.new("d", 'DISPLACE'); d.texture = clouds("bag%d" % seed, 0.2); d.strength = 0.03
    apply_mods(o); shade_smooth(o)
    return o

def sandbag_pile():
    clear(); random.seed(5)
    bags = []
    bw, bl, bh = 0.65, 0.42, 0.26
    y = bh / 2
    for li, count in enumerate([4, 3, 3, 2, 1]):
        stag = bl * 0.4 if li % 2 else 0.0
        for b in range(count):
            x = -(count - 1) * bw * 0.5 + b * bw
            o = bag(bw * random.uniform(0.95, 1.05), bl, bh, li * 10 + b)
            o.location = (x, stag + random.uniform(-0.03, 0.03), y)
            o.rotation_euler = Euler((0, 0, random.uniform(-0.1, 0.1)))
            bags.append(o)
        y += bh * 0.82
    p = join(bags, "sandbag_pile"); assign(p, M['sand'])
    export("sandbag_pile")

# ---------------------------------------------------------------- CONCRETE BARRIERS
def concrete_block(size, name):
    o = cube(size, (0, 0, size[2] / 2), name)
    bevel(o, 0.04, 2); apply_mods(o)
    # subtle cast-concrete surface variation + chipped feel
    d = o.modifiers.new("d", 'DISPLACE'); d.texture = clouds(name + "_t", 0.5); d.strength = 0.015
    apply_mods(o)
    assign(o, M['concrete'])
    return o

def barrier_low():
    clear(); concrete_block((3.0, 0.65, 1.28), "barrier_low"); export("barrier_low")

def barrier_tall():
    clear(); concrete_block((2.8, 0.85, 2.10), "barrier_tall"); export("barrier_tall")

def barrier_L():
    clear()
    a = concrete_block((2.6, 0.7, 1.45), "a"); a.location = (0.95, 0, 0)
    bpy.ops.object.transform_apply(location=True)
    b = concrete_block((0.7, 2.0, 1.45), "b"); b.location = (-0.6, 0.95, 0)
    bpy.ops.object.transform_apply(location=True)
    o = join([a, b], "barrier_L"); assign(o, M['concrete'])
    export("barrier_L")

# ---------------------------------------------------------------- SPAWN ARCH
def spawn_arch():
    clear()
    w, h, t, depth = 5.5, 3.4, 0.5, 2.0
    parts = []
    for sz in (-w / 2, w / 2):                      # jambs
        parts.append(cube((t, h, depth), (0, sz, h / 2)))
    parts.append(cube((t, t, w + t), (0, 0, h - t / 2)))          # header
    parts.append(cube((depth, h, w + t), (-depth / 2 - t / 2, 0, h / 2)))  # back wall (recess)
    # side returns -> alcove depth
    for sz in (-w / 2, w / 2):
        parts.append(cube((depth, h, t), (-depth / 2, sz, h / 2)))
    o = join(parts, "spawn_arch"); assign(o, M['concrete'])
    bevel(o, 0.03, 1); apply_mods(o)
    export("spawn_arch")

# ---------------------------------------------------------------- CANOPY
def canopy():
    clear()
    half = 5.3; ridge = 1.25
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=28, y_subdivisions=28, size=2)
    o = bpy.context.active_object
    o.scale = (half, half, 1); bpy.ops.object.transform_apply(scale=True)
    me = o.data; bm = bmesh.new(); bm.from_mesh(me)
    for v in bm.verts:
        x, y = v.co.x, v.co.y
        ex, ey = abs(x) / half, abs(y) / half
        hump = (1 - max(ex, ey)) * ridge
        dip = -0.3 * math.sin(math.pi * ex) * math.sin(math.pi * ey)   # per-quadrant sag
        v.co.z = hump + dip
    bm.to_mesh(me); bm.free()
    sol = o.modifiers.new("s", 'SOLIDIFY'); sol.thickness = 0.06
    apply_mods(o); shade_smooth(o); assign(o, M['orange'])
    fabric = o
    steel = []
    # perimeter frame beams at eave
    for sx in (-1, 1):
        steel.append(cube((half * 2 + 0.3, 0.14, 0.18), (0, sx * half, 0.0)))
        steel.append(cube((0.14, half * 2 + 0.3, 0.18), (sx * half, 0, 0.0)))
    # seam ridges dividing the 4 panels (along x=0 and y=0)
    seam = cube((half * 2, 0.07, 0.07), (0, 0, ridge * 0.5)); steel.append(seam)
    seam2 = cube((0.07, half * 2, 0.07), (0, 0, ridge * 0.5)); steel.append(seam2)
    # corner bolt blocks
    for sx in (-half, half):
        for sy in (-half, half):
            steel.append(cube((0.22, 0.22, 0.22), (sx, sy, 0.0)))
    s = join(steel, "canopy_steel"); assign(s, M['steel'])
    join([fabric, s], "canopy")
    export("canopy")

# ---------------------------------------------------------------- TREES
def leafball(loc, r, seed, m):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=r, location=loc)
    o = bpy.context.active_object
    d = o.modifiers.new("d", 'DISPLACE'); d.texture = clouds("lf%d" % seed, 0.55); d.strength = r * 0.4
    apply_mods(o); shade_smooth(o); assign(o, m)
    return o

def tree(name, height, seed):
    clear(); random.seed(seed)
    tr = height * 0.045
    wood = []
    bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=tr, radius2=tr * 0.5, depth=height * 0.55, location=(0, 0, height * 0.275))
    wood.append(bpy.context.active_object)
    for i in range(5):
        ang = random.uniform(0, math.tau); bl = height * random.uniform(0.2, 0.32)
        bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=tr * 0.45, radius2=tr * 0.18, depth=bl)
        br = bpy.context.active_object
        br.location = (math.cos(ang) * bl * 0.25, math.sin(ang) * bl * 0.25, height * random.uniform(0.38, 0.6))
        br.rotation_euler = Euler((math.radians(random.uniform(38, 62)), 0, ang))
        wood.append(br)
    w = join(wood, name + "_wood"); assign(w, M['wood'])
    leaves = []
    cz = height * 0.66
    for i in range(9):
        off = Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-0.35, 0.65))) * height * 0.17
        m = M['leaf1'] if i % 2 else M['leaf2']
        leaves.append(leafball((off.x, off.y, cz + off.z), height * random.uniform(0.16, 0.26), seed * 10 + i, m))
    c = join(leaves, name + "_canopy")
    join([w, c], name)
    export(name)

# ---------------------------------------------------------------- ROCK
def rock():
    clear()
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1.0)
    o = bpy.context.active_object
    o.scale = (1.0, 0.85, 0.6); bpy.ops.object.transform_apply(scale=True)
    d = o.modifiers.new("d", 'DISPLACE'); d.texture = clouds("rk", 0.6); d.strength = 0.26
    apply_mods(o); shade_smooth(o); assign(o, M['concrete'])
    o.name = "rock"
    export("rock")

# ---------------------------------------------------------------- run
def main():
    build_mats()
    crate(); case(); barrel(); sandbag_pile()
    barrier_low(); barrier_tall(); barrier_L(); spawn_arch()
    canopy()
    rock()
    # NOTE: trees/shrubs/ivy are generated by gen_vegetation.py (branching skeleton
    # + alpha leaf cards). gen_assets must NOT write tree_a/tree_b or it clobbers
    # them with the old low-detail cone trees.
    print("ALL_ASSETS_DONE")

main()
