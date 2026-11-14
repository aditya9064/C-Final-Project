"""
Fireteam: Skirmish — exterior terrain generator (Blender 4.2 headless).

Builds a sculpted grass terrain that surrounds the arena: flat within an ellipse
around the walls, then gentle hills + distant ridges with noise. Grass/dirt/rock
variation is baked into vertex colors (no extra textures), so Godot renders it with
a single vertex-color material. Exports terrain.glb to assets/models/.

Run: blender --background --factory-startup --python gen_terrain.py -- <out_models_dir>
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector, noise as mnoise

def resolve_out():
    if "--" in sys.argv:
        a = sys.argv[sys.argv.index("--") + 1:]
        if a:
            return os.path.abspath(a[0])
    return os.path.abspath(os.path.join(os.getcwd(), "..", "assets", "models"))

OUT = resolve_out()
SRC = os.path.abspath(os.path.join(OUT, "..", "blend"))
os.makedirs(OUT, exist_ok=True); os.makedirs(SRC, exist_ok=True)

# flat ellipse around the arena (arena is 42x34, walls to ~±21 x / ±17 z)
AX, AY = 27.0, 23.0
BASE = -0.15                      # grass sits just below the 0.13 concrete slab top

# muted, natural palette (less saturated / warmer than neon lime) to match the
# references: olive-green grass, dry-grass patches, warm dirt, dusty rock.
GRASS = Vector((0.23, 0.29, 0.14))
GRASS2 = Vector((0.27, 0.30, 0.16))
DRYGRASS = Vector((0.38, 0.38, 0.21))
DIRT = Vector((0.34, 0.27, 0.18))
GRAVEL = Vector((0.44, 0.41, 0.35))
ROCKC = Vector((0.40, 0.39, 0.34))

def height_color(x, y):
    e = math.sqrt((x / AX) ** 2 + (y / AY) ** 2)
    n1 = mnoise.noise(Vector((x * 0.021, y * 0.021, 0.0)))     # -1..1 rolling
    n2 = mnoise.noise(Vector((x * 0.0075, y * 0.0075, 5.0)))   # broad ridges
    patch = mnoise.noise(Vector((x * 0.06, y * 0.06, 9.0)))
    if e < 1.0:
        z = BASE
        t = 0.0
    else:
        t = min((e - 1.0) / 2.2, 1.0)
        z = BASE + t * (2.0 + 9.5 * t) + (n1 * 1.3 + n2 * 3.4) * t
    # colour
    col = GRASS.lerp(GRASS2, (n1 + 1) * 0.5)
    dry = mnoise.noise(Vector((x * 0.035, y * 0.035, 17.0)))
    if dry > 0.15:                                 # dry / sun-bleached grass drifts
        col = col.lerp(DRYGRASS, min((dry - 0.15) / 0.55, 1.0) * 0.6)
    if 1.0 <= e < 1.5:                              # bare dirt ring hugging the walls
        col = col.lerp(DIRT, min((1.5 - e) / 0.5, 1.0) * 0.7)
    if patch > 0.35:                               # scattered dirt patches
        col = col.lerp(DIRT, (patch - 0.35) / 0.65 * 0.85)
    if z > 4.5:                                    # rocky high ground
        col = col.lerp(ROCKC, min((z - 4.5) / 5.0, 1.0))
    if e < 1.08 and e >= 1.0:                       # gravel apron: soft ramp, no stair-steps
        col = col.lerp(GRAVEL, 0.3 * (1.08 - e) / 0.08)
    return z, col

def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    SX, SY = 220.0, 195.0
    NX, NY = 176, 156
    bm = bmesh.new()
    cols = NX + 1; rows = NY + 1
    verts = []
    vcol = {}
    for j in range(rows):
        row = []
        for i in range(cols):
            x = (i / NX - 0.5) * SX
            y = (j / NY - 0.5) * SY
            z, c = height_color(x, y)
            v = bm.verts.new((x, y, z))
            row.append(v); vcol[v] = c
        verts.append(row)
    for j in range(NY):
        for i in range(NX):
            bm.faces.new((verts[j][i], verts[j][i + 1], verts[j + 1][i + 1], verts[j + 1][i]))
    bm.normal_update()
    clay = bm.loops.layers.color.new("Col")
    for f in bm.faces:
        for loop in f.loops:
            c = vcol[loop.vert]
            loop[clay] = (c.x, c.y, c.z, 1.0)
    me = bpy.data.meshes.new("terrain")
    bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new("terrain", me)
    bpy.context.collection.objects.link(o)
    m = bpy.data.materials.new("terrain")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 1.0
    vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "Col"
    nt.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
    o.data.materials.append(m)
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.shade_smooth()
    try:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(SRC, "terrain.blend"))
    except Exception as e:
        print("blend warn", e)
    path = os.path.join(OUT, "terrain.glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True,
                              export_apply=True, export_yup=True)
    print("EXPORTED:", path, "verts=", cols * rows)

build()
print("TERRAIN_DONE")
