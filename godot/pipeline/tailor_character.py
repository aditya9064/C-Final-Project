"""Phase 1 — model hoodie + cargo pants as body-derived, rig-weighted garments.
Args after -- : base_gltf ual_glb hair_gltf out_glb  accentR accentG accentB  sex(m/f)
Technique (per review): weld UV-seam-split verts BEFORE offsetting; garments are
duplicated from the already-skinned body so they inherit correct weights; delete body
faces under opaque garments; smooth to kill muscle detail; modest outward offset +
Solidify for fabric thickness.
"""
import bpy, bmesh, sys
from mathutils import Vector
a = sys.argv[sys.argv.index("--") + 1:]
BASE, UAL, HAIR, OUT = a[0], a[1], a[2], a[3]
ACC = (float(a[4]), float(a[5]), float(a[6]))
SEX = a[7] if len(a) > 7 else "m"

def mat(name, rgb, rough=0.78, metal=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m

bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
for c in (bpy.data.actions, bpy.data.meshes, bpy.data.armatures, bpy.data.materials):
    for b in list(c):
        if b.users == 0: c.remove(b)

bpy.ops.import_scene.gltf(filepath=BASE)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
body = max(meshes, key=lambda o: len(o.data.vertices))
for mm in meshes:  # brown eyebrows
    if "brow" in mm.name.lower() or any("brow" in (s.material.name.lower() if s.material else "") for s in mm.material_slots):
        mm.data.materials.clear(); mm.data.materials.append(mat("brow_brown", (0.10, 0.06, 0.03), 0.8))

grp = {vg.index: vg.name for vg in body.vertex_groups}
def dom_name(v):
    best, bw = -1, -1.0
    for g in v.groups:
        if g.weight > bw: bw, best = g.weight, g.group
    return grp.get(best, "")

HOODIE = mat("m_hoodie", (0.052, 0.057, 0.066))     # charcoal
PANTS  = mat("m_pants",  (0.078, 0.080, 0.086))      # dark slate (reads as a separate layer)
GLOVE  = mat("m_glove",  (0.03, 0.03, 0.035), 0.6)   # dark tactical gloves
SKIN_KEEP = {"head", "hand_l", "hand_r", "foot_l", "foot_r", "ball_l", "ball_r",
             "thumb", "index", "middle", "ring", "pinky", "neck_01", "lowerarm_l",
             "lowerarm_r", "calf_l", "calf_r"}

def dup(name):
    bpy.ops.object.select_all(action='DESELECT')
    body.select_set(True); bpy.context.view_layer.objects.active = body
    bpy.ops.object.duplicate()
    o = bpy.context.active_object; o.name = name
    return o

def weld_and_doms(o):
    bm = bmesh.new(); bm.from_mesh(o.data)
    before = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0002)
    bm.verts.ensure_lookup_table()
    bm.to_mesh(o.data); bm.free()
    o.data.update()
    print(f"  {o.name}: welded {before}->{len(o.data.vertices)} verts")
    return [dom_name(v) for v in o.data.vertices]

def delete_faces(o, keep_pred):
    """keep_pred(vdom_list, face) -> True to keep."""
    vdom = [dom_name(v) for v in o.data.vertices]
    bm = bmesh.new(); bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    dele = [f for f in bm.faces if not keep_pred(vdom, f)]
    bmesh.ops.delete(bm, geom=dele, context='FACES')
    bm.to_mesh(o.data); bm.free(); o.data.update()

