"""Tailor Fireteam operators to the supplied turnaround reference.
Args after -- : base_gltf ual_glb hair_gltf out_glb  accentR accentG accentB  sex(m/f)

Core technique (per review): garments are duplicated from the skinned body (to inherit
rig weights), then RESHAPED to clothing profiles in bone space -- a straight boxy torso
and tube sleeves/legs -- so the muscle anatomy is erased.  A skin-offset shell *is*
anatomy, which read as a bodysuit; a bone-axis profile reads as loose clothing.
Colour blocking: light-grey hoodie torso + charcoal sleeves/hood + team-colour piping;
dark cargo pants w/ team stripe; H-harness w/ buckle; grey/white hi-top sneakers.
"""
import bpy, bmesh, sys, math
from mathutils import Vector
a = sys.argv[sys.argv.index("--") + 1:]
BASE, UAL, HAIR, OUT = a[0], a[1], a[2], a[3]
ACC = (float(a[4]), float(a[5]), float(a[6]))
SEX = a[7] if len(a) > 7 else "m"

# ---------------- materials ----------------
def mat(name, rgb, rough=0.78, metal=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m

HOOD_LT = mat("hood_light", (0.52, 0.53, 0.55), 0.85)    # light-grey hoodie torso
HOOD_DK = mat("hood_dark",  (0.085, 0.090, 0.10), 0.85)   # charcoal sleeves / hood
PANTS   = mat("pants",      (0.065, 0.068, 0.078), 0.88)   # dark charcoal cargo
GEAR    = mat("gear",       (0.016, 0.016, 0.020), 0.6)    # black webbing / harness / pouches
ACCM    = mat("accent",     ACC, 0.45)                     # teal / orange
SNEAK   = mat("sneak",      (0.30, 0.31, 0.33), 0.7)       # grey sneaker upper
SOLE    = mat("sole",       (0.80, 0.81, 0.82), 0.6)       # white sole
GLOVE   = mat("glove",      (0.028, 0.028, 0.033), 0.6)    # dark fingerless gloves
KNEE    = mat("knee",       (0.10, 0.105, 0.115), 0.7)     # plain dark knee pad

# ---------------- import base ----------------
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
for c in (bpy.data.actions, bpy.data.meshes, bpy.data.armatures):
    for b in list(c):
        if b.users == 0: c.remove(b)
bpy.ops.import_scene.gltf(filepath=BASE)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
body = max(meshes, key=lambda o: len(o.data.vertices))
for mm in meshes:   # brown eyebrows
    if "brow" in mm.name.lower() or any("brow" in (s.material.name.lower() if s.material else "") for s in mm.material_slots):
        mm.data.materials.clear(); mm.data.materials.append(mat("brow_brown", (0.10, 0.06, 0.03), 0.8))

grp = {vg.index: vg.name for vg in body.vertex_groups}
def dom_name(v):
    best, bw = -1, -1.0
    for g in v.groups:
        if g.weight > bw: bw, best = g.weight, g.group
    return grp.get(best, "")

zs = [v.co.z for v in body.data.vertices]
HEIGHT = max(zs) - min(zs); FLOOR = min(zs)
xs = [v.co.x for v in body.data.vertices]; ys = [v.co.y for v in body.data.vertices]
CX = (max(xs) + min(xs)) / 2
print("body height(Z):", round(HEIGHT, 3), "floor:", round(FLOOR, 3))

# ---------------- bone helpers ----------------
def bone_pos(name):
    b = arm.data.bones.get(name)
    if not b: return None, None
    return arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local

_fh, _ft = bone_pos("ball_l")
FY = 1.0 if (_ft and _fh and (_ft.y - _fh.y) > 0) else -1.0   # +1 if front faces +Y
# torso centre line (use spine for front/back centre)
_sp, _ = bone_pos("spine_02")
CY = _sp.y if _sp else (max(ys) + min(ys)) / 2

ARM = {f"{p}_{s}": bone_pos(f"{p}_{s}") for p in ("upperarm", "lowerarm") for s in "lr"}
LEG = {f"{p}_{s}": bone_pos(f"{p}_{s}") for p in ("thigh", "calf") for s in "lr"}

def dup(name):
    bpy.ops.object.select_all(action='DESELECT')
    body.select_set(True); bpy.context.view_layer.objects.active = body
    bpy.ops.object.duplicate()
    o = bpy.context.active_object; o.name = name
    return o

def weld(o):
    bm = bmesh.new(); bm.from_mesh(o.data); before = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0002)
    bm.to_mesh(o.data); bm.free(); o.data.update()
    print(f"  {o.name}: welded {before}->{len(o.data.vertices)} verts")

