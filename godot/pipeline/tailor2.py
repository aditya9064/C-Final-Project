"""Tailor2 -- rebuild Fireteam operators with LOFTED garment meshes (not body-derived).

Each garment is its own tube of rings with regular topology: materials assign by
(ring, segment) index -> clean panels, seams and accent strips, never jagged islands.
The garment radii (measured to the reference) set the silhouette, so the muscular base
underneath is irrelevant where it is covered.  Visible skin (head/hands/neck) is kept;
weights come from the body by Data Transfer so everything deforms with the 12 clips.

Args after -- : base_gltf ual_glb hair_gltf out_glb  accentR accentG accentB  sex(m/f)
"""
import bpy, bmesh, sys, math
from mathutils import Vector
a = sys.argv[sys.argv.index("--") + 1:]
BASE, UAL, HAIR, OUT = a[0], a[1], a[2], a[3]
ACC = (float(a[4]), float(a[5]), float(a[6]))
SEX = a[7] if len(a) > 7 else "m"
M = (SEX == "m")

# ---------------- materials (corrected to the reference) ----------------
def mat(name, rgb, rough=0.82, metal=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m

HOOD   = mat("hoodie",   (0.34, 0.35, 0.37), 0.9)    # light grey hoodie (albedo kept down so arena light lands ~light-grey, not white)
SLEEVE = mat("sleeve",   (0.135, 0.14, 0.15), 0.88)  # darker charcoal sleeves (user: "darker sleeves")
CUFF   = mat("cuff",     (0.235, 0.245, 0.265), 0.87) # ribbed cuffs / collar edge (mid grey)
TEE    = mat("tee",      (0.075, 0.078, 0.088), 0.9) # dark tee under the open collar
PANTS  = mat("pants",    (0.085, 0.088, 0.10), 0.9)  # dark charcoal cargo joggers
GEAR   = mat("gear",     (0.020, 0.020, 0.024), 0.55)# black webbing / harness / pouches
ACCM   = mat("accent",   ACC, 0.42)                  # teal / orange
SNEAK  = mat("sneak",    (0.20, 0.21, 0.23), 0.7)    # dark sneaker upper
SNKGRY = mat("snk_grey", (0.55, 0.56, 0.58), 0.7)    # grey sneaker panel
SOLE   = mat("sole",     (0.82, 0.83, 0.84), 0.6)    # white sole
GLOVE  = mat("glove",    (0.032, 0.032, 0.038), 0.6) # dark fingerless gloves
KNEE   = mat("knee",     (0.055, 0.058, 0.065), 0.72) # dark knee pad
HAIRM  = mat("hair",     (0.14, 0.085, 0.045), 0.7)  # brown hair
SKINM  = None  # set after import

# ---------------- import base ----------------
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
for c in (bpy.data.actions, bpy.data.meshes, bpy.data.armatures):
    for b in list(c):
        if b.users == 0: c.remove(b)
bpy.ops.import_scene.gltf(filepath=BASE)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
body = max(meshes, key=lambda o: len(o.data.vertices))
# Natural fair/light skin (the only shipped base texture is the "Dark" variant, so we use a
# fresh flat tone; the eyes + eyebrows are separate meshes that survive and give the face features).
SKINM = mat("skin", (0.72, 0.44, 0.33), 0.5)

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
print("height", round(HEIGHT, 3), "floor", round(FLOOR, 3))

def pos(name):
    b = arm.data.bones.get(name)
    return (arm.matrix_world @ b.head_local) if b else None
FY = 1.0
_bh = arm.data.bones.get("ball_l"); _ft = arm.data.bones.get("foot_l")
if _bh and _ft:
    FY = 1.0 if (_bh.head_local.y - _ft.head_local.y) > 0 else -1.0
_sp = pos("spine_02"); CY = _sp.y if _sp else (max(ys) + min(ys)) / 2
FRONT_ANG = (math.pi / 2) if FY > 0 else (3 * math.pi / 2)
def ang_to_front(ang):
    return abs(((ang - FRONT_ANG + math.pi) % (2 * math.pi)) - math.pi)  # 0 at exact front

# ---------------- mesh building (regular-topology lofts) ----------------
def make(name, verts, faces, mats, face_mat):
    me = bpy.data.meshes.new(name); me.from_pydata(verts, [], faces); me.update()
    for m in mats: me.materials.append(m)
    for i, p in enumerate(me.polygons):
        p.material_index = face_mat[i]; p.use_smooth = True
    ob = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(ob)
    return ob

def ellipse_ring(cx, cy, z, rx, ry, nseg, front_push=0.0, front_bulge=0.0, front_bulge_z=0.0):
    """nseg verts around an ellipse; optional forward push & a soft front bulge (bust)."""
    out = []
    for i in range(nseg):
        ang = 2 * math.pi * i / nseg          # 0 at +X, CCW
        x = cx + rx * math.cos(ang)
        y = cy + ry * math.sin(ang)
        fwd = (math.sin(ang) * FY)            # 1 at front, -1 at back
        if fwd > 0:
            y += FY * front_push
            if front_bulge > 0:
                y += FY * front_bulge * (fwd ** 2)
        out.append(Vector((x, y, z)))
    return out

def stitch_rings(rings, nseg):
    """faces between consecutive rings; returns (verts, faces, ring_of_face, seg_of_face)."""
    verts = []; faces = []; rof = []; sof = []
    for r in rings: verts.extend(r)
    for ri in range(len(rings) - 1):
        b0 = ri * nseg; b1 = (ri + 1) * nseg
        for si in range(nseg):
            sn = (si + 1) % nseg
            faces.append([b0 + si, b0 + sn, b1 + sn, b1 + si])
            rof.append(ri); sof.append(si)
    return verts, faces, rof, sof

def seg_at_front(si, nseg):
    """angle of segment si; front(+Y*FY) center is at ang=pi/2."""
    return 2 * math.pi * (si + 0.5) / nseg

# ================= HOODIE (torso tube + sleeves) =================
def build_hoodie():
    hem_z   = FLOOR + (0.47 if M else 0.49) * HEIGHT
    waist_z = FLOOR + (0.56 if M else 0.57) * HEIGHT
    chest_z = FLOOR + (0.68 if M else 0.69) * HEIGHT
    shld_z  = pos("clavicle_l").z
    neck_z  = pos("neck_01").z
    top_z   = neck_z + 0.02
    nseg = 40
    # radii: lean. male slightly broader; female narrower torso, defined waist, slight bust
    rx_hip  = 0.150 if M else 0.146
    rx_wst  = 0.138 if M else 0.114
    rx_chs  = 0.162 if M else 0.138
    rx_shl  = 0.170 if M else 0.138
    ry = lambda rx: rx * (0.70 if M else 0.74)
    levels = [
        (hem_z,            rx_hip, ry(rx_hip),  0.0,   0.0),
        (hem_z+0.05,       rx_hip, ry(rx_hip),  0.0,   0.0),   # hem band
        (waist_z,          rx_wst, ry(rx_wst),  0.0,   0.0),
        (chest_z,          rx_chs, ry(rx_chs),  0.0,   (0.0 if M else 0.072)),  # bust (female)
        (shld_z,           rx_shl, ry(rx_shl),  0.0,   0.0),
        (shld_z+0.055,     0.105,  0.098,       0.0,   0.0),   # collar base
        (top_z,            0.082,  0.078,       0.0,   0.0),   # collar top
    ]
    rings = [ellipse_ring(CX, CY, z, rx, r_y, nseg, front_bulge=fb) for (z, rx, r_y, fp, fb) in levels]
    verts, faces, rof, sof = stitch_rings(rings, nseg)
    # material per face: body light; collar top two rings -> CUFF; open-front dark tee V at chest/collar
    fm = []
    nlev = len(levels)
    for i in range(len(faces)):
        ri, si = rof[i], sof[i]
        ang = seg_at_front(si, nseg)
        fm.append(0)  # HOOD
        if ri >= nlev - 3:            # collar region
            fm[-1] = 1                # CUFF
        # dark tee: narrow front-centre V, widening upward (open zip over a tee)
        dfront = ang_to_front(ang)
        if ri >= 3 and dfront < (0.16 + 0.11 * (ri - 3)):
            fm[-1] = 2                # TEE
    ob = make("Hoodie", verts, faces, [HOOD, CUFF, TEE], fm)
    # zipper placket: thin accent line at exact front centre, chest->hem
    return ob, dict(hem_z=hem_z, shld_z=shld_z, chest_z=chest_z, neck_z=neck_z, nseg=nseg)

def build_sleeve(side):
    s = 1 if side == "r" else -1
    ua_h = pos(f"upperarm_{side}"); la_h = pos(f"lowerarm_{side}"); hnd = pos(f"hand_{side}")
    # chain points: shoulder, elbow, wrist
    pts = [ua_h, la_h, hnd]
    nseg = 20; nres = 10
    # build rings along the chain, radius tapering to the wrist, with a ribbed cuff at the end
    rings = []; radii = []
    for k in range(nres + 1):
        t = k / nres
        # piecewise along [shoulder->elbow->wrist]
        if t < 0.5:
            tt = t / 0.5; p = pts[0].lerp(pts[1], tt); nxt = pts[1]
        else:
            tt = (t - 0.5) / 0.5; p = pts[1].lerp(pts[2], tt); nxt = pts[2]
        R = (0.078 if M else 0.068) * (1.0 - 0.28 * t)
        radii.append(R)
        rings.append((p, R))
    # axis per ring from neighbor
    verts = []; faces = []; rof = []; sof = []
    ringverts = []
    for k, (p, R) in enumerate(rings):
        if k < len(rings) - 1: axis = (rings[k+1][0] - p)
        else: axis = (p - rings[k-1][0])
        axis = axis.normalized()
        # build an orthonormal frame
        up = Vector((0, 0, 1))
        if abs(axis.dot(up)) > 0.95: up = Vector((0, 1, 0))
        u = axis.cross(up).normalized(); w = u.cross(axis).normalized()
        ring = []
        cuff = (k >= nres - 1)
        Rr = R * (1.12 if cuff else 1.0)
        for i in range(nseg):
            ang = 2 * math.pi * i / nseg
            ring.append(p + u * (Rr * math.cos(ang)) + w * (Rr * math.sin(ang)))
        ringverts.append(ring)
    for r in ringverts: verts.extend(r)
    for k in range(len(ringverts) - 1):
        b0 = k * nseg; b1 = (k + 1) * nseg
        for i in range(nseg):
            ni = (i + 1) % nseg
            faces.append([b0 + i, b0 + ni, b1 + ni, b1 + i]); rof.append(k); sof.append(i)
    fm = []
    for i in range(len(faces)):
        ri, si = rof[i], sof[i]
        if ri >= nres - 1: fm.append(1)  # CUFF
        else: fm.append(0)               # HOOD light
    ob = make(f"Sleeve_{side}", verts, faces, [SLEEVE, CUFF, ACCM], fm)  # darker charcoal sleeves
    # accent stripe along the OUTER-TOP of the upper arm (clean longitudinal strip)
    me = ob.data
    for i, p in enumerate(me.polygons):
        ri, si = rof[i], sof[i]
        if ri <= 4:
            # outer-top direction ~ +Z and +X*s; pick contiguous seg band near the top
            ang = 2 * math.pi * (si + 0.5) / nseg
            # top of ring is where local w (~up) is max -> sin(ang) large
            if math.sin(ang) > 0.72:
                p.material_index = 2  # ACCENT
    return ob

# ================= CARGO PANTS =================
def build_leg(side):
    s = 1 if side == "r" else -1
    th = pos(f"thigh_{side}"); cf = pos(f"calf_{side}"); ft = pos(f"foot_{side}")
    pts = [th, cf, ft]
    nseg = 22; nres = 12
    ringverts = []; rof = []; sof = []
    rings = []
    for k in range(nres + 1):
        t = k / nres
        if t < 0.5:
            tt = t / 0.5; p = pts[0].lerp(pts[1], tt)
        else:
            tt = (t - 0.5) / 0.5; p = pts[1].lerp(pts[2], tt)
        R = (0.115 if M else 0.108) * (1.0 - 0.34 * t)
        rings.append((p, R))
    for k, (p, R) in enumerate(rings):
        if k < len(rings) - 1: axis = (rings[k+1][0] - p)
        else: axis = (p - rings[k-1][0])
        axis = axis.normalized()
        up = Vector((0, 0, 1))
        if abs(axis.dot(up)) > 0.95: up = Vector((0, 1, 0))
        u = axis.cross(up).normalized(); w = u.cross(axis).normalized()
        cuff = (k >= nres - 1)
        Rr = R * (1.18 if cuff else 1.0)
        ring = [p + u * (Rr * math.cos(2*math.pi*i/nseg)) + w * (Rr * math.sin(2*math.pi*i/nseg)) for i in range(nseg)]
        ringverts.append(ring)
    verts = []; faces = []
    for r in ringverts: verts.extend(r)
    for k in range(len(ringverts) - 1):
        b0 = k*nseg; b1 = (k+1)*nseg
        for i in range(nseg):
            ni = (i+1) % nseg
            faces.append([b0+i, b0+ni, b1+ni, b1+i]); rof.append(k); sof.append(i)
    fm = [1 if rof[i] >= nres - 1 else 0 for i in range(len(faces))]  # ankle cuff -> CUFFdark
    ob = make(f"Leg_{side}", verts, faces, [PANTS, GEAR, ACCM], fm)
    # thigh accent diagonal stripe: a short clean band on the OUTER-FRONT mid-thigh
    for i, p in enumerate(ob.data.polygons):
        ri, si = rof[i], sof[i]
        ang = 2*math.pi*(si+0.5)/nseg
        front = math.sin(ang) * FY
        outer = math.cos(ang) * s
        if ri in (4, 5) and front > 0.15 and outer > 0.55:
            p.material_index = 2
    return ob

# ===== HIP / PELVIS bridge (closes the crotch; both leg tubes join under it) =====
def build_hips():
    waist_z = FLOOR + (0.56 if M else 0.57) * HEIGHT
    top_z   = FLOOR + (0.50 if M else 0.51) * HEIGHT   # overlaps hoodie hem
    seat_z  = FLOOR + (0.44 if M else 0.45) * HEIGHT   # bottom of the seat
    hip = pos("pelvis"); nseg = 26
    rx = 0.155 if M else 0.152; ry = rx * (0.74 if M else 0.78)
    levels = [(top_z, rx*0.96, ry*0.96), (waist_z*0+ (top_z+seat_z)/2, rx, ry), (seat_z, rx*0.98, ry*1.02)]
    rings = [ellipse_ring(CX, CY, z, rxx, ryy, nseg) for (z, rxx, ryy) in levels]
    verts, faces, rof, sof = stitch_rings(rings, nseg)
    ob = make("Hips", verts, faces, [PANTS], [0]*len(faces))
    return ob

# build garments
hoodie, HD = build_hoodie()
hips = build_hips()
sleeves = [build_sleeve("l"), build_sleeve("r")]
legs = [build_leg("l"), build_leg("r")]

# ---- zipper placket (front centre, thin accent) ----
def zipper():
    z0 = HD["hem_z"] + 0.02; z1 = HD["chest_z"] + 0.03
    fy = CY + (0.142 if M else 0.128) * FY
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(CX, fy, (z0+z1)/2))
    o = bpy.context.active_object; o.name = "zipper"
    o.scale = Vector((0.012, 0.012, (z1 - z0)/2)); bpy.ops.object.transform_apply(scale=True)
    o.data.materials.append(ACCM)
    for p in o.data.polygons: p.use_smooth = False
    return o
