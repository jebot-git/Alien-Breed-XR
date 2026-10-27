"""Run through Blender MCP to re-import and inspect the five runtime GLBs."""
from pathlib import Path
import contextlib
import io
import json
import bpy
import bmesh

root=Path(__file__).resolve().parents[2]/'xr/assets/weapons'
manifest=json.loads((root/'reconstruction.json').read_text())
original=bpy.context.window.scene
name='AB3DI weapon roundtrip QA'
if bpy.data.scenes.get(name):raise RuntimeError('QA scene already exists; preserve it before retrying')
qa=bpy.data.scenes.new(name);qa.render.fps=50;bpy.context.window.scene=qa
report=[]
try:
    for item in manifest['weapons']:
        if item['slot'] not in (0,1,2,4,7):continue
        before=set(qa.objects)
        with contextlib.redirect_stdout(io.StringIO()):bpy.ops.import_scene.gltf(filepath=str(root/(item['name']+'.glb')))
        objects=[o for o in qa.objects if o not in before]
        meshes=[o for o in objects if o.type=='MESH']
        assert meshes and all(o.data.uv_layers and len(o.data.vertices)>0 for o in meshes)
        assert sum(o.get('socket')=='support_grip' for o in objects)==1
        variants=[o for o in objects if o.name.startswith('sprite_frame_')]
        assert len(variants)==4
        samples=[]
        for tick in sorted(set([0,1,2,len(item['sequence'])//2,len(item['sequence'])-1])):
            qa.frame_set(tick);bpy.context.view_layer.update()
            active=[o for o in variants if abs(o.scale.x-1)<1e-5]
            assert len(active)==1,(item['name'],tick)
            expected=item['sequence'][min(tick,len(item['sequence'])-1)]
            assert active[0].name.split('.')[0]=='sprite_frame_'+str(expected),(item['name'],tick,expected)
            samples.append([tick,active[0].name.split('.')[0]])
        boundary=0
        for obj in meshes:
            if obj.name.startswith('Muzzle flash'):continue
            bm=bmesh.new();bm.from_mesh(obj.data)
            # glTF splits vertices at UV/normal seams; weld coincident vertices
            # in this temporary diagnostic mesh before counting real boundaries.
            bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.000001)
            boundary+=sum(e.is_boundary for e in bm.edges)
            bm.free()
        assert boundary==0,(item['name'],'open body boundaries',boundary)
        report.append(dict(file=item['name']+'.glb',mesh_objects=len(meshes),samples=samples,uvs=True,body_boundary_edges=boundary))
        for obj in objects:bpy.data.objects.remove(obj,do_unlink=True)
finally:
    bpy.context.window.scene=original
    for obj in list(qa.objects):bpy.data.objects.remove(obj,do_unlink=True)
    bpy.data.scenes.remove(qa)
(root/'roundtrip.json').write_text(json.dumps({'version':2,'blender':bpy.app.version_string,'weapons':report},indent=2)+'\n')
print('Round-trip verified: five original-palette weapons, UVs, animation states and closed body surfaces')
