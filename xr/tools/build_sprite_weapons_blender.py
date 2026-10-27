"""Run in system Blender (via Blender MCP or --python) after prepare script.

Creates its own scene. TKG meshes are hidden modelling references only. Exported
models are new low-poly reconstructions with original AB3D I sprite textures.
"""
from pathlib import Path
import json
import math
import struct
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'xr/assets/weapons'
DATA=json.loads((OUT/'reconstruction.json').read_text())
SCENE_NAME='AB3DI Sprite Weapon Reconstructions'
if bpy.data.scenes.get(SCENE_NAME):
    raise RuntimeError('Reconstruction scene already exists; keep edits and use a fresh Blender for regeneration')
scene=bpy.data.scenes.new(SCENE_NAME)
bpy.context.window.scene=scene
scene.unit_settings.scale_length=1
scene.render.fps=50
scene.render.resolution_x=1600
scene.render.resolution_y=1000
scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('AB3DI Studio World')
scene.world.use_nodes=True
background=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
background.inputs[0].default_value=(.08,.095,.12,1)
background.inputs[1].default_value=.5
models=bpy.data.collections.new('Reconstructed weapons - export')
references=bpy.data.collections.new('TKG original geometry - reference only')
scene.collection.children.link(models);scene.collection.children.link(references)
references.hide_render=True
roots=[]
report=[]


def empty(name,parent=None,collection=models):
    o=bpy.data.objects.new(name,None);collection.objects.link(o)
    o.parent=parent
    return o


def material(name,path,emission=0,alpha=False):
    mat=bpy.data.materials.new(name);mat.use_nodes=True
    shader=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    image=bpy.data.images.load(str(path),check_existing=True);image.pack()
    tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
    choices=[i.identifier for i in tex.bl_rna.properties['interpolation'].enum_items]
    tex.interpolation=next(x for x in choices if x.lower()=='closest')
    mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
    shader.inputs['Roughness'].default_value=.65
    shader.inputs['Metallic'].default_value=.12
    mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Emission Color'])
    shader.inputs['Emission Strength'].default_value=emission
    if alpha:
        mat.node_tree.links.new(tex.outputs['Alpha'],shader.inputs['Alpha'])
        mat.surface_render_method=next(i.identifier for i in mat.bl_rna.properties['surface_render_method'].enum_items if i.identifier=='BLENDED')
    return mat


def mesh(name,verts,faces,parent,mat,swatch=None,item=None):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);models.objects.link(obj);obj.parent=parent
    data.materials.append(mat)
    uv=data.uv_layers.new(name='Original sprite projection')
    if item:lo,top,right,bottom=item['sprite_bounds'];length=item['size_m'][1];width=item['size_m'][0]
    for poly in data.polygons:
        for li in poly.loop_indices:
            if swatch is not None:u,v=(swatch*16+8)/384,1-66/74
            elif item:
                x,y,z=data.vertices[data.loops[li].vertex_index].co
                px=(lo+right)/2+x/max(width,.01)*(right-lo)*.65
                py=top+(bottom-top)*(.22+.63*(1-max(0,min(1,y/length))))-z*.22/max(width,.01)*(bottom-top)
                px=max(lo+.5,min(right-1.5,px));py=max(top+.5,min(bottom-1.5,py))
                u,v=(px+.5)/384,1-(py+.5)/74
            else:u,v=0,0
            uv.data[li].uv=(u,v)
    return obj


def profile(name,sections,parent,mat,item,swatch=None):
    verts=[]
    # Octagonal chamfered rectangular cross sections along weapon forward +Y.
    shape=[(-1,-.65),(-.65,-1),(.65,-1),(1,-.65),(1,.65),(.65,1),(-.65,1),(-1,.65)]
    for y,w,h,z in sections:
        verts += [(sx*w/2,y,z+sz*h/2) for sx,sz in shape]
    faces=[tuple(reversed(range(8)))]
    for j in range(len(sections)-1):
        for i in range(8):faces.append((j*8+i,j*8+(i+1)%8,(j+1)*8+(i+1)%8,(j+1)*8+i))
    faces.append(tuple((len(sections)-1)*8+i for i in range(8)))
    return mesh(name,verts,faces,parent,mat,swatch,item)


