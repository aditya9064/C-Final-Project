"""Assemble a rigged, animated character GLB from a Quaternius base + UAL animation GLB.
Both share the identical 65-bone skeleton, so actions transfer by bone name (no retarget).
Args after -- : base_gltf  ual_glb  out_glb
Keeps a mapped subset of clips with clean names, exports one GLB (mesh + skeleton + animations)."""
import bpy, sys, os

args = sys.argv[sys.argv.index("--") + 1:]
BASE, UAL, OUT = args[0], args[1], args[2]

MAP = {
    'Idle_Loop': 'idle', 'Walk_Loop': 'walk', 'Sprint_Loop': 'run', 'Jog_Fwd_Loop': 'jog',
    'Jump_Start': 'jump_start', 'Jump_Loop': 'jump_loop', 'Jump_Land': 'jump_land',
    'Death01': 'death', 'Pistol_Aim_Neutral': 'aim', 'Pistol_Shoot': 'fire',
    'Pistol_Reload': 'reload', 'Pistol_Idle_Loop': 'aim_idle',
}

# clean slate
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
for coll in (bpy.data.actions, bpy.data.meshes, bpy.data.armatures):
    for b in list(coll):
        if b.users == 0: coll.remove(b)

# import base character
bpy.ops.import_scene.gltf(filepath=BASE)
base_arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
base_meshes = [o for o in bpy.data.objects if o.type == 'MESH']
before_actions = set(bpy.data.actions)

# import animation library (adds its own armature+mesh+actions)
bpy.ops.import_scene.gltf(filepath=UAL)
ual_arm = [o for o in bpy.data.objects if o.type == 'ARMATURE' and o is not base_arm]
ual_mesh = [o for o in bpy.data.objects if o.type == 'MESH' and o not in base_meshes]

# rename kept actions by PREFIX match (importer appends _Armature/.001 suffixes), delete the rest
print("IMPORTED ACTIONS:", [a.name for a in bpy.data.actions])
kept = {}
for key, clean in MAP.items():
    match = next((a for a in bpy.data.actions if a.name.startswith(key)), None)
    if match:
        match.name = clean; match.use_fake_user = True; kept[clean] = match
for a in list(bpy.data.actions):
    if a not in kept.values():
        bpy.data.actions.remove(a)

# drop the UAL armature + mesh (keep only the base, actions persist via fake user)
for o in ual_mesh + ual_arm:
    bpy.data.objects.remove(o, do_unlink=True)

# push each kept action onto the base armature as its own NLA track (deterministic export)
base_arm.animation_data_create()
ad = base_arm.animation_data
ad.action = None
for name, act in kept.items():
    tr = ad.nla_tracks.new(); tr.name = name
    st = tr.strips.new(name, int(act.frame_range[0]), act); st.name = name

# export base + skeleton + all NLA tracks as one GLB
bpy.ops.object.select_all(action='DESELECT')
for o in base_meshes + [base_arm]:
    o.select_set(True)
bpy.context.view_layer.objects.active = base_arm
bpy.ops.export_scene.gltf(
    filepath=OUT, export_format='GLB', use_selection=True, export_apply=False,
    export_animations=True, export_animation_mode='NLA_TRACKS', export_yup=True)
print("ASSEMBLED:", OUT)
print("CLIPS:", sorted(kept.keys()))