def delete_faces(o, keep_pred):
    vdom = [dom_name(v) for v in o.data.vertices]
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    dele = [f for f in bm.faces if not keep_pred(vdom, f)]
    bmesh.ops.delete(bm, geom=dele, context='FACES')
    bm.to_mesh(o.data); bm.free(); o.data.update()

def smooth(o, iters=3, fac=0.5):
    bm = bmesh.new(); bm.from_mesh(o.data)
    for _ in range(iters):
        bmesh.ops.smooth_vert(bm, verts=bm.verts, factor=fac,
                              use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(o.data); bm.free(); o.data.update()

def solidify(o, th=0.014):
    m = o.modifiers.new("solid", 'SOLIDIFY'); m.thickness = th; m.offset = 0.0

def cut_below(o, z):
    """Clean horizontal slice: keep geometry above z, remove below (clean hem edge)."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, z), plane_no=(0, 0, 1),
                           clear_inner=True, clear_outer=False)
    bm.to_mesh(o.data); bm.free(); o.data.update()

def add_folds(o, strength=0.005, scale=0.16):
    tex = bpy.data.textures.new(o.name + "_fold", 'CLOUDS')
    tex.noise_scale = scale; tex.noise_depth = 1
    m = o.modifiers.new("folds", 'DISPLACE'); m.texture = tex
    m.strength = strength; m.mid_level = 0.5

# ---------------- clothing-profile reshape ----------------
# Pull every garment vertex to a target radius around its bone axis, erasing muscle.
def reshape(o, torso_rx, torso_ry, torso_ztop, hem_z=None, waist=False):
    doms = [dom_name(v) for v in o.data.vertices]
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    for v in bm.verts:
        d = doms[v.index]
        if d.startswith("neck"):
            # collar: narrow cylinder about centre
            dx = v.co.x - CX; dy = v.co.y - CY; dd = math.hypot(dx, dy)
            if dd > 1e-5:
                r = 0.085
                v.co.x = CX + dx / dd * r; v.co.y = CY + dy / dd * r
        elif d.startswith(("spine", "clavicle", "pelvis")):
            # torso: elliptical cross-section with a gentle waist (reads as a jacket, not a barrel)
            dx = v.co.x - CX; dy = v.co.y - CY
            ang = math.atan2(dy, dx)
            rx, ry = torso_rx, torso_ry
            if waist and hem_z is not None:
                zn = (v.co.z - hem_z) / max(1e-5, (torso_ztop - hem_z))
                taper = 1.0 - 0.16 * math.exp(-((zn - 0.42) / 0.24) ** 2)   # pinch at the waist
                rx *= taper; ry *= taper
            er = (rx * ry) / math.sqrt((ry * math.cos(ang))**2 + (rx * math.sin(ang))**2 + 1e-9)
            dd = math.hypot(dx, dy)
            if dd > 1e-5:
                v.co.x = CX + dx / dd * er; v.co.y = CY + dy / dd * er
        else:
            seg = ARM.get(d) or LEG.get(d)
            if not seg or seg[0] is None: continue
            h, t = seg
            ax = (t - h); L = ax.length
            if L < 1e-5: continue
            axn = ax / L
            tt = max(0.0, min(1.0, (v.co - h).dot(axn) / L))
            axis_pt = h + axn * (tt * L)
            radial = v.co - axis_pt; dd = radial.length
            if d.startswith("upperarm"): R = 0.082
            elif d.startswith("lowerarm"): R = 0.072 - 0.010 * tt      # taper to wrist cuff
            elif d.startswith("thigh"): R = 0.125 - 0.010 * tt
            else: R = 0.110 - 0.045 * tt                               # calf -> ankle pinch
            if dd > 1e-5: v.co = axis_pt + radial / dd * R
    bm.to_mesh(o.data); bm.free(); o.data.update()

def color_by_region(o, region_map, default):
    """region_map: list of (predicate(dom)->bool, material). First match wins."""
    doms = [dom_name(v) for v in o.data.vertices]
    mats = []
    for _, m in region_map:
        if m.name not in [mm.name for mm in mats]: mats.append(m)
    if default.name not in [mm.name for mm in mats]: mats.append(default)
    o.data.materials.clear()
    for m in mats: o.data.materials.append(m)
    idx = {m.name: i for i, m in enumerate(mats)}
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    for f in bm.faces:
        fdoms = [doms[v.index] for v in f.verts]
        chosen = default
        for pred, m in region_map:
            if sum(1 for d in fdoms if pred(d)) >= 2:
                chosen = m; break
        f.material_index = idx[chosen.name]
    bm.to_mesh(o.data); bm.free(); o.data.update()

def flare_hem(o, z_hem, band=0.10, out=1.16, drop=0.03):
    bm = bmesh.new(); bm.from_mesh(o.data)
    n = max(1, len(bm.verts))
    cx = sum(v.co.x for v in bm.verts) / n; cy = sum(v.co.y for v in bm.verts) / n
    for v in bm.verts:
        if v.co.z < z_hem + band:
            t = max(0.0, min(1.0, 1.0 - (v.co.z - z_hem) / band))
            v.co.x = cx + (v.co.x - cx) * (1 + (out - 1) * t)
            v.co.y = cy + (v.co.y - cy) * (1 + (out - 1) * t)
            v.co.z -= drop * t
    bm.to_mesh(o.data); bm.free(); o.data.update()

# ================= HOODIE =================
hem = FLOOR + 0.44 * HEIGHT
shoulder_z = (bone_pos("clavicle_l")[0].z)
hoodie = dup("Hoodie"); weld(hoodie)
HOOD_KEEP = ("spine", "neck_01", "clavicle", "upperarm", "lowerarm", "pelvis")
def keep_hoodie(vdom, f):
    if any(v.co.z < hem - 0.03 for v in f.verts): return False   # rough trim; clean cut after reshape
    return any(vdom[v.index].startswith(HOOD_KEEP) and not vdom[v.index].startswith("head") for v in f.verts)
delete_faces(hoodie, keep_hoodie)
reshape(hoodie, torso_rx=0.165 if SEX == "m" else 0.150, torso_ry=0.112 if SEX == "m" else 0.100,
        torso_ztop=shoulder_z, hem_z=hem, waist=True)
smooth(hoodie, 3, 0.5)
cut_below(hoodie, hem)                      # clean horizontal hem edge
flare_hem(hoodie, hem, band=0.07, out=1.03, drop=0.0)
solidify(hoodie, 0.018)
add_folds(hoodie, 0.005, 0.15)
def paint_faces(o, mt, pred):
    names = [m.name for m in o.data.materials]
    if mt.name not in names: o.data.materials.append(mt); names.append(mt.name)
    idx = names.index(mt.name)
    for p in o.data.polygons:
        if pred(p.center): p.material_index = idx

# two-tone by POSITION (clean split): light torso, dark sleeves (|x| past torso edge) + dark collar
_shoulder_z = bone_pos("clavicle_l")[0].z
_tedge = (0.165 if SEX == "m" else 0.150) + 0.012
color_by_region(hoodie, [], HOOD_LT)                                   # all light first
paint_faces(hoodie, HOOD_DK, lambda c: abs(c.x - CX) > _tedge)        # sleeves
paint_faces(hoodie, HOOD_DK, lambda c: c.z > _shoulder_z + 0.05)      # collar
# teal: centre-front zipper + raglan seam at the shoulder/sleeve boundary
paint_faces(hoodie, ACCM, lambda c: abs(c.x - CX) < 0.013 and FY * (c.y - CY) > 0.04 and c.z < _shoulder_z + 0.02)
paint_faces(hoodie, ACCM, lambda c: abs(abs(c.x - CX) - _tedge) < 0.02 and c.z > _shoulder_z - 0.14)

# ================= CARGO PANTS =================
waist = FLOOR + 0.52 * HEIGHT
pants = dup("Pants"); weld(pants)
def keep_pants(vdom, f):
    if any(v.co.z > waist for v in f.verts): return False
    return any(vdom[v.index].startswith(("pelvis", "thigh", "calf")) for v in f.verts)
delete_faces(pants, keep_pants)
reshape(pants, torso_rx=0.16, torso_ry=0.14, torso_ztop=waist)
smooth(pants, 3, 0.5)
# ankle cuff gather
ankle_z = FLOOR + 0.085 * HEIGHT
flare_hem(pants, ankle_z, band=0.05, out=0.80, drop=0.0)   # pinch in at the cuff
solidify(pants, 0.016)
add_folds(pants, 0.006, 0.15)
pants.data.materials.clear(); pants.data.materials.append(PANTS)

# ================= recolor body under garments =================
def recolor_body():
    vdom = [dom_name(v) for v in body.data.vertices]
    skin = body.data.materials[0] if len(body.data.materials) else mat("m_skin", (0.75, 0.57, 0.47))
    body.data.materials.clear()
    for m in (skin, HOOD_LT, PANTS, GLOVE): body.data.materials.append(m)
    bm = bmesh.new(); bm.from_mesh(body.data)
    SKN = ("head", "foot", "ball", "neck")
    FING = ("thumb", "index", "middle", "ring", "pinky")
    is_tip = lambda d: d.startswith(FING) and ("_02_" in d or "_03_" in d or "_04" in d)
    is_hand = lambda d: d.startswith("hand") or d.startswith(FING)
    for f in bm.faces:
        doms = [vdom[v.index] for v in f.verts]
        leg = sum(1 for d in doms if d.startswith(("thigh", "calf")))
        tor = sum(1 for d in doms if d.startswith(("spine", "clavicle", "upperarm", "lowerarm", "pelvis")))
        skn = sum(1 for d in doms if d.startswith(SKN))
        tip = sum(1 for d in doms if is_tip(d))
        hnd = sum(1 for d in doms if is_hand(d))
        if tip >= 2: f.material_index = 0
        elif skn >= 2: f.material_index = 0
        elif hnd >= 2: f.material_index = 3
        elif leg >= tor and leg > 0: f.material_index = 2
        elif tor > 0: f.material_index = 1
        else: f.material_index = 0
    bm.to_mesh(body.data); bm.free(); body.data.update()
recolor_body()

# ================= rig-attach helpers =================
def bind_single(o, bone):
    vg = o.vertex_groups.new(name=bone); vg.add(range(len(o.data.vertices)), 1.0, 'REPLACE')
    o.modifiers.new("arm", 'ARMATURE').object = arm; o.parent = arm

def bind_transfer(o):
    md = o.modifiers.new("dt", 'DATA_TRANSFER'); md.object = body
    md.use_vert_data = True; md.data_types_verts = {'VGROUP_WEIGHTS'}
    md.vert_mapping = 'POLYINTERP_NEAREST'
    md.layers_vgroup_select_src = 'ALL'; md.layers_vgroup_select_dst = 'NAME'
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.datalayout_transfer(modifier="dt"); bpy.ops.object.modifier_apply(modifier="dt")
    o.modifiers.new("arm", 'ARMATURE').object = arm; o.parent = arm

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

def HAIRMAT():
    return bpy.data.materials.get("m_hair") or mat("m_hair", (0.11, 0.065, 0.032), 0.72)

# ================= tactical kit (reference-driven, no stray accessories) =================
def build_kit(sex):
    # ---- hi-top sneakers (grey upper + white sole + accent) ----
    ankle = FLOOR + 0.13 * HEIGHT
    shoes = dup("Shoes"); weld(shoes)
    vb = [dom_name(v) for v in shoes.data.vertices]
    bm = bmesh.new(); bm.from_mesh(shoes.data); bm.verts.ensure_lookup_table()
    dele = [f for f in bm.faces if (any(v.co.z > ankle for v in f.verts) or
            not any(vb[v.index].startswith(("foot", "ball", "calf")) for v in f.verts))]
    bmesh.ops.delete(bm, geom=dele, context='FACES'); bm.to_mesh(shoes.data); bm.free(); shoes.data.update()
    smooth(shoes, 1, 0.4); solidify(shoes, 0.012)
    shoes.data.materials.clear(); shoes.data.materials.append(SNEAK)
    for side in ("l", "r"):
        ft, _ = bone_pos(f"foot_{side}"); ba, _ = bone_pos(f"ball_{side}")
        mid = Vector(((ft.x + ba.x) / 2, (ft.y + ba.y) / 2, FLOOR + 0.022))
        sole = add_cube(f"sole_{side}", mid, Vector((0.10, 0.285, 0.045)), SOLE)
        bind_single(sole, f"foot_{side}")
        toe = add_sphere(f"toe_{side}", Vector((ba.x, ba.y + FY * 0.05, FLOOR + 0.058)), 0.052, SNEAK)
        toe.scale = Vector((1.18, 1.5, 1.0)); bpy.ops.object.transform_apply(scale=True)
        bind_single(toe, f"ball_{side}")
        acc = add_cube(f"shoe_acc_{side}", Vector((ft.x, ft.y + FY * 0.02, FLOOR + 0.095)),
                       Vector((0.085, 0.14, 0.02)), ACCM)
        bind_single(acc, f"foot_{side}")

    # ---- belt + central buckle + hip pouch ----
    hip, _ = bone_pos("pelvis"); beltz = FLOOR + 0.50 * HEIGHT
    belt = add_torus("belt", Vector((hip.x, hip.y, beltz)), 0.150, 0.026, GEAR)
    bind_single(belt, "pelvis")
    buckle = add_cube("belt_buckle", Vector((hip.x, hip.y + FY * 0.155, beltz)), Vector((0.06, 0.03, 0.05)), GEAR)
    bind_single(buckle, "pelvis")
    pouch = add_cube("pouch_hip", Vector((hip.x - 0.13, hip.y + FY * 0.08, beltz - 0.02)),
                     Vector((0.075, 0.06, 0.10)), GEAR)
    bind_single(pouch, "pelvis")

    # ---- cargo pockets (flaps) on both thighs + thigh holster strap on right ----
    for side, sgn in (("l", -1), ("r", 1)):
        thx, _ = bone_pos(f"thigh_{side}")
        flap = add_cube(f"cargo_flap_{side}", Vector((thx.x + sgn * 0.085, thx.y + FY * 0.075, thx.z - 0.14)),
                        Vector((0.095, 0.03, 0.085)), GEAR)
        bind_single(flap, f"thigh_{side}")
        # team-colour diagonal accent stripe on the front thigh
        st = add_cube(f"thigh_stripe_{side}", Vector((thx.x + sgn * 0.035, thx.y + FY * 0.105, thx.z - 0.07)),
                      Vector((0.028, 0.02, 0.24)), ACCM, rot=(sgn * -0.28, 0, 0))
        bind_single(st, f"thigh_{side}")
    thr, _ = bone_pos("thigh_r")
    hol = add_cube("thigh_holster", Vector((thr.x + 0.06, thr.y + FY * 0.09, thr.z - 0.04)),
                   Vector((0.07, 0.05, 0.11)), GEAR)
    bind_single(hol, "thigh_r")

    # ---- plain dark knee pads (no stripe) ----
    for side in ("l", "r"):
        kn, _ = bone_pos(f"calf_{side}")
        pad = add_sphere(f"knee_{side}", Vector((kn.x, kn.y + FY * 0.080, kn.z + 0.02)), 0.072, KNEE)
        pad.scale = Vector((1.0, 0.55, 1.2)); bpy.ops.object.transform_apply(scale=True)
        bind_single(pad, f"calf_{side}")

    # ---- H-harness: 2 front vertical straps + chest buckle + X across the back ----
    ch, _ = bone_pos("spine_03")
    cbk = add_cube("chest_buckle", Vector((ch.x, ch.y + FY * 0.135, ch.z - 0.02)), Vector((0.05, 0.03, 0.055)), GEAR)
    bind_transfer(cbk)
    for side, s in (("l", -1), ("r", 1)):
        # front strap: shoulder -> chest buckle
        fs = add_cube(f"fstrap_{side}", Vector((ch.x + s * 0.07, ch.y + FY * 0.125, ch.z + 0.04)),
                      Vector((0.035, 0.03, 0.22)), GEAR, rot=(0, s * FY * 0.18, 0))
        bind_transfer(fs)
        # back diagonal strap (X)
        bs = add_cube(f"bstrap_{side}", Vector((ch.x + s * 0.04, ch.y - FY * 0.135, ch.z + 0.02)),
                      Vector((0.035, 0.03, 0.26)), GEAR, rot=(0, s * FY * 0.5, 0))
        bind_transfer(bs)

    # ---- teal/orange piping: colour a thin band of hoodie faces along each shoulder seam ----
    # (done as a vertex-region recolour on the hoodie itself, not floating cubes)

    # ---- hood: small charcoal cowl sitting at the back of the neck ----
    nk, _ = bone_pos("neck_01")
    hood = add_sphere("hood", Vector((nk.x, nk.y - FY * 0.055, nk.z - 0.01)), 0.095, HOOD_DK)
    hood.scale = Vector((1.0, 0.85, 1.15)); bpy.ops.object.transform_apply(scale=True)
    bmh = bmesh.new(); bmh.from_mesh(hood.data)
    kill = [v for v in bmh.verts if v.co.y * FY > -0.015]
    bmesh.ops.delete(bmh, geom=kill, context='VERTS'); bmh.to_mesh(hood.data); bmh.free(); hood.data.update()
    bind_transfer(hood)

    if sex == "f":
        hv = [v.co for v in body.data.vertices if dom_name(v).startswith("Head")]
        hx = sum(v.x for v in hv) / len(hv)
        hzmax = max(v.z for v in hv); hzmin = min(v.z for v in hv)
        hymin = min(v.y for v in hv); hymax = max(v.y for v in hv); cy = (hymin + hymax) / 2
        front_y = hymax if FY > 0 else hymin; back_y = hymin if FY > 0 else hymax
        forehead_z = hzmin + 0.80 * (hzmax - hzmin)
        HM = HAIRMAT()
        # ---- high ponytail: tie at the crown-back + one smooth tapered tail swept down-back ----
        tie_pt = Vector((hx, back_y + FY * 0.015, hzmax - 0.02))
        tie = add_sphere("pony_tie", tie_pt, 0.05, HM)
        tie.scale = Vector((1.0, 1.05, 0.85)); bpy.ops.object.transform_apply(scale=True)
        bind_single(tie, "Head")
        tail_dir = Vector((0, -FY * 0.55, -1.0)).normalized()
        bpy.ops.mesh.primitive_cone_add(radius1=0.052, radius2=0.013, depth=0.32,
                                        location=tie_pt + tail_dir * 0.15)
        pony = bpy.context.active_object; pony.name = "ponytail"
        pony.rotation_euler = tail_dir.to_track_quat('Z', 'Y').to_euler()
        bpy.ops.object.transform_apply(rotation=True)
        pony.data.materials.clear(); pony.data.materials.append(HM)
        smooth(pony, 1, 0.3); bind_single(pony, "Head")
        # ---- slim goggles: thin dark frame band + flat accent lens on the forehead ----
        band = add_torus("goggle_band", Vector((hx, cy, forehead_z)), 0.090, 0.010, GEAR)
        band.scale = Vector((1.0, 1.0, 0.6)); bpy.ops.object.transform_apply(scale=True)
        bind_single(band, "Head")
        lens = add_cube("goggle_lens", Vector((hx, front_y + FY * 0.002, forehead_z)),
                        Vector((0.145, 0.018, 0.03)), ACCM)
        bind_single(lens, "Head")

build_kit(SEX)

# ---------------- hair ----------------
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=HAIR)
hair_new = [o for o in bpy.data.objects if o not in before]
for o in hair_new:
    if o.type == 'ARMATURE': bpy.data.objects.remove(o, do_unlink=True)
HAIR_M = HAIRMAT()
for hm in [o for o in hair_new if o.type == 'MESH']:
    hm.data.materials.clear(); hm.data.materials.append(HAIR_M)
    vg = hm.vertex_groups.new(name="Head"); vg.add(range(len(hm.data.vertices)), 1.0, 'REPLACE')
    hm.modifiers.new("arm", 'ARMATURE').object = arm; hm.parent = arm

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