def inflate_offset(o, offset, infl_xz=1.0, infl_center=(0.0, 0.0)):
    """Push verts outward along normal; optionally scale cross-section about a vertical axis."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    bm.normal_update()
    cx, cz = infl_center
    for v in bm.verts:
        if infl_xz != 1.0:
            v.co.x = cx + (v.co.x - cx) * infl_xz
            v.co.z = cz + (v.co.z - cz) * infl_xz
        v.co += v.normal * offset
    bm.to_mesh(o.data); bm.free(); o.data.update()

def smooth(o, iters=2, fac=0.5):
    bm = bmesh.new(); bm.from_mesh(o.data)
    for _ in range(iters):
        bmesh.ops.smooth_vert(bm, verts=bm.verts, factor=fac,
                              use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(o.data); bm.free(); o.data.update()

def solidify(o, th=0.012):
    m = o.modifiers.new("solid", 'SOLIDIFY'); m.thickness = th; m.offset = 0.0

def flare_hem(o, z_hem, band=0.10, out=1.16, drop=0.03):
    """Flare the bottom band of a garment outward + down for a baggy hem (Z-up)."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    n = max(1, len(bm.verts))
    cx = sum(v.co.x for v in bm.verts) / n
    cy = sum(v.co.y for v in bm.verts) / n
    for v in bm.verts:
        if v.co.z < z_hem + band:
            t = max(0.0, min(1.0, 1.0 - (v.co.z - z_hem) / band))
            v.co.x = cx + (v.co.x - cx) * (1 + (out - 1) * t)
            v.co.y = cy + (v.co.y - cy) * (1 + (out - 1) * t)
            v.co.z -= drop * t
    bm.to_mesh(o.data); bm.free(); o.data.update()

def recolor_body():
    """Paint the body under the garments with matching colours so any gap/open hem
    reads as cloth, not a hole. Keeps skin on head/hands/feet/neck."""
    vdom = [dom_name(v) for v in body.data.vertices]
    skin = body.data.materials[0] if len(body.data.materials) else mat("m_skin", (0.75, 0.57, 0.47))
    body.data.materials.clear()
    for m in (skin, HOODIE, PANTS, GLOVE): body.data.materials.append(m)
    bm = bmesh.new(); bm.from_mesh(body.data)
    SKN = ("head", "foot", "ball", "neck")
    HND = ("hand", "thumb", "index", "middle", "ring", "pinky")
    for f in bm.faces:
        doms = [vdom[v.index] for v in f.verts]
        leg = sum(1 for d in doms if d.startswith(("thigh", "calf")))
        tor = sum(1 for d in doms if d.startswith(("spine", "clavicle", "upperarm", "lowerarm", "pelvis")))
        skn = sum(1 for d in doms if d.startswith(SKN))
        hnd = sum(1 for d in doms if d.startswith(HND))
        if skn >= 2: f.material_index = 0
        elif hnd >= 2: f.material_index = 3            # gloves
        elif leg >= tor and leg > 0: f.material_index = 2
        elif tor > 0: f.material_index = 1
        else: f.material_index = 0
    bm.to_mesh(body.data); bm.free(); body.data.update()

# ---------------- HOODIE (torso + arms to wrist, hem at upper thigh) ----------------
# Blender axes here: Y up (glTF imported as Y-up->Blender Z-up? import_scene.gltf gives Z-up).
# After import vertices use Blender Z-up; height is Z. Verify via body bbox.
zs = [v.co.z for v in body.data.vertices]
HEIGHT = max(zs) - min(zs); FLOOR = min(zs)
print("body height(Z):", round(HEIGHT, 3), "floor:", round(FLOOR, 3))
hem = FLOOR + 0.46 * HEIGHT       # hoodie hem ~mid-hip/upper-thigh

hoodie = dup("Hoodie")
weld_and_doms(hoodie)
HOOD_KEEP = ("spine", "neck_01", "clavicle", "upperarm", "lowerarm", "pelvis")
def keep_hoodie(vdom, f):
    if any(v.co.z < hem for v in f.verts): return False       # trim hem clean
    return any(vdom[v.index].startswith(HOOD_KEEP) and not vdom[v.index].startswith("head")
               for v in f.verts)
delete_faces(hoodie, keep_hoodie)
smooth(hoodie, 2, 0.5)
inflate_offset(hoodie, 0.020 if SEX == "m" else 0.013, infl_xz=1.05 if SEX == "m" else 1.025)
flare_hem(hoodie, hem, band=0.12, out=1.18 if SEX == "m" else 1.14, drop=0.035)
solidify(hoodie, 0.015)
hoodie.data.materials.clear(); hoodie.data.materials.append(HOODIE)