zip_o = zipper()

# ===== slim the hidden body so it sits INSIDE every garment (no poke-through) =====
BODY_R = {"upperarm": 0.060, "lowerarm": 0.048, "thigh": 0.098, "calf": 0.068}
def slim_body():
    vdom = [dom_name(v) for v in body.data.vertices]
    bm = bmesh.new(); bm.from_mesh(body.data); bm.verts.ensure_lookup_table()
    for v in bm.verts:
        d = vdom[v.index]
        # torso: clamp into an ellipse well inside the hoodie
        if d.startswith(("spine", "clavicle", "pelvis")):
            dx = v.co.x - CX; dy = v.co.y - CY; dd = math.hypot(dx, dy)
            if dd > 1e-6:
                ang = math.atan2(dy, dx); rx, ry = 0.135, 0.092
                er = (rx*ry)/math.sqrt((ry*math.cos(ang))**2 + (rx*math.sin(ang))**2 + 1e-9)
                if dd > er:
                    v.co.x = CX + dx/dd*er; v.co.y = CY + dy/dd*er
            continue
        key = next((k for k in BODY_R if d.startswith(k)), None)
        if not key: continue
        side = "l" if d.endswith("_l") else ("r" if d.endswith("_r") else None)
        bone = key + ("_" + side if side else "")
        h = pos(bone); t = pos({"upperarm":"lowerarm","lowerarm":"hand","thigh":"calf","calf":"foot"}[key] + ("_"+side if side else ""))
        if not h or not t: continue
        ax = (t - h); L = ax.length
        if L < 1e-6: continue
        axn = ax / L; tt = max(0.0, min(1.0, (v.co - h).dot(axn) / L))
        axpt = h + axn * (tt * L); radial = v.co - axpt; dd = radial.length
        R = BODY_R[key]
        if dd > R and dd > 1e-6: v.co = axpt + radial / dd * R
    bm.to_mesh(body.data); bm.free(); body.data.update()
