"""
Fireteam: Skirmish — vegetation generator (Blender 4.2 headless).

Builds real branching trees (not spheres): a recursive tapered-branch skeleton
plus alpha-cutout leaf cards textured with a procedurally drawn leaf image.
Also shrubs and hanging ivy strands. Each tree exports as GLB with two separate,
named objects — "<name>_wood" and "<name>_leaves" — so Godot can give the wood a
bark material and the leaves an alpha-scissor foliage material by node name.

Run:
    blender --background --factory-startup --python gen_vegetation.py -- <out_models_dir>

Writes GLBs to assets/models/ and the leaf texture to textures/foliage/leaf.png.
"""
import bpy, bmesh, math, os, sys, random
from mathutils import Vector

TAU = math.tau

def resolve_out():
    if "--" in sys.argv:
        a = sys.argv[sys.argv.index("--") + 1:]
        if a:
            return os.path.abspath(a[0])
    return os.path.abspath(os.path.join(os.getcwd(), "..", "assets", "models"))

OUT = resolve_out()
SRC = os.path.abspath(os.path.join(OUT, "..", "blend"))
TEX = os.path.abspath(os.path.join(OUT, "..", "..", "textures", "foliage"))
for d in (OUT, SRC, TEX):
    os.makedirs(d, exist_ok=True)
print("VEG_OUT_DIR:", OUT)

# ---------------------------------------------------------------- helpers
def clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for m in list(bpy.data.meshes):
        if m.users == 0:
            bpy.data.meshes.remove(m)

def mat(name, color, rough=0.9, metal=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (color[0], color[1], color[2], 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m

def export(objs, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    try:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(SRC, name + ".blend"))
    except Exception as e:
        print("blend warn", e)
    path = os.path.join(OUT, name + ".glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB',
                              use_selection=True, export_apply=True, export_yup=True)
    print("EXPORTED:", path)

# ---------------------------------------------------------------- leaf texture
def make_leaf_png():
    """Draw a small cluster of leaflets with alpha and save to textures/foliage."""
    W = H = 128
    img = bpy.data.images.new("leafcard", W, H, alpha=True)
    px = [0.0] * (W * H * 4)
    # three overlapping leaflets (a sprig) for a fuller card
    sprigs = [(-0.34, -0.1, 0.55, 0.34, 0.55),
              (0.30, -0.05, -0.5, 0.32, 0.6),
              (0.0, 0.18, 0.0, 0.40, 0.78)]
    for y in range(H):
        vy = (y + 0.5) / H * 2 - 1
        for x in range(W):
            vx = (x + 0.5) / W * 2 - 1
            best = 0.0
            for (cx, cy, rot, rw, rh) in sprigs:
                dx = vx - cx; dy = vy - cy
                c = math.cos(rot); s = math.sin(rot)
                lx = dx * c - dy * s; ly = dx * s + dy * c
                r = (lx * lx) / (rw * rw) + (ly * ly) / (rh * rh)
                if r <= 1.0:
                    rib = 1.0 if abs(lx) < 0.035 else 0.0
                    shade = 0.5 - 0.22 * abs(lx) + 0.12 * ly + 0.12 * rib
                    best = max(best, shade)
            i = (y * W + x) * 4
            if best > 0.0:
                px[i] = 0.10 + 0.05 * best
                px[i + 1] = 0.24 + 0.34 * best
                px[i + 2] = 0.07 + 0.05 * best
                px[i + 3] = 1.0
            else:
                px[i + 3] = 0.0
    img.pixels = px
    img.filepath_raw = os.path.join(TEX, "leaf.png")
    img.file_format = 'PNG'
    img.save()
    print("EXPORTED_TEX:", img.filepath_raw)
    return img

# ---------------------------------------------------------------- branch mesh
def basis(z):
    z = z.normalized()
    up = Vector((0, 0, 1))
    if abs(z.dot(up)) > 0.9:
        up = Vector((1, 0, 0))
    x = z.cross(up).normalized()
    y = z.cross(x).normalized()
    return x, y

def bend(d, ang, azi):
    d = d.normalized()
    x, y = basis(d)
    perp = x * math.cos(azi) + y * math.sin(azi)
    return (d * math.cos(ang) + perp * math.sin(ang)).normalized()

def add_taper(bm, p0, p1, r0, r1, seg=6):
    d = p1 - p0
    if d.length < 1e-4:
        return
    x, y = basis(d)
    r0ring = []; r1ring = []
    for i in range(seg):
        a = TAU * i / seg
        off = x * math.cos(a) + y * math.sin(a)
        r0ring.append(bm.verts.new(p0 + off * r0))
        r1ring.append(bm.verts.new(p1 + off * r1))
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((r0ring[i], r0ring[j], r1ring[j], r1ring[i]))

def grow(segs, leaves, start, direction, length, radius, depth, rng):
    end = start + direction.normalized() * length
    segs.append((start, end, radius, radius * 0.7))
    if depth <= 0 or radius < 0.035:
        # sparse so individual leaf cards read as leaves, not a solid shell
        n = rng.randint(2, 3)
        for _ in range(n):
            off = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))) * length * 0.6
            leaves.append((end + off, rng.uniform(0.6, 1.0)))
        return
    nch = rng.randint(2, 3)
    for _ in range(nch):
        ang = rng.uniform(0.4, 0.95)
        azi = rng.uniform(0, TAU)
        nd = bend(direction, ang, azi)
        nd.z += 0.25                                   # branches reach upward
        grow(segs, leaves, end, nd, length * rng.uniform(0.62, 0.78),
             radius * rng.uniform(0.58, 0.72), depth - 1, rng)
    if rng.random() < 0.7:                             # continuing leader
        nd = bend(direction, rng.uniform(0.1, 0.3), rng.uniform(0, TAU))
        grow(segs, leaves, end, nd, length * 0.82, radius * 0.72, depth - 1, rng)