def box(name,loc,size,parent,mat,item,swatch=None):
    x,y,z=loc;w,l,h=size
    o=profile(name,[(y-l/2,w,h,z),(y+l/2,w,h,z)],parent,mat,item,swatch)
    for v in o.data.vertices:v.co.x+=x
    return o


def barrel(name,sections,x,z,parent,mat,item,swatch=None,sides=12):
    verts=[(x+math.cos(i*math.tau/sides)*r,y,z+math.sin(i*math.tau/sides)*r) for y,r in sections for i in range(sides)]
    faces=[tuple(reversed(range(sides)))]
    for k in range(len(sections)-1):
        for j in range(sides):faces.append((k*sides+j,k*sides+(j+1)%sides,(k+1)*sides+(j+1)%sides,(k+1)*sides+j))
    faces.append(tuple((len(sections)-1)*sides+j for j in range(sides)))
    return mesh(name,verts,faces,parent,mat,swatch,item)


def tube(name,points,r,parent,mat,item,swatch=None):
    verts=[];sides=8
    for i,p in enumerate(points):
        tangent=Vector(points[min(i+1,len(points)-1)])-Vector(points[max(i-1,0)])
        q=Vector((0,1,0)).rotation_difference(tangent.normalized())
        verts.extend(tuple(Vector(p)+q@Vector((math.cos(k*math.tau/sides)*r,0,math.sin(k*math.tau/sides)*r))) for k in range(sides))
    faces=[tuple(reversed(range(sides)))]
    for j in range(len(points)-1):
        for k in range(sides):faces.append((j*sides+k,j*sides+(k+1)%sides,(j+1)*sides+(k+1)%sides,(j+1)*sides+k))
    faces.append(tuple((len(points)-1)*sides+i for i in range(sides)))
    return mesh(name,verts,faces,parent,mat,swatch,item)


def grip(root,mat,item,y=-.01):
    profile('Grip',[(y-.05,.065,.11,-.035),(y+.01,.065,.15,-.02)],root,mat,item,0)
    tube('Trigger guard',[(0,y+.02,.055),(0,y+.09,.025),(0,y+.09,-.025),(0,y+.01,-.045)],.007,root,mat,item,1)
    box('Trigger',(0,y+.045,.009),(.012,.012,.038),root,mat,item,2)