slim_body()

# ===== recolor body: visible skin kept, covered regions darkened (no hollow peek) =====
def process_body():
    vdom = [dom_name(v) for v in body.data.vertices]
    body.data.materials.clear()
    for m in (SKINM, GLOVE, TEE, PANTS): body.data.materials.append(m)  # 0 skin 1 glove 2 dark-top 3 dark-leg
    SKN = ("head", "neck")   # feet are covered by shoes -> keep them dark, not skin
    FING = ("thumb", "index", "middle", "ring", "pinky")
    is_hand = lambda d: d.startswith("hand") or d.startswith(FING)
    for f in body.data.polygons:
        doms = [vdom[v].lower() for v in f.vertices]   # lower(): base group is "Head" (capital)
        skn = sum(1 for d in doms if d.startswith(SKN))
        hnd = sum(1 for d in doms if is_hand(d))
        leg = sum(1 for d in doms if d.startswith(("thigh", "calf")))
        if skn >= 2: f.material_index = 0
        elif hnd >= 2: f.material_index = 1
        elif leg >= 2: f.material_index = 3
        else: f.material_index = 2
        f.use_smooth = True
process_body()
# drop custom split normals so muscle shading doesn't survive on visible skin
try:
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.mesh.customdata_custom_splitnormals_clear()
except Exception as e: print("normals clear:", e)