def build_leaves(leaves, name):
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new()
    rng = random.Random(len(leaves))
    for (pos, size) in leaves:
        for k in range(2):                             # two crossed quads
            ang = rng.uniform(0, TAU) + k * math.pi / 2
            tilt = rng.uniform(-0.5, 0.5)
            x = Vector((math.cos(ang), math.sin(ang), 0))
            yv = Vector((-math.sin(ang) * math.sin(tilt), math.cos(ang) * math.sin(tilt), math.cos(tilt)))
            hw = size * 0.5
            corners = [pos - x * hw - yv * hw, pos + x * hw - yv * hw,
                       pos + x * hw + yv * hw, pos - x * hw + yv * hw]
            vs = [bm.verts.new(c) for c in corners]
            f = bm.faces.new(vs)
            uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
            for loop, uv in zip(f.loops, uvs):
                loop[uvl].uv = uv
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    return o

def tree(name, trunk_len, trunk_r, depth, seed, bark_col):
    clear()
    rng = random.Random(seed)
    segs = []; leaves = []
    grow(segs, leaves, Vector((0, 0, 0)), Vector((0, 0, 1)) + Vector((rng.uniform(-.1, .1), rng.uniform(-.1, .1), 0)),
         trunk_len, trunk_r, depth, rng)
    # wood mesh
    bm = bmesh.new()
    for (p0, p1, r0, r1) in segs:
        add_taper(bm, p0, p1, r0, r1, 6)
    wme = bpy.data.meshes.new(name + "_wood")
    bm.to_mesh(wme); bm.free()
    wood = bpy.data.objects.new(name + "_wood", wme)
    bpy.context.collection.objects.link(wood)
    wood.data.materials.append(mat("bark", bark_col, 0.95))
    bpy.context.view_layer.objects.active = wood; wood.select_set(True)
    bpy.ops.object.shade_smooth()
    # leaves mesh
    lv = build_leaves(leaves, name + "_leaves")
    lv.data.materials.append(mat("leafmat", (0.18, 0.4, 0.13), 0.9))
    export([wood, lv], name)
    print("TREE %s: %d branch segs, %d leaf cards" % (name, len(segs), len(leaves)))

# ---------------------------------------------------------------- shrub & ivy
def shrub(name, seed):
    clear()
    rng = random.Random(seed)
    leaves = []
    for _ in range(rng.randint(26, 36)):
        off = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(0.1, 1.4)))
        off.x *= 0.9; off.y *= 0.9
        leaves.append((off, rng.uniform(0.45, 0.75)))
    lv = build_leaves(leaves, name + "_leaves")
    lv.data.materials.append(mat("leafmat", (0.2, 0.42, 0.14), 0.9))
    # a couple of tiny stems
    bm = bmesh.new()
    for _ in range(3):
        a = rng.uniform(0, TAU)
        add_taper(bm, Vector((0, 0, 0)), Vector((math.cos(a) * 0.2, math.sin(a) * 0.2, 0.9)), 0.05, 0.02, 5)
    wme = bpy.data.meshes.new(name + "_wood")
    bm.to_mesh(wme); bm.free()
    wood = bpy.data.objects.new(name + "_wood", wme)
    bpy.context.collection.objects.link(wood)
    wood.data.materials.append(mat("bark", (0.22, 0.15, 0.09), 0.95))
    export([wood, lv], name)

def ivy(name, seed):
    clear()
    rng = random.Random(seed)
    # a hanging strand ~1.5 m with leaf cards along it
    leaves = []
    bm = bmesh.new()
    pts = []
    p = Vector((0, 0, 0))
    d = Vector((rng.uniform(-.15, .15), rng.uniform(-.1, .1), -1)).normalized()
    for i in range(7):
        np = p + d * 0.24
        pts.append((p, np))
        leaves.append((np + Vector((rng.uniform(-.12, .12), rng.uniform(-.12, .12), 0)), rng.uniform(0.3, 0.5)))
        if i % 2 == 0:
            leaves.append((np + Vector((rng.uniform(-.2, .2), rng.uniform(-.2, .2), 0.05)), rng.uniform(0.25, 0.4)))
        p = np
        d = (d + Vector((rng.uniform(-.1, .1), rng.uniform(-.1, .1), -0.15))).normalized()
    for (a, b) in pts:
        add_taper(bm, a, b, 0.02, 0.015, 4)
    wme = bpy.data.meshes.new(name + "_wood")
    bm.to_mesh(wme); bm.free()
    wood = bpy.data.objects.new(name + "_wood", wme)
    bpy.context.collection.objects.link(wood)
    wood.data.materials.append(mat("bark", (0.18, 0.14, 0.08), 0.95))
    lv = build_leaves(leaves, name + "_leaves")
    lv.data.materials.append(mat("ivymat", (0.17, 0.36, 0.12), 0.9))
    export([wood, lv], name)

# ---------------------------------------------------------------- run
def main():
    make_leaf_png()
    tree("tree_a", 3.2, 0.26, 4, 7, (0.30, 0.21, 0.12))    # medium broadleaf ~8 m
    tree("tree_b", 4.0, 0.38, 5, 23, (0.26, 0.18, 0.10))   # large shade ~11 m
    tree("tree_c", 2.1, 0.17, 3, 41, (0.33, 0.24, 0.14))   # small ornamental ~5 m
    shrub("shrub", 61)
    ivy("ivy", 83)
    print("VEGETATION_DONE")

main()