# ---------------- CARGO PANTS (waist to ankle, baggy) ----------------
waist = FLOOR + 0.52 * HEIGHT
pants = dup("Pants")
weld_and_doms(pants)
PANT_KEEP = ("pelvis", "thigh", "calf")
def keep_pants(vdom, f):
    if any(v.co.z > waist for v in f.verts): return False
    return any(vdom[v.index].startswith(PANT_KEEP) for v in f.verts)
delete_faces(pants, keep_pants)
smooth(pants, 1, 0.5)
inflate_offset(pants, 0.028 if SEX == "m" else 0.020, infl_xz=1.09 if SEX == "m" else 1.055)
solidify(pants, 0.016)
pants.data.materials.clear(); pants.data.materials.append(PANTS)

# ---------------- recolor body under garments (any gap reads as cloth, not a hole) ----------------
recolor_body()

# ================= PHASE 2: tactical gear =================
GEAR   = mat("m_gear",   (0.018, 0.018, 0.022), 0.6)    # black webbing/pouches
BOOT   = mat("m_boot",   (0.022, 0.022, 0.027), 0.55)
ACCM   = mat("m_accent", ACC, 0.5)
PANEL  = mat("m_panel",  (0.40, 0.42, 0.45), 0.8)       # light-grey chest panel
GLOVE  = mat("m_glove",  (0.03, 0.03, 0.035), 0.6)

def bone_pos(name):
    b = arm.data.bones.get(name)
    if not b: return None, None
    return arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local

# front-facing direction (toe bone points forward)
_fh, _ft = bone_pos("ball_l")
FY = 1.0 if (_ft and _fh and (_ft.y - _fh.y) > 0) else -1.0   # +1 if front=+Y else -1

def bind_single(o, bone):
    vg = o.vertex_groups.new(name=bone); vg.add(range(len(o.data.vertices)), 1.0, 'REPLACE')
    m = o.modifiers.new("arm", 'ARMATURE'); m.object = arm; o.parent = arm

def bind_transfer(o):
    md = o.modifiers.new("dt", 'DATA_TRANSFER'); md.object = body
    md.use_vert_data = True; md.data_types_verts = {'VGROUP_WEIGHTS'}
    md.vert_mapping = 'POLYINTERP_NEAREST'
    md.layers_vgroup_select_src = 'ALL'; md.layers_vgroup_select_dst = 'NAME'
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.datalayout_transfer(modifier="dt")
    bpy.ops.object.modifier_apply(modifier="dt")
    am = o.modifiers.new("arm", 'ARMATURE'); am.object = arm; o.parent = arm

def add_cube(name, loc, size, mt, rot=None):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    o = bpy.context.active_object; o.name = name; o.scale = size
    if rot: o.rotation_euler = rot
    bpy.ops.object.transform_apply(scale=True, rotation=bool(rot))
    o.data.materials.clear(); o.data.materials.append(mt)
    for p in o.data.polygons: p.use_smooth = False
    return o

def add_torus(name, loc, major, minor, mt, rot=None):
    bpy.ops.mesh.primitive_torus_add(location=loc, major_radius=major, minor_radius=minor,
                                     major_segments=28, minor_segments=12)
    o = bpy.context.active_object; o.name = name
    if rot: o.rotation_euler = rot; bpy.ops.object.transform_apply(rotation=True)
    o.data.materials.clear(); o.data.materials.append(mt); return o

def add_sphere(name, loc, r, mt):
    bpy.ops.mesh.primitive_uv_sphere_add(location=loc, radius=r, segments=20, ring_count=12)
    o = bpy.context.active_object; o.name = name
    o.data.materials.clear(); o.data.materials.append(mt); return o