# ---------------- rig-attach helpers ----------------
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

# body-derived weights for garments
for g in [hoodie, hips] + sleeves + legs:
    bind_transfer(g)
bind_transfer(zip_o)

def add_cube(name, loc, size, mt, rot=None, smooth=False):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    o = bpy.context.active_object; o.name = name; o.scale = size
    if rot: o.rotation_euler = rot
    bpy.ops.object.transform_apply(scale=True, rotation=bool(rot))
    o.data.materials.append(mt)
    for p in o.data.polygons: p.use_smooth = smooth
    return o
def add_sphere(name, loc, r, mt):
    bpy.ops.mesh.primitive_uv_sphere_add(location=loc, radius=r, segments=24, ring_count=14)
    o = bpy.context.active_object; o.name = name; o.data.materials.append(mt)
    for p in o.data.polygons: p.use_smooth = True   # smooth: no faceted balls (toe caps, shoulder caps, hood)
    return o
def add_torus(name, loc, major, minor, mt, rot=None):
    bpy.ops.mesh.primitive_torus_add(location=loc, major_radius=major, minor_radius=minor,
                                     major_segments=28, minor_segments=12)
    o = bpy.context.active_object; o.name = name
    if rot: o.rotation_euler = rot; bpy.ops.object.transform_apply(rotation=True)
    o.data.materials.append(mt); return o

