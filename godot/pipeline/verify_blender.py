"""Blender self-test: import bpy, enable glTF, build a cube, export GLB + OBJ.
Run: blender --background --factory-startup --python verify_blender.py -- <out_dir>
Prints machine-readable lines the setup script / CI can grep.
"""
import bpy, sys, os, addon_utils

out = os.getcwd()
if "--" in sys.argv:
    a = sys.argv[sys.argv.index("--") + 1:]
    if a:
        out = os.path.abspath(a[0])
os.makedirs(out, exist_ok=True)

print("BLENDER_VERSION:", bpy.app.version_string)
print("BPY_IMPORT: ok")

# clean scene, add a cube
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()
bpy.ops.mesh.primitive_cube_add(size=1)
print("PY_EXEC: ok")

glb_ok = False
try:
    addon_utils.enable("io_scene_gltf2", default_set=True, persistent=True)
    gpath = os.path.join(out, "_selftest.glb")
    bpy.ops.export_scene.gltf(filepath=gpath, export_format='GLB', use_selection=False)
    glb_ok = os.path.exists(gpath) and os.path.getsize(gpath) > 0
    print("GLB_EXPORT:", "ok" if glb_ok else "empty", os.path.getsize(gpath) if os.path.exists(gpath) else 0)
except Exception as e:
    print("GLB_EXPORT: FAILED", repr(e))

try:
    opath = os.path.join(out, "_selftest.obj")
    bpy.ops.wm.obj_export(filepath=opath, forward_axis='NEGATIVE_Z', up_axis='Y')
    print("OBJ_EXPORT:", "ok" if os.path.getsize(opath) > 0 else "empty")
except Exception as e:
    print("OBJ_EXPORT: FAILED", repr(e))

print("SELFTEST_DONE glb=%s" % glb_ok)
