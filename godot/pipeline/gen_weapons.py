"""
Fireteam: Skirmish — procedural weapon generator (Blender 4.2 headless).

Builds the compact SMG from the character concept sheet as single-piece GLBs with
real material slots (dark polymer, light-grey receiver panel, steel, and one ACCENT
slot). Exports a teal variant and an orange variant to assets/models/weapons/.

Run (GLB export needs factory startup so the glTF add-on loads cleanly):
    blender --background --factory-startup --python gen_weapons.py -- <out_models_dir>

Blender is Z-up; the gun is modelled lying along +X (muzzle toward +X), up is +Z,
thickness is +/-Y. glTF export converts to Y-up for Godot. Mirrors the conventions
in gen_assets.py (cube/cyl helpers, apply transforms, join, export GLB).
"""
import bpy, math, os, sys
from mathutils import Vector

def resolve_out():
    if "--" in sys.argv:
        a = sys.argv[sys.argv.index("--") + 1:]
        if a:
            return os.path.abspath(a[0])
    sd = os.path.dirname(os.path.abspath(sys.argv[sys.argv.index("--python") + 1])) \
        if "--python" in sys.argv else os.getcwd()
    return os.path.abspath(os.path.join(sd, "..", "assets", "models"))

OUT = os.path.join(resolve_out(), "weapons")
SRC = os.path.abspath(os.path.join(resolve_out(), "..", "blend"))
os.makedirs(OUT, exist_ok=True)
os.makedirs(SRC, exist_ok=True)
print("WEAPON_OUT_DIR:", OUT)

# ---------------------------------------------------------------- helpers
def clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for m in list(bpy.data.meshes):
        if m.users == 0:
            bpy.data.meshes.remove(m)

def set_active(o):
    bpy.context.view_layer.objects.active = o
    o.select_set(True)

def mat(name, color, rough=0.5, metal=0.0):
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

def cube(size, loc, name="c", roty=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.active_object; o.name = name
    o.scale = (size[0], size[1], size[2])
    if roty:
        o.rotation_euler = (0, math.radians(roty), 0)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    return o

def cyl(radius, depth, loc, name="y", axis='X', verts=24):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=depth, location=loc, vertices=verts)
    o = bpy.context.active_object; o.name = name
    if axis == 'X':
        o.rotation_euler = (0, math.radians(90), 0)
    elif axis == 'Y':
        o.rotation_euler = (math.radians(90), 0, 0)
    bpy.ops.object.transform_apply(rotation=True)
    return o

def bevel(o, w=0.004, seg=1):
    set_active(o)
    m = o.modifiers.new("bev", 'BEVEL'); m.width = w; m.segments = seg
    m.limit_method = 'ANGLE'
    bpy.ops.object.modifier_apply(modifier=m.name)

def join(objs, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    set_active(objs[0]); bpy.ops.object.join()
    objs[0].name = name
    return objs[0]

def export(obj, name):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True); set_active(obj)
    try:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(SRC, name + ".blend"))
    except Exception as e:
        print("blend save warn:", e)
    path = os.path.join(OUT, name + ".glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB',
                              use_selection=True, export_apply=True, export_yup=True)
    print("EXPORTED:", path)