# ================= tactical kit =================
def build_kit():
    # hood cowl at the back of the neck (light grey outside)
    nk = pos("neck_01")
    hood = add_sphere("hood", Vector((nk.x, nk.y - FY*0.05, nk.z + 0.0)), 0.10, HOOD)
    hood.scale = Vector((1.05, 0.9, 1.1)); bpy.ops.object.transform_apply(scale=True)
    bmh = bmesh.new(); bmh.from_mesh(hood.data)
    kill = [v for v in bmh.verts if (v.co.y - nk.y) * FY > -0.01]
    bmesh.ops.delete(bmh, geom=kill, context='VERTS'); bmh.to_mesh(hood.data); bmh.free()
    bind_transfer(hood)

    # shoulder caps: cover the deltoid gap between the torso tube and each sleeve.
    # SLEEVE colour so the arm reads charcoal from the shoulder down (raglan seam).
    # smaller/narrower on the female so the shoulders don't read as pads.
    capR = 0.092 if M else 0.066
    for side, s in (("l", -1), ("r", 1)):
        sh = pos(f"upperarm_{side}"); cl = pos(f"clavicle_{side}")
        cap_pt = sh.lerp(cl, 0.30 if M else 0.45) + Vector((0, 0, 0.015))
        cap = add_sphere(f"shldcap_{side}", cap_pt, capR, SLEEVE)
        cap.scale = Vector((1.05, 1.0, 0.8)); bpy.ops.object.transform_apply(scale=True)
        bind_transfer(cap)

    # H-harness: front straps + chest buckle + X on the back
    ch = pos("spine_03")
    cbk = add_cube("chest_buckle", Vector((ch.x, ch.y + FY*0.15, ch.z - 0.01)), Vector((0.055, 0.03, 0.06)), GEAR)
    bind_transfer(cbk)
    for side, s in (("l", -1), ("r", 1)):
        fs = add_cube(f"fstrap_{side}", Vector((ch.x + s*0.072, ch.y + FY*0.14, ch.z + 0.055)),
                      Vector((0.032, 0.03, 0.24)), GEAR, rot=(0, s*FY*0.16, 0))
        bind_transfer(fs)
        bs = add_cube(f"bstrap_{side}", Vector((ch.x + s*0.03, ch.y - FY*0.15, ch.z + 0.0)),
                      Vector((0.032, 0.028, 0.30)), GEAR, rot=(0, s*FY*0.52, 0))
        bind_transfer(bs)

    # belt + buckle + hip pouches
    hip = pos("pelvis"); bz = FLOOR + (0.50 if M else 0.51) * HEIGHT
    belt = add_torus("belt", Vector((hip.x, hip.y, bz)), (0.150 if M else 0.146), 0.022, GEAR)
    bind_single(belt, "pelvis")
    bk = add_cube("belt_buckle", Vector((hip.x, hip.y + FY*0.152, bz)), Vector((0.05, 0.028, 0.045)), GEAR)
    bind_single(bk, "pelvis")
    for side, s in (("l", -1), ("r", 1)):
        pz = add_cube(f"pouch_{side}", Vector((hip.x + s*0.12, hip.y + FY*0.07, bz - 0.03)),
                      Vector((0.07, 0.06, 0.10)), GEAR)
        bind_single(pz, "pelvis")
    # thigh holster (right) + utility pouch block (left front, as in ref)
    thr = pos("thigh_r")
    hol = add_cube("holster", Vector((thr.x + 0.075, thr.y + FY*0.05, thr.z - 0.05)), Vector((0.05, 0.065, 0.13)), GEAR)
    bind_single(hol, "thigh_r")

    # cargo pocket flaps + knee pads
    for side, s in (("l", -1), ("r", 1)):
        thx = pos(f"thigh_{side}")
        fl = add_cube(f"cargo_{side}", Vector((thx.x + s*0.10, thx.y + FY*0.06, thx.z - 0.13)),
                      Vector((0.085, 0.03, 0.08)), GEAR)
        bind_single(fl, f"thigh_{side}")
        kn = pos(f"calf_{side}")
        pad = add_cube(f"knee_{side}", Vector((kn.x, kn.y + FY*0.085, kn.z + 0.02)), Vector((0.10, 0.06, 0.12)), KNEE, smooth=True)
        bind_single(pad, f"calf_{side}")

    # hi-top sneakers: dark upper (body-derived) + grey panel + white sole + accent + toe
    ankle = FLOOR + 0.12 * HEIGHT
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
    bpy.context.view_layer.objects.active = body; bpy.ops.object.duplicate()
    shoes = bpy.context.active_object; shoes.name = "Shoes"
    bm = bmesh.new(); bm.from_mesh(shoes.data); bm.verts.ensure_lookup_table()
    vb = [dom_name(v) for v in shoes.data.vertices]
    dele = [f for f in bm.faces if any(v.co.z > ankle for v in f.verts) or
            not any(vb[v.index].startswith(("foot", "ball", "calf")) for v in f.verts)]
    bmesh.ops.delete(bm, geom=dele, context='FACES'); bm.to_mesh(shoes.data); bm.free()
    shoes.data.materials.clear(); shoes.data.materials.append(SNEAK)
    m = shoes.modifiers.new("s", 'SOLIDIFY'); m.thickness = 0.012
    for side in ("l", "r"):
        ft = pos(f"foot_{side}"); ba = pos(f"ball_{side}")
        mid = Vector(((ft.x+ba.x)/2, (ft.y+ba.y)/2, FLOOR + 0.020))
        # white sole: a thin slab under the foot (pulled back so it doesn't protrude past the toe)
        sole = add_cube(f"sole_{side}", Vector((mid.x, mid.y - FY*0.015, FLOOR + 0.018)),
                        Vector((0.102, 0.255, 0.036)), SOLE); bind_single(sole, f"foot_{side}")
        gp = add_cube(f"snk_grey_{side}", Vector((ft.x, ft.y + FY*0.02, FLOOR+0.085)), Vector((0.095, 0.16, 0.05)), SNKGRY)
        bind_single(gp, f"foot_{side}")
        # toe box: a low, flat, wide rounded cap in the dark upper colour (not a proud grey ball)
        toe = add_cube(f"toe_{side}", Vector((ba.x, ba.y + FY*0.045, FLOOR+0.044)),
                       Vector((0.098, 0.085, 0.072)), SNEAK, smooth=True); bind_single(toe, f"ball_{side}")
        acc = add_cube(f"snk_acc_{side}", Vector((ft.x, ft.y + FY*0.075, FLOOR+0.10)), Vector((0.08, 0.05, 0.022)), ACCM)
        bind_single(acc, f"foot_{side}")
    shoes.parent = arm; shoes.modifiers.new("arm", 'ARMATURE').object = arm

    # fingerless-glove cuff rings at the wrists
    for side in ("l", "r"):
        hd = pos(f"hand_{side}"); lw = pos(f"lowerarm_{side}")
        cuffpt = hd.lerp(lw, 0.35)
        cuff = add_torus(f"gcuff_{side}", cuffpt, 0.052, 0.014, GEAR)
        bind_single(cuff, f"lowerarm_{side}")

    if SEX == "f":
        hv = [v.co for v in body.data.vertices if dom_name(v).lower().startswith("head")]
        if hv:
            hx = sum(v.x for v in hv)/len(hv); hz0 = min(v.z for v in hv); hz1 = max(v.z for v in hv)
            hy0 = min(v.y for v in hv); hy1 = max(v.y for v in hv); cy = (hy0+hy1)/2
            hx0 = min(v.x for v in hv); hx1 = max(v.x for v in hv)
            back_y = hy0 if FY > 0 else hy1; front_y = hy1 if FY > 0 else hy0
            forehead_z = hz0 + 0.68*(hz1-hz0)
            # pulled-back scalp cap (replaces the male parted hair): hair over top/back/sides
            cap = add_sphere("f_scalp", Vector((hx, cy - FY*0.005, (hz0+hz1)/2 + 0.01)), 1.0, HAIRM)
            cap.scale = Vector(((hx1-hx0)/2*1.10, (hy1-hy0)/2*1.12, (hz1-hz0)/2*1.08))
            bpy.ops.object.transform_apply(scale=True)
            bmc = bmesh.new(); bmc.from_mesh(cap.data)
            kill = [v for v in bmc.verts if (v.co.z < forehead_z and (v.co.y - cy)*FY > 0.002)]  # open the face
            bmesh.ops.delete(bmc, geom=kill, context='VERTS'); bmc.to_mesh(cap.data); bmc.free()
            for p in cap.data.polygons: p.use_smooth = True
            bind_single(cap, "Head")
            # face-framing bangs: two small flattened strands at the temples
            for s in (-1, 1):
                bng = add_sphere(f"f_bang_{'lr'[(s+1)//2]}", Vector((hx + s*(hx1-hx0)*0.42, front_y - FY*0.01, forehead_z-0.01)), 0.045, HAIRM)
                bng.scale = Vector((0.5, 0.7, 1.3)); bpy.ops.object.transform_apply(scale=True)
                bind_single(bng, "Head")
            # high ponytail: tie + swept tapered tail
            tie_pt = Vector((hx, back_y + FY*0.01, hz1 - 0.02))
            tie = add_sphere("pony_tie", tie_pt, 0.048, HAIRM); tie.scale = Vector((1,1.05,0.85))
            bpy.ops.object.transform_apply(scale=True); bind_single(tie, "Head")
            tdir = Vector((0, -FY*0.5, -1.0)).normalized()
            bpy.ops.mesh.primitive_cone_add(radius1=0.05, radius2=0.012, depth=0.30, location=tie_pt + tdir*0.15)
            pony = bpy.context.active_object; pony.name = "ponytail"
            pony.rotation_euler = tdir.to_track_quat('Z','Y').to_euler(); bpy.ops.object.transform_apply(rotation=True)
            pony.data.materials.append(HAIRM)
            for p in pony.data.polygons: p.use_smooth = True
            bind_single(pony, "Head")
            # slim goggles on the forehead: thin dark band + accent lens
            fz = hz0 + 0.82*(hz1-hz0)
            band = add_torus("goggle_band", Vector((hx, cy, fz)), 0.088, 0.009, GEAR)
            band.scale = Vector((1,1,0.55)); bpy.ops.object.transform_apply(scale=True); bind_single(band, "Head")
            lens = add_cube("goggle_lens", Vector((hx, front_y + FY*0.004, fz)), Vector((0.14, 0.016, 0.028)), ACCM, smooth=True)
            bind_single(lens, "Head")
