"""Neutral ortho turnaround render: front/side/back on grey bg.
Usage: blender -b -P ortho_render.py -- <in.glb> <out_prefix>"""
import bpy, sys, math
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
IN, OUT = argv[0], argv[1]

# clean scene
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=IN)

# merged AABB
mn = Vector((1e9,)*3); mx = Vector((-1e9,)*3)
for o in bpy.data.objects:
    if o.type == 'MESH':
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            for i in range(3):
                mn[i] = min(mn[i], w[i]); mx[i] = max(mx[i], w[i])
ctr = (mn + mx) * 0.5
H = mx[2] - mn[2]

# neutral grey world
world = bpy.data.worlds.new("w"); bpy.context.scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.63, 0.64, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0

# key + fill sun
for ang, en in ((( 0.5, -0.6, 0.9), 3.0), ((-0.6, 0.5, 0.4), 1.3)):
    l = bpy.data.lights.new("s", 'SUN'); l.energy = en
    ob = bpy.data.objects.new("s", l); bpy.context.collection.objects.link(ob)
    d = Vector(ang).normalized()
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()

# ortho camera (fit full height vertically so arms never shrink the frame)
cam = bpy.data.cameras.new("c"); cam.type = 'ORTHO'
cam.sensor_fit = 'VERTICAL'; cam.ortho_scale = H * 1.15
co = bpy.data.objects.new("c", cam); bpy.context.collection.objects.link(co)
bpy.context.scene.camera = co

scn = bpy.context.scene
try: scn.render.engine = 'BLENDER_EEVEE_NEXT'
except Exception:
    try: scn.render.engine = 'BLENDER_EEVEE'
    except Exception: pass
scn.render.resolution_x = 360; scn.render.resolution_y = 1086
scn.render.film_transparent = False
scn.view_settings.view_transform = 'Standard'

R = H * 2.2
views = {
    "front": Vector(( 0, -R, 0)),   # -Y
    "side":  Vector(( R,  0, 0)),   # +X
    "back":  Vector(( 0,  R, 0)),   # +Y
}
for name, off in views.items():
    co.location = ctr + off
    d = (ctr - co.location).normalized()
    co.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    scn.render.filepath = f"{OUT}_{name}.png"
    bpy.ops.render.render(write_still=True)
    print("WROTE", scn.render.filepath)