def build_kit(sex):
    # ---- boots (body-derived, dark, covers foot + ankle) ----
    ankle = FLOOR + 0.12 * HEIGHT
    boots = dup("Boots");
    bm = bmesh.new(); bm.from_mesh(boots.data); bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0002)
    bm.to_mesh(boots.data); bm.free()
    vb = [dom_name(v) for v in boots.data.vertices]
    bmk = bmesh.new(); bmk.from_mesh(boots.data); bmk.verts.ensure_lookup_table()
    dele = [f for f in bmk.faces if (any(v.co.z > ankle for v in f.verts) or
            not any(vb[v.index].startswith(("foot", "ball", "calf")) for v in f.verts))]
    bmesh.ops.delete(bmk, geom=dele, context='FACES'); bmk.to_mesh(boots.data); bmk.free(); boots.data.update()
    smooth(boots, 1, 0.4); inflate_offset(boots, 0.012, infl_xz=1.06); solidify(boots, 0.012)
    boots.data.materials.clear(); boots.data.materials.append(BOOT)
    for side in ("l", "r"):                   # team-colour cuff at each ankle
        fh, _ = bone_pos(f"foot_{side}")
        cuff = add_torus(f"boot_acc_{side}", Vector((fh.x, fh.y, fh.z + 0.03)), 0.058, 0.012, ACCM)
        bind_single(cuff, f"foot_{side}")

    # ---- belt + pouches ----
    hip, _ = bone_pos("pelvis")
    beltz = FLOOR + 0.52 * HEIGHT
    belt = add_torus("belt", Vector((hip.x, hip.y, beltz + 0.01)), 0.155, 0.03, GEAR)
    bind_single(belt, "pelvis")
    for i, dx in enumerate((-0.11, 0.11)):     # two front hip pouches
        p = add_cube(f"pouch_hip_{i}", Vector((hip.x + dx, hip.y + FY * 0.11, beltz - 0.02)),
                     Vector((0.08, 0.05, 0.10)), GEAR)
        bind_single(p, "pelvis")
    # ---- thigh pouch (right) ----
    th, _ = bone_pos("thigh_r")
    tp = add_cube("pouch_thigh", Vector((th.x + 0.02, th.y + FY * 0.095, th.z - 0.13)),
                  Vector((0.085, 0.045, 0.11)), GEAR)
    bind_single(tp, "thigh_r")

    # ---- knee pads (rigid, on calf): dark pad + thin accent stripe ----
    for side in ("l", "r"):
        kn, _ = bone_pos(f"calf_{side}")
        pc = Vector((kn.x, kn.y + FY * 0.080, kn.z + 0.02))
        pad = add_sphere(f"knee_{side}", pc, 0.072, GEAR)
        pad.scale = Vector((1.05, 0.55, 1.25)); bpy.ops.object.transform_apply(scale=True)
        bind_single(pad, f"calf_{side}")
        stripe = add_cube(f"knee_acc_{side}", pc + Vector((0, FY * 0.02, 0)),
                          Vector((0.13, 0.02, 0.028)), ACCM)
        bind_single(stripe, f"calf_{side}")

    # ---- chest harness: horizontal band + 2 diagonal straps + light panel ----
    ch, _ = bone_pos("spine_03")
    band = add_torus("chest_band", Vector((ch.x, ch.y, ch.z + 0.02)), 0.175, 0.022, GEAR)
    bind_transfer(band)
    for side, s in (("l", -1), ("r", 1)):     # straps over each shoulder (front<->back)
        cl, _ = bone_pos(f"clavicle_{side}")
        strap = add_cube(f"strap_{side}", Vector((ch.x + s * 0.085, ch.y, ch.z + 0.115)),
                         Vector((0.05, 0.30, 0.04)), GEAR)
        bind_transfer(strap)
    panel = add_cube("chest_panel", Vector((ch.x, ch.y + FY * 0.135, ch.z + 0.05)),
                     Vector((0.18, 0.03, 0.15)), PANEL)
    bind_transfer(panel)
    # shoulder accent patches (wrap the shoulder cap)
    for side in ("l", "r"):
        sh, _ = bone_pos(f"upperarm_{side}")
        patch = add_cube(f"shoulder_{side}", Vector((sh.x, sh.y, sh.z + 0.01)),
                         Vector((0.135, 0.145, 0.10)), ACCM)
        bind_single(patch, f"upperarm_{side}")

    # ---- hood (cowl draped on upper back) ----
    nk, _ = bone_pos("neck_01")
    hood = add_sphere("hood", Vector((nk.x, nk.y + FY * -0.07, nk.z + 0.02)), 0.13, HOODIE)
    hood.scale = Vector((1.05, 0.9, 1.25)); bpy.ops.object.transform_apply(scale=True)
    # cut the front half off (keep back drape) — verts are LOCAL, centred on origin
    bmh = bmesh.new(); bmh.from_mesh(hood.data)
    kill = [v for v in bmh.verts if v.co.y * FY > -0.01]   # remove front-facing verts
    bmesh.ops.delete(bmh, geom=kill, context='VERTS'); bmh.to_mesh(hood.data); bmh.free(); hood.data.update()
    bind_transfer(hood)

    if sex == "f":
        # measure the real head geometry (bone tail is unreliable)
        hv = [v.co for v in body.data.vertices if dom_name(v).startswith("Head")]
        hx = sum(v.x for v in hv) / len(hv)
        hzmax = max(v.z for v in hv); hzmin = min(v.z for v in hv)
        hymin = min(v.y for v in hv); hymax = max(v.y for v in hv)
        cy = (hymin + hymax) / 2
        front_y = hymax if FY > 0 else hymin
        back_y = hymin if FY > 0 else hymax
        forehead_z = hzmin + 0.74 * (hzmax - hzmin)
        # ---- ponytail: wide at crown, tapering tip hanging down + slightly back ----
        base = Vector((hx, back_y - FY * 0.02, hzmax - 0.10))
        bpy.ops.mesh.primitive_cone_add(radius1=0.055, radius2=0.02, depth=0.30, location=base)
        pony = bpy.context.active_object; pony.name = "ponytail"
        pony.rotation_euler = (FY * 2.9, 0, 0)        # +Z(thin tip) rotates to point down-back
        bpy.ops.object.transform_apply(rotation=True)
        pony.data.materials.clear(); pony.data.materials.append(HAIRMAT_LATE())
        bind_single(pony, "Head")
        # ---- goggle strap across the forehead + lenses resting on the brow ----
        strap = add_torus("goggles", Vector((hx, cy, forehead_z)), 0.094, 0.017, GEAR)
        bind_single(strap, "Head")
        for dx in (-0.038, 0.038):
            lens = add_sphere(f"lens_{dx}", Vector((hx + dx, front_y + FY * 0.008, forehead_z)), 0.03, ACCM)
            lens.scale = Vector((1.0, 0.42, 1.0)); bpy.ops.object.transform_apply(scale=True)
            bind_single(lens, "Head")

