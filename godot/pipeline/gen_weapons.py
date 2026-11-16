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

    # --- receiver (two-tone block) -------------------------------------
    add(cube((0.26, 0.055, 0.052), (0.00, 0, 0.000), "recv_lo"), DARK)
    add(cube((0.215, 0.050, 0.034), (-0.005, 0, 0.043), "recv_up"), GREY)
    # ejection-port detail (small dark inset on grey)
    add(cube((0.06, 0.052, 0.022), (0.05, 0, 0.040), "ejport"), DARK)

    # --- top Picatinny rail + teeth ------------------------------------
    add(cube((0.21, 0.020, 0.010), (0.015, 0, 0.066), "rail"), DARK)
    x = -0.07
    while x < 0.12:
        add(cube((0.008, 0.021, 0.014), (x, 0, 0.068), "tooth"), DARK)
        x += 0.018

    # --- red-dot optic -------------------------------------------------
    add(cube((0.055, 0.030, 0.030), (-0.015, 0, 0.088), "optic_base"), DARK)
    add(cyl(0.021, 0.034, (-0.015, 0, 0.104), "optic_tube", axis='X'), DARK)
    add(cyl(0.016, 0.006, (-0.001, 0, 0.104), "optic_lens", axis='X'), GLASS)

    # --- handguard (front) + M-LOK slots -------------------------------
    add(cube((0.17, 0.050, 0.050), (0.205, 0, -0.002), "handguard"), DARK)
    for sx in (0.16, 0.205, 0.25):
        add(cube((0.028, 0.054, 0.012), (sx, 0, 0.004), "mlok"), GREY)

    # --- barrel + muzzle ----------------------------------------------
    add(cyl(0.012, 0.16, (0.345, 0, 0.012), "barrel", axis='X'), STEEL)
    add(cyl(0.019, 0.055, (0.415, 0, 0.012), "muzzle", axis='X'), STEEL)
    add(cyl(0.020, 0.012, (0.447, 0, 0.012), "muzzle_tip", axis='X'), ACC)

    # --- pistol grip (angled) + trigger guard --------------------------
    add(cube((0.042, 0.048, 0.135), (-0.045, 0, -0.082), "grip", roty=16), DARK)
    add(cube((0.075, 0.044, 0.010), (-0.005, 0, -0.035), "guard_bot"), DARK)
    add(cube((0.010, 0.044, 0.030), (0.030, 0, -0.030), "guard_fr"), DARK)

    # --- magazine (angled fwd) + accent base ---------------------------
    add(cube((0.050, 0.044, 0.150), (0.040, 0, -0.085), "mag", roty=-9), DARK)
    add(cube((0.055, 0.048, 0.022), (0.052, 0, -0.158), "mag_base"), ACC)

    # --- skeleton stock (rear) -----------------------------------------
    add(cube((0.150, 0.020, 0.014), (-0.195, 0, 0.028), "stock_top"), DARK)
    add(cube((0.120, 0.020, 0.014), (-0.180, 0, -0.030), "stock_bot", roty=-12), DARK)
    add(cube((0.018, 0.045, 0.095), (-0.262, 0, 0.000), "butt"), ACC)

    # --- accent side stripes on the receiver ---------------------------
    for sy in (0.028, -0.028):
        add(cube((0.11, 0.006, 0.016), (0.02, sy, 0.012), "stripe"), ACC)

    for o in parts:
        if o.name.startswith(("recv", "handguard", "grip", "mag", "butt", "optic_base")):
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