build_kit()

# ---------------- hair (male: imported tousled parted; female: built pulled-back, in build_kit) ----------------
if M:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=HAIR)
    hair_new = [o for o in bpy.data.objects if o not in before]
    for o in hair_new:
        if o.type == 'ARMATURE': bpy.data.objects.remove(o, do_unlink=True)
    for hm in [o for o in hair_new if o.type == 'MESH']:
        hm.data.materials.clear(); hm.data.materials.append(HAIRM)
        vg = hm.vertex_groups.new(name="Head"); vg.add(range(len(hm.data.vertices)), 1.0, 'REPLACE')
        hm.modifiers.new("arm", 'ARMATURE').object = arm; hm.parent = arm

# ---------------- eyebrows brown (keep from base if present) ----------------
for mm in meshes:
    if mm is body: continue
    if "brow" in mm.name.lower():
        mm.data.materials.clear(); mm.data.materials.append(mat("brow", (0.12, 0.07, 0.035), 0.8))

# ---------------- animations ----------------
MAP = {'Idle_Loop':'idle_loop','Walk_Loop':'walk_loop','Sprint_Loop':'run_loop','Jog_Fwd_Loop':'jog_loop',
       'Jump_Start':'jump_start','Jump_Loop':'jump_loop','Jump_Land':'jump_land','Death01':'death',
       'Pistol_Aim_Neutral':'aim','Pistol_Shoot':'fire','Pistol_Reload':'reload','Pistol_Idle_Loop':'aim_idle_loop'}
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=UAL)
ual_objs = [o for o in bpy.data.objects if o not in before]
kept = {}
for key, clean in MAP.items():
    mm = next((ac for ac in bpy.data.actions if ac.name.startswith(key)), None)
    if mm: mm.name = clean; mm.use_fake_user = True; kept[clean] = mm
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
print("TAILORED2:", OUT, "clips:", sorted(kept.keys()))