def HAIRMAT_LATE():
    return bpy.data.materials.get("m_hair") or mat("m_hair", (0.11, 0.065, 0.032), 0.72)

build_kit(SEX)

# ---------------- hair ----------------
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=HAIR)
hair_new = [o for o in bpy.data.objects if o not in before]
for o in hair_new:
    if o.type == 'ARMATURE': bpy.data.objects.remove(o, do_unlink=True)
HAIRMAT = mat("m_hair", (0.11, 0.065, 0.032), 0.72)
for hm in [o for o in hair_new if o.type == 'MESH']:
    hm.data.materials.clear(); hm.data.materials.append(HAIRMAT)
    vg = hm.vertex_groups.new(name="Head"); vg.add(range(len(hm.data.vertices)), 1.0, 'REPLACE')
    md = hm.modifiers.new("arm", 'ARMATURE'); md.object = arm; hm.parent = arm

# ---------------- animations ----------------
MAP = {'Idle_Loop': 'idle_loop', 'Walk_Loop': 'walk_loop', 'Sprint_Loop': 'run_loop',
       'Jog_Fwd_Loop': 'jog_loop', 'Jump_Start': 'jump_start', 'Jump_Loop': 'jump_loop',
       'Jump_Land': 'jump_land', 'Death01': 'death', 'Pistol_Aim_Neutral': 'aim',
       'Pistol_Shoot': 'fire', 'Pistol_Reload': 'reload', 'Pistol_Idle_Loop': 'aim_idle_loop'}
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

# ---------------- export ----------------
bpy.ops.object.select_all(action='DESELECT')
for o in bpy.data.objects: o.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.gltf(filepath=OUT, export_format='GLB', use_selection=True,
    export_apply=True, export_animations=True, export_animation_mode='NLA_TRACKS', export_yup=True)
print("TAILORED:", OUT, "clips:", sorted(kept.keys()))