def flash_plane(item,frame,root,muzzle):
    path=OUT/'textures'/f'{item["name"]}-flash-{frame}.png'
    im=bpy.data.images.load(str(path),check_existing=True)
    pixels=list(im.pixels);w,h=map(int,im.size)
    points=[(i%w,i//w) for i in range(w*h) if pixels[i*4+3]>.5]
    if not points:return None
    lo=min(x for x,y in points);right=max(x for x,y in points)+1
    bottom=min(y for x,y in points);top=max(y for x,y in points)+1
    mat=material(f'{item["name"]} original flash {frame}',path,1,True)
    width=min(.26,max(.06,(right-lo)/(top-bottom)*.15));height=.15
    y=muzzle[1]+.004;z=muzzle[2]
    # Two crossed planes give the original flash volume from oblique XR views.
    verts=[(-width/2,y,z-height*.3),(width/2,y,z-height*.3),(width/2,y,z+height*.7),(-width/2,y,z+height*.7),
           (0,y-width/2,z-height*.3),(0,y+width/2,z-height*.3),(0,y+width/2,z+height*.7),(0,y-width/2,z+height*.7)]
    o=mesh(f'Muzzle flash sprite {frame}',verts,[(0,1,2,3),(4,5,6,7)],root,mat)
    uvs=[(lo/w,bottom/h),(right/w,bottom/h),(right/w,top/h),(lo/w,top/h)]*2
    for uv,v in zip(o.data.uv_layers.active.data,uvs):uv.uv=v
    return o


def reconstruct(item):
    name=item['name'];root=empty(name);root['weapon_slot']=item['slot'];root['source']='AB3D I sprite reconstruction; TKG shape references where available'
    root['forward']='Blender +Y; glTF -Z';root['grip_origin']='metres, at root';root['approximation']='Hidden surfaces inferred, not recovered from a single sprite view'
    mat=material(name+' original sprite atlas',OUT/'textures'/f'{name}-sprite-atlas.png',.18)
    glow=material(name+' sprite accent',OUT/'textures'/f'{name}-sprite-atlas.png',.65)
    pump=None
    if name=='pistol':
        profile('Receiver',[(-.07,.09,.085,.075),(.08,.11,.10,.08),(.17,.075,.075,.085)],root,mat,item)
        barrel('Barrel',[(.07,.027),(.28,.024),(.31,.034),(.34,.034),(.34,.018),(.32,.018)],0,.10,root,mat,item,2)
        barrel('Bore',[(.322,.017),(.323,.017)],0,.10,root,mat,item,0)
        box('Green top panel',(0,.04,.137),(.07,.12,.014),root,glow,item,3)
        box('Rear sight',(0,-.04,.15),(.065,.014,.012),root,mat,item,0)
        box('Front sight',(0,.30,.14),(.014,.02,.025),root,mat,item,2);grip(root,mat,item)
        muzzle=(0,.342,.10)
    elif name=='plasma':
        profile('Receiver',[(-.11,.15,.10,.075),(.05,.18,.12,.09),(.20,.105,.10,.10)],root,mat,item)
        barrel('Emitter',[(.05,.032),(.40,.023),(.45,.045),(.48,.04)],0,.11,root,mat,item,2)
        barrel('Blue energy core',[(.14,.025),(.40,.025)],0,.11,root,glow,item,3)
        barrel('Emitter face',[(.481,.027),(.482,.027)],0,.11,root,glow,item,3)
        for side in (-1,1):
            tube('Side prong',[(side*.07,.04,.12),(side*.095,.20,.14),(side*.09,.36,.16),(side*.05,.46,.14)],.014,root,mat,item,2)
            for y in (.13,.23,.33):barrel('Prong collar',[(y,.023),(y+.02,.023)],side*.09,.14,root,mat,item,0)
        grip(root,mat,item);muzzle=(0,.482,.11)
    elif name=='rocket':
        barrel('Launcher tube',[(-.15,.10),(-.10,.12),(.02,.112),(.47,.10),(.62,.115),(.68,.11),(.68,.075),(.63,.075)],0,.12,root,mat,item)
        barrel('Dark bore',[(.631,.074),(.632,.074)],0,.12,root,mat,item,0)
        for y in (.02,.18,.41):barrel('Tube band',[(y,.115),(y+.025,.115)],0,.12,root,mat,item,3)
        box('Sight',(0,.10,.255),(.055,.20,.035),root,mat,item,0)
        box('Green sight display',(0,.04,.278),(.035,.055,.008),root,glow,item,3)
        grip(root,mat,item,.04);box('Forward grip',(0,.40,-.025),(.055,.07,.16),root,mat,item,0);muzzle=(0,.682,.12)
    elif name=='flamethrower':
        profile('Fuel manifold',[(-.08,.13,.10,.08),(.14,.15,.13,.09),(.25,.08,.08,.10)],root,mat,item)
        for side in (-1,1):
            barrel('Copper fuel chamber',[(-.06,.026),(-.02,.038),(.18,.038),(.22,.025)],side*.056,.075,root,mat,item,3)
            tube('Fuel hose',[(side*.07,.08,.04),(side*.07,.16,-.04),(side*.04,.31,-.02),(side*.015,.36,.08)],.009,root,mat,item,0)
        barrel('Nozzle',[(.16,.03),(.39,.02),(.43,.034),(.48,.026),(.48,.014),(.46,.014)],0,.12,root,mat,item,2)
        barrel('Pilot flame tip',[(.43,.004),(.49,.004)],.02,.12,root,glow,item,3)
        grip(root,mat,item);muzzle=(0,.482,.12)
    elif name=='grenade':
        profile('Breech',[(-.10,.10,.095,.08),(.07,.13,.12,.09),(.20,.10,.10,.10)],root,mat,item)
        barrel('Grenade barrel',[(.10,.057),(.43,.05),(.50,.065),(.53,.062),(.53,.042),(.49,.042)],0,.12,root,mat,item,2)
        barrel('Bore',[(.491,.041),(.492,.041)],0,.12,root,mat,item,0)
        pump=barrel('Sliding foregrip',[(.17,.06),(.33,.06)],0,.12,root,mat,item,1)
        for y in (.19,.23,.27,.31):barrel('Foregrip ridge',[(y,.064),(y+.01,.064)],0,.12,pump,mat,item,0)
        box('Top sight',(0,.32,.19),(.015,.16,.014),root,mat,item,1);grip(root,mat,item);muzzle=(0,.532,.12)
    else:
        profile('Shotgun receiver',[(-.10,.085,.09,.075),(.08,.085,.095,.08),(.17,.07,.075,.09)],root,mat,item)
        for x in (-.026,.026):
            barrel('Twin barrel',[(.08,.021),(.78,.021),(.82,.024),(.82,.015),(.79,.015)],x,.12,root,mat,item,2)
            barrel('Bore',[(.791,.014),(.792,.014)],x,.12,root,mat,item,0)
        profile('Stock',[(-.24,.07,.105,.015),(-.10,.07,.08,.05)],root,mat,item,3)
        pump=profile('Pump',[(.28,.085,.06,.08),(.43,.085,.06,.08)],root,mat,item,3)
        for y in (.30,.33,.36,.39):box('Pump groove',(0,y,.048),(.088,.008,.009),pump,mat,item,0)
        box('Rib sight',(0,.53,.147),(.01,.54,.008),root,mat,item,1);grip(root,mat,item);muzzle=(0,.822,.12)
    marker=empty('muzzle',root);marker.location=muzzle
    marker['purpose']='Shot origin and forward direction; +Y before glTF axis conversion'
    grip_marker=empty('grip',root);grip_marker['socket']='grip';marker['socket']='muzzle'
    flashes={f:flash_plane(item,f,root,muzzle) for f in (2,3)}
    motion=empty('motion_reference',root);motion['purpose']='Sprite measurements retained in reconstruction.json'
    # Core glTF has no animated texture-index property. Four UV variants use
    # standard STEP scale tracks instead, preserving each original sprite's
    # appearance without requiring a custom shader or extension.
    body=[o for o in list(root.children) if o.type=='MESH' and o not in flashes.values()]
    variants=[];pumps=[]
    original_children={o:list(o.children) for o in root.children_recursive}
    for frame in range(4):
        variant=empty('sprite_frame_'+str(frame),root);variants.append(variant)
        lo,top,right,bottom=item['sprite_bounds'];fl,ft,fr,fb=item['sprite_bounds_per_frame'][frame]
        def duplicate(original,parent):
            if frame==0:
                obj=original;obj.parent=parent
            else:
                obj=original.copy();obj.data=original.data.copy();models.objects.link(obj);obj.parent=parent
                if obj.type=='MESH':
                    for uv in obj.data.uv_layers.active.data:
                        px=uv.uv.x*384;py=(1-uv.uv.y)*74
                        if py<58:
                            px=fl+(px-lo)/(right-lo)*(fr-fl)
                            py=ft+(py-top)/(bottom-top)*(fb-ft)
                        uv.uv=((px+frame*96)/384,1-py/74)
            if original==pump:pumps.append(obj)
            for child in original_children.get(original,[]):duplicate(child,obj)
            return obj
        for original in body:duplicate(original,variant)
    # Batch static pieces per sprite state; keep the sliding foregrip separate.
    def join_parts(parts,active):
        bpy.ops.object.select_all(action='DESELECT')
        for part in parts:part.select_set(True)
        bpy.context.view_layer.objects.active=active
        if len(parts)>1:bpy.ops.object.join()
    for variant in variants:
        static=[o for o in variant.children if o.type=='MESH' and o not in pumps]
        if static:join_parts(static,static[0]);static[0].name='Body sprite variant'
    for moving in pumps:
        pieces=[moving]+[o for o in moving.children_recursive if o.type=='MESH']
        join_parts(pieces,moving)
    return root,pumps,flashes,variants


def key(obj,path,frame):obj.keyframe_insert(data_path=path,frame=frame,group='Fire')


def finalize_glb(path):
    # Blender's sampled SCENE exporter uses LINEAR for object scale. Sprite
    # visibility must change on exact game ticks. Sample nonsingular parents
    # in Blender, then make the resulting glTF visibility exactly zero/one.
    raw=path.read_bytes();json_size=struct.unpack_from('<I',raw,12)[0]
    document=json.loads(raw[20:20+json_size]);bin_at=20+json_size
    binary=bytearray(raw[bin_at+8:])
    for node in document['nodes']:
        if node.get('extras',{}).get('socket'):node['name']=node['extras']['socket']
        if node.get('name','').startswith('sprite_frame_') and 'scale' in node:
            node['scale']=[0 if abs(v-.0001)<1e-7 else (1 if abs(v-1)<1e-6 else v) for v in node['scale']]
    for animation in document.get('animations',[]):
        for channel in animation['channels']:
            if channel['target']['path']!='scale':continue
            sampler=animation['samplers'][channel['sampler']];sampler['interpolation']='STEP'
            if not document['nodes'][channel['target']['node']].get('name','').startswith('sprite_frame_'):continue
            accessor=document['accessors'][sampler['output']]
            view=document['bufferViews'][accessor['bufferView']]
            offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)
            converted=[]
            for i in range(accessor['count']):
                at=offset+i*view.get('byteStride',12)
                value=struct.unpack_from('<fff',binary,at)
                value=[0 if abs(v-.0001)<1e-7 else (1 if abs(v-1)<1e-6 else v) for v in value]
                converted.append(value)
                struct.pack_into('<fff',binary,at,*value)
            accessor['min']=[min(v[k] for v in converted) for k in range(3)]
            accessor['max']=[max(v[k] for v in converted) for k in range(3)]
    document['asset']['copyright']='Original sprites/designs: Team17 and original authors; reconstructed mesh prototypes: de-xr'
    encoded=json.dumps(document,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
    contents=struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary
    path.write_bytes(struct.pack('<III',0x46546c67,2,12+len(contents))+contents)


# Build all models and hidden source references before exporting each hierarchy.
for item in DATA['weapons']:
    root,pumps,flashes,variants=reconstruct(item);roots.append(root)
    reference=item.get('tkg_reference')
    if reference:
        verts=reference['vertices'];mins=[min(v[k] for v in verts) for k in range(3)];maxs=[max(v[k] for v in verts) for k in range(3)]
        axis=0 if item['name'] in ('pistol','shotgun') else 2
        scale=item['size_m'][1]/(maxs[axis]-mins[axis])
        converted=[]
        for v in verts:
            across=v[2] if axis==0 else v[0]
            converted.append((across*scale,(-v[axis]+maxs[axis])*scale,-v[1]*scale))
        md=bpy.data.meshes.new('Original TKG '+reference['name']);md.from_pydata(converted,[],reference['faces']);md.update()
        obj=bpy.data.objects.new('REFERENCE TKG '+reference['name'],md);references.objects.link(obj);obj.hide_set(True)
        obj['original_asset_sha256']=reference['sha256'];obj['purpose']='Hidden shape/proportion reference, not exported'
    # Exact countdown sequence from the engine, sampled at its 50 Hz tick rate.
    scene.frame_start=0;scene.frame_end=len(item['sequence'])-1
    base=item['poses'][0];pose=item['poses']
    for frame,sprite in enumerate(item['sequence']):
        delta_x=(pose[sprite]['center'][0]-base['center'][0])/96
        delta_y=(pose[sprite]['center'][1]-base['center'][1])/58
        roll=max(-.5,min(.5,pose[sprite]['axis_angle']-base['axis_angle']))
        root.location=(delta_x*.22, -abs(delta_y)*.08-(.018 if sprite==3 else 0),-delta_y*.12)
        root.rotation_mode='XYZ';root.rotation_euler=(delta_y*.35,roll*.65,0)
        key(root,'location',frame);key(root,'rotation_euler',frame)
        for pump in pumps:
            pump.location.y=-.065 if sprite==1 else (-.025 if sprite==2 else 0)
            key(pump,'location',frame)
        for which,variant in enumerate(variants):
            variant.scale=(1,1,1) if sprite==which else (.0001,.0001,.0001)
            key(variant,'scale',frame)
        for which,obj in flashes.items():
            if obj:
                obj.scale=(1,1,1) if sprite==which else (0,0,0)
                key(obj,'scale',frame)
    # Discrete sprite appearance changes use standard glTF STEP channels.
    for obj in [*variants,*[o for o in flashes.values() if o]]:
        action=obj.animation_data.action
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for fc in bag.fcurves:
                        for point in fc.keyframe_points:point.interpolation='CONSTANT'
    # Keep the final neutral pose as the glTF node defaults.
    scene.frame_set(scene.frame_end)
    bpy.ops.object.select_all(action='DESELECT')
    selection=[root]+list(root.children_recursive)
    for obj in selection:obj.select_set(True)
    bpy.context.view_layer.objects.active=root
    import io_scene_gltf2
    formats=io_scene_gltf2.get_format_items(None,bpy.context)
    glb=next(x[0] for x in formats if x[0].lower()=='glb')
    scene.name='Fire'
    bpy.ops.export_scene.gltf(filepath=str(OUT/f'{item["name"]}.glb'),export_format=glb,use_selection=True,
        export_animations=True,export_animation_mode='SCENE',export_anim_scene_split_object=False,
        export_force_sampling=True,export_extras=True,export_yup=True,export_current_frame=True)
    finalize_glb(OUT/f'{item["name"]}.glb')
    scene.name=SCENE_NAME
    objects=[o for o in selection if o.type=='MESH']
    triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objects)
    visible_triangles=max(sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in v.children_recursive if o.type=='MESH') for v in variants)+4
    report.append(dict(name=item['name'],slot=item['slot'],mesh_objects=len(objects),triangles_all_variants=triangles,visible_triangles_max=visible_triangles,
        duration_s=item['duration_s'],reference=reference['name'] if reference else None))
    # Remove source-only layout animation from viewport placement by using an
    # additional unanimated display parent, outside exported hierarchies.
    display=empty(item['name']+' display placement');root.parent=display
    index=len(roots)-1;display.location=((index%3)*1.05,(index//3)*1.15,0)

scene.frame_start=0;scene.frame_end=63;scene.frame_set(63)
# Native Blender studio preview, isolated from game asset exports.
rig=bpy.data.collections.new('Studio - not exported');scene.collection.children.link(rig)
camera_data=bpy.data.cameras.new('Weapon overview camera');camera=bpy.data.objects.new('Weapon overview camera',camera_data);rig.objects.link(camera)
camera.location=(2.7,-2.4,5.7);target=Vector((1.0,.8,.05));camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
camera_data.type=next(i.identifier for i in camera_data.bl_rna.properties['type'].enum_items if i.identifier=='ORTHO');camera_data.ortho_scale=5.1
scene.camera=camera
for name,location,power,size in [('Key',(.5,-1.2,3.8),250,3.5),('Fill',(3,2,2.8),180,3),('Rim',(-1,2,2.5),200,2)]:
    ld=bpy.data.lights.new(name,'AREA');ld.energy=power;ld.shape='DISK';ld.size=size
    o=bpy.data.objects.new(name,ld);rig.objects.link(o);o.location=location;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
# Human-readable labels stay out of each glTF file.
for i,item in enumerate(DATA['weapons']):
    curve=bpy.data.curves.new(item['name']+' label','FONT');curve.body=item['name'].upper();curve.size=.06
    obj=bpy.data.objects.new(item['name']+' label',curve);rig.objects.link(obj);obj.location=((i%3)*1.05-.25,2.17 if i//3 else -.34,-.13)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_perspective='CAMERA'
        area.spaces.active.shading.type='MATERIAL'
scene.render.filepath=str(OUT/'overview.png')
(OUT/'build-report.json').write_text(json.dumps({'blender':bpy.app.version_string,'weapons':report},indent=2)+'\n')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ab3di-sprite-weapons.blend'),compress=True)
print('Built and exported six sprite-textured animated weapon reconstructions',report)
