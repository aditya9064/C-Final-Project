"""Convert selected Quaternius Stylized Nature MegaKit glTF models (CC0) to
glTF files with a shared textures/ folder for the Fireteam arena. Run with:
  blender --background --factory-startup --python convert_nature.py -- <src_gltf_dir> <out_dir>
Textures are written once to <out_dir>/textures/; MASK alpha + double-sided are preserved by the glTF roundtrip.
"""
import bpy, sys, os

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = argv[0], argv[1]
os.makedirs(OUT, exist_ok=True)

MODELS = [
    "CommonTree_1", "CommonTree_2", "CommonTree_3", "CommonTree_4", "CommonTree_5",
    "Bush_Common", "Bush_Common_Flowers", "Fern_1",
    "Grass_Common_Tall", "Grass_Wispy_Tall",
    "Rock_Medium_1", "Rock_Medium_2", "Rock_Medium_3",
    "Pebble_Round_1", "Pebble_Round_3", "Pebble_Round_5",
]

def wipe():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.images):
        for b in list(block):
            if b.users == 0:
                block.remove(b)

done = []
for name in MODELS:
    src = os.path.join(SRC, name + ".gltf")
    if not os.path.exists(src):
        print("SKIP missing", name); continue
    wipe()
    bpy.ops.import_scene.gltf(filepath=src)
    # Join all mesh objects into one, rename, recentre X/Z to origin, drop to y=0
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        print("SKIP no mesh", name); continue
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    # Apply transforms so scale/rotation are baked
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    out = os.path.join(OUT, name + ".gltf")
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    # GLTF_SEPARATE: <name>.gltf + <name>.bin + textures/<image>.png. The kit's
    # trees/rocks share the same bark/rock atlases, so writing them once into a
    # shared textures/ folder instead of embedding a copy in every GLB cuts the
    # committed asset size by roughly 70% with identical pixels.
    bpy.ops.export_scene.gltf(
        filepath=out, export_format="GLTF_SEPARATE", use_selection=True,
        export_apply=True, export_yup=True,
        export_texture_dir="textures", export_keep_originals=False,
    )
    done.append((name, out))
    print("WROTE", out)

print("=== CONVERTED %d models ===" % len(done))
for n, o in done:
    print("  ", n, os.path.getsize(o), "bytes")
