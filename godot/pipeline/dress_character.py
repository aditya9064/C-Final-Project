"""Dress a Quaternius base character: build a clothing shell from the body (keeps rig
weights), apply grey/charcoal tactical scheme + team accent, add hair, bundle animations.
Args after -- : base_gltf ual_glb hair_gltf out_glb  accentR accentG accentB
"""
import bpy, bmesh, sys
a = sys.argv[sys.argv.index("--") + 1:]
BASE, UAL, HAIR, OUT = a[0], a[1], a[2], a[3]
ACC = (float(a[4]), float(a[5]), float(a[6]), 1.0)

def mat(name, rgb, rough=0.75, metal=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m

# clean
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
for c in (bpy.data.actions, bpy.data.meshes, bpy.data.armatures, bpy.data.materials):
    for b in list(c):
        if b.users == 0: c.remove(b)

# import base
bpy.ops.import_scene.gltf(filepath=BASE)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
body = max(meshes, key=lambda o: len(o.data.vertices))

# --- build clothing shell from the body ---
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.duplicate()
clothes = bpy.context.active_object
clothes.name = "Clothes"
clothes.data.materials.clear()
HOODIE = mat("cloth_hoodie", (0.085, 0.09, 0.10))   # dark charcoal-grey hoodie
PANTS  = mat("cloth_pants",  (0.045, 0.048, 0.055))  # near-black cargo pants
BOOTS  = mat("cloth_boots",  (0.02, 0.02, 0.025))
ACCENT = mat("cloth_accent", ACC[:3], 0.55)
LIGHT  = mat("cloth_panel",  (0.42, 0.44, 0.47))     # light-grey chest panel (as in concept)
for m in (HOODIE, PANTS, BOOTS, ACCENT, LIGHT):
    clothes.data.materials.append(m)
IDX = {"hoodie": 0, "pants": 1, "boots": 2, "accent": 3, "panel": 4}

# dominant bone group per vertex
grp_name = {vg.index: vg.name for vg in clothes.vertex_groups}
def dom(v):
    best, bw = None, -1
    for g in v.groups:
        if g.weight > bw: bw, best = g.weight, g.group
    return grp_name.get(best, "")

vdom = [dom(v) for v in clothes.data.vertices]

def region(name):
    if name.startswith(("foot", "ball")): return "boots"
    if name.startswith(("pelvis", "thigh", "calf")): return "pants"
    if name.startswith(("clavicle", "lowerarm")): return "accent"
    if name.startswith(("spine_02", "spine_03")): return "panel"   # chest panel
    return "hoodie"

bm = bmesh.new(); bm.from_mesh(clothes.data)
bm.verts.ensure_lookup_table()
# delete faces dominated by Head (leave head/face bare skin)
del_faces = [f for f in bm.faces if all(vdom[v.index] == "Head" for v in f.verts)]
bmesh.ops.delete(bm, geom=del_faces, context='FACES')
# push shell outward along normals so it sits over the skin
for v in bm.verts:
    v.co += v.normal * 0.012
# assign material per face by majority region
for f in bm.faces:
    votes = {}
    for v in f.verts:
        r = region(vdom[v.index]); votes[r] = votes.get(r, 0) + 1
    r = max(votes, key=votes.get)
    f.material_index = IDX[r]
bm.to_mesh(clothes.data); bm.free()
clothes.data.update()

# --- hair ---
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=HAIR)
hair_new = [o for o in bpy.data.objects if o not in before]
hair_meshes = [o for o in hair_new if o.type == 'MESH']
for o in hair_new:
    if o.type == 'ARMATURE':
        bpy.data.objects.remove(o, do_unlink=True)
HAIRMAT = mat("hair_brown", (0.12, 0.07, 0.035), 0.7)
for hm in hair_meshes:
    hm.data.materials.clear(); hm.data.materials.append(HAIRMAT)
    vg = hm.vertex_groups.new(name="Head")
    vg.add(range(len(hm.data.vertices)), 1.0, 'REPLACE')
    md = hm.modifiers.new("arm", 'ARMATURE'); md.object = arm
    hm.parent = arm

# --- animations (transfer from UAL by matching bone names) ---
MAP = {'Idle_Loop': 'idle', 'Walk_Loop': 'walk', 'Sprint_Loop': 'run', 'Jog_Fwd_Loop': 'jog',
       'Jump_Start': 'jump_start', 'Jump_Loop': 'jump_loop', 'Jump_Land': 'jump_land',
       'Death01': 'death', 'Pistol_Aim_Neutral': 'aim', 'Pistol_Shoot': 'fire',
       'Pistol_Reload': 'reload', 'Pistol_Idle_Loop': 'aim_idle'}
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=UAL)
ual_objs = [o for o in bpy.data.objects if o not in before]
kept = {}
for key, clean in MAP.items():
    m = next((ac for ac in bpy.data.actions if ac.name.startswith(key)), None)
    if m: m.name = clean; m.use_fake_user = True; kept[clean] = m
for ac in list(bpy.data.actions):
    if ac not in kept.values(): bpy.data.actions.remove(ac)
for o in ual_objs: bpy.data.objects.remove(o, do_unlink=True)
arm.animation_data_create(); arm.animation_data.action = None
for name, act in kept.items():
    tr = arm.animation_data.nla_tracks.new(); tr.name = name
    tr.strips.new(name, int(act.frame_range[0]), act)

# --- export everything ---
bpy.ops.object.select_all(action='DESELECT')
for o in bpy.data.objects:
    o.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.gltf(filepath=OUT, export_format='GLB', use_selection=True,
    export_apply=False, export_animations=True, export_animation_mode='NLA_TRACKS', export_yup=True)
print("DRESSED:", OUT, "clips:", sorted(kept.keys()))