# ---------------------------------------------------------------- SMG
def build_smg(accent_rgb, name):
    clear()
    DARK  = mat("smg_dark",  (0.045, 0.05, 0.06), 0.52, 0.25)   # polymer / lower receiver
    GREY  = mat("smg_grey",  (0.60, 0.63, 0.66), 0.45, 0.15)    # light-grey upper panel
    STEEL = mat("smg_steel", (0.10, 0.11, 0.13), 0.33, 0.90)    # barrel / muzzle
    GLASS = mat("smg_glass", (0.02, 0.06, 0.09), 0.12, 0.10)    # optic lens
    ACC   = mat("smg_accent", accent_rgb, 0.50, 0.0)            # the single ACCENT slot

    parts = []
    def add(o, m):
        assign(o, m); parts.append(o); return o

    # --- main body: silver upper DOMINANT + dark lower strip (two-tone like the ref) ---
    add(cube((0.34, 0.058, 0.045), (0.02, 0, -0.020), "recv_lo"), DARK)     # dark lower strip
    add(cube((0.33, 0.064, 0.066), (0.00, 0, 0.030), "recv_up"), GREY)      # big silver receiver
    add(cube((0.085, 0.066, 0.068), (0.150, 0, 0.030), "recv_cut"), DARK)   # dark front section (two-tone break)
    add(cube((0.05, 0.066, 0.030), (-0.090, 0, 0.034), "ejport"), DARK)     # ejection port / rear dark

    # --- short grey handguard forward + M-LOK slots (dark recesses) ----
    add(cube((0.12, 0.056, 0.052), (0.235, 0, 0.000), "handguard"), GREY)
    for sx in (0.205, 0.240, 0.275):
        for sy in (0.0295, -0.0295):
            add(cube((0.022, 0.004, 0.026), (sx, sy, 0.000), "mlok_s"), DARK)   # side slots
        add(cube((0.022, 0.044, 0.005), (sx, 0, 0.027), "mlok_t"), DARK)        # top slot

    # --- short barrel + muzzle brake (front) ---------------------------
    add(cyl(0.011, 0.07, (0.315, 0, 0.012), "barrel", axis='X'), STEEL)
    add(cube((0.045, 0.030, 0.030), (0.350, 0, 0.012), "muzzle"), STEEL)

    # --- top rail (on silver) + teeth ----------------------------------
    add(cube((0.17, 0.018, 0.010), (0.04, 0, 0.055), "rail"), DARK)
    x = -0.03
    while x < 0.12:
        add(cube((0.007, 0.019, 0.013), (x, 0, 0.057), "tooth"), DARK)
        x += 0.016

    # --- red-dot optic: round tube sight on a mount (dark body, glass lens, accent top) ---
    add(cube((0.034, 0.024, 0.020), (0.000, 0, 0.068), "optic_mount"), DARK)      # mount block
    add(cyl(0.020, 0.062, (0.004, 0, 0.094), "optic_tube", axis='X', verts=20), DARK)  # round housing
    add(cyl(0.0205, 0.007, (0.036, 0, 0.094), "optic_lensF", axis='X', verts=20), GLASS)# front lens
    add(cyl(0.0205, 0.007, (-0.028, 0, 0.094), "optic_lensR", axis='X', verts=20), DARK)# rear
    add(cube((0.034, 0.008, 0.008), (0.004, 0, 0.116), "optic_acc"), ACC)         # accent top strip

    # --- angled pistol grip + trigger guard ----------------------------
    add(cube((0.040, 0.046, 0.125), (-0.045, 0, -0.072), "grip", roty=18), DARK)
    add(cube((0.070, 0.044, 0.010), (0.000, 0, -0.035), "guard_bot"), DARK)
    add(cube((0.010, 0.044, 0.028), (0.032, 0, -0.030), "guard_fr"), DARK)

    # --- chunky angled magazine + teal baseplate -----------------------
    add(cube((0.055, 0.048, 0.150), (0.055, 0, -0.088), "mag", roty=-8), DARK)
    add(cube((0.060, 0.050, 0.024), (0.070, 0, -0.160), "mag_base"), ACC)

    # --- triangular skeleton stock (rear) ------------------------------
    add(cube((0.165, 0.020, 0.016), (-0.215, 0, 0.044), "stock_top"), DARK)
    add(cube((0.180, 0.020, 0.016), (-0.205, 0, -0.004), "stock_diag", roty=17), DARK)
    add(cube((0.018, 0.046, 0.095), (-0.295, 0, 0.022), "butt"), ACC)       # teal butt pad

    # --- teal accent dashes on the silver receiver side ----------------
    for sy in (0.031, -0.031):
        add(cube((0.050, 0.004, 0.013), (0.060, sy, 0.018), "dash1", roty=34), ACC)
        add(cube((0.032, 0.004, 0.011), (0.097, sy, 0.028), "dash2", roty=34), ACC)

    for o in parts:
        if o.name.startswith(("recv", "handguard", "grip", "mag", "butt", "optic_base", "muzzle")):
            bevel(o, 0.004, 1)

    gun = join(parts, name)
    # sit the origin at the grip base for easy placement in Godot
    export(gun, name)

def main():
    build_smg((0.015, 0.52, 0.50), "smg_teal")     # teal operative
    build_smg((0.92, 0.42, 0.06), "smg_orange")    # orange operative
    print("WEAPONS DONE")

if __name__ == "__main__":
    main()
