"""Solid, sprite-informed weapon reconstruction and coherent inferred surfaces.

Imported by the Blender build script. All coordinates are metres, +Y forward.
Only the two flash cutouts are planes; bodies, lips, bores, panels and guards
have actual depth. Original palette surfaces are prepared separately.
"""
import math
from mathutils import Vector, Matrix

TILES=('receiver_side','receiver_top','receiver_end','steel','rubber','vents','accent','dark',
       'barrel','bore','bolts','panel','stock','heatshield','marking','edge')

def surface_uv(data,item,surface):
    """Box project coherent face islands into named pixel-art material tiles."""
    uv=data.uv_layers.active
    mins=[min(v.co[k] for v in data.vertices) for k in range(3)]
    maxs=[max(v.co[k] for v in data.vertices) for k in range(3)]
    for poly in data.polygons:
        axis=max(range(3),key=lambda k:abs(poly.normal[k]))
        region=surface
        if isinstance(surface,list):region=surface[poly.index]
        if region=='receiver':region=('receiver_side','receiver_end','receiver_top')[axis]
        if region is None:region='receiver_side'
        tile=TILES.index(region)
        a,b=((1,2),(0,2),(0,1))[axis]
        for loop in poly.loop_indices:
            p=data.vertices[data.loops[loop].vertex_index].co
            u=(p[a]-mins[a])/max(.00001,maxs[a]-mins[a])
            v=(p[b]-mins[b])/max(.00001,maxs[b]-mins[b])
            # Keep within tile gutters so adjacent materials never bleed.
            uv.data[loop].uv=((tile%4*64+3+u*57)/1024,1-(tile//4*64+3+(1-v)*57)/256)

def reconstruct(item,api):
    import bpy
    empty,profile,box,barrel,tube,mesh,material,flash=(api[n] for n in ('empty','profile','box','barrel','tube','mesh','material','flash_plane'))
    name=item['name'];root=empty(name)
    root['weapon_slot']=item['slot'];root['reconstruction_version']=2
    root['source']='Original AB3D I sprite silhouette, explicit depth sections and original-palette UV surfaces'
    root['inferred']='Unseen side/bottom geometry continues receiver panels, grips, bores and mechanical joints; excludes painted hands'
    root['forward']='Blender +Y; glTF -Z';root['grip_origin']='metres, at root'
    mat=material(name+' palette metal and composite',api['OUT']/'textures'/f'{name}-surfaces.png',.025)
    glow=material(name+' original accent light',api['OUT']/'textures'/f'{name}-surfaces.png',.3)
    sections=[]
    def p(label,values,surface='receiver',parent=root):
        sections.append({'part':label,'cross_sections_y_width_height_center_z':values})
        return profile(label,values,parent,mat,item,surface)
    def b(label,loc,size,surface='receiver_side',parent=root,lit=False):
        return box(label,loc,size,parent,glow if lit else mat,item,surface)
    def c(label,values,x,z,surface='steel',parent=root,sides=16):
        return barrel(label,values,x,z,parent,mat,item,surface,sides)
    def t(label,points,r,surface='steel',parent=root):
        return tube(label,points,r,parent,mat,item,surface)
    def hollow(label,start,end,radius,inner,x,z,parent=root):
        n=20;values=[(start,radius*.92),(start+.02,radius),(end-.014,radius),(end,radius*.96),(end,inner),(end-.05,inner)]
        verts=[(x+math.cos(k*math.tau/n)*r,y,z+math.sin(k*math.tau/n)*r) for y,r in values for k in range(n)]
        faces=[tuple(reversed(range(n)))];regions=['dark']
        for j in range(len(values)-1):
            for k in range(n):
                faces.append((j*n+k,j*n+(k+1)%n,(j+1)*n+(k+1)%n,(j+1)*n+k))
                regions.append('bore' if j==4 else 'edge' if j==3 else 'barrel')
        faces.append(tuple((len(values)-1)*n+k for k in range(n)));regions.append('bore')
        return mesh(label,verts,faces,parent,mat,regions,item)
    def fasteners(y,z,width):
        for side in (-1,1):
            # Recessed side screw heads have real thickness and dark sockets.
            o=c('Receiver side fastener',[(0,.006),(.003,.006)],0,0,'bolts')
            o.rotation_euler[2]=math.pi/2;o.location=(side*width,y,z)
    def handle(y=0):
        p('Angled composite pistol grip',[(y-.048,.055,.145,-.06),(y+.009,.061,.155,-.045)],'rubber')
        b('Grip heel',(0,y-.036,-.134),(.063,.052,.014),'dark')
        for z in (-.025,-.052,-.079,-.106):b('Grip finger rib',(0,y+.014,z),(.059,.009,.008),'rubber')
        t('Open trigger guard',[(0,y+.018,.022),(0,y+.087,.015),(0,y+.092,-.046),(0,y+.021,-.052)],.0055,'dark')
        t('Curved trigger',[(0,y+.037,.023),(0,y+.047,-.011),(0,y+.036,-.024)],.004,'steel')
    moving=None
    if name=='pistol':
        p('Pulse rifle stepped receiver',[(-.115,.088,.087,.08),(-.065,.112,.11,.085),(.09,.112,.11,.085),(.17,.084,.085,.09)])
        p('Squared barrel heat shroud',[(.14,.078,.075,.09),(.275,.071,.064,.095),(.315,.058,.052,.099)],'heatshield')
        hollow('Recessed pulse muzzle',.245,.35,.029,.017,0,.10)
        b('Inset olive upper cover',(0,-.001,.146),(.077,.17,.014),'receiver_top')
        b('Olive status panel',(0,.045,.155),(.043,.057,.004),'accent',lit=True)
        for side in (-1,1):
            b('Receiver recessed side panel',(side*.057,.004,.08),(.007,.108,.047),'panel')
            b('Shroud side vent recess',(side*.039,.226,.096),(.004,.071,.025),'vents')
        p('Short rear brace',[(-.185,.074,.064,.073),(-.115,.078,.063,.08)],'stock')
        b('Butt pad',(0,-.191,.073),(.08,.014,.072),'rubber')
        b('Rear sight base',(0,-.085,.154),(.051,.026,.013),'dark')
        for x in (-.018,.018):b('Rear sight ear',(x,-.085,.168),(.009,.015,.022),'steel')
        b('Front sight post',(0,.287,.14),(.009,.016,.026),'steel')
        moving=b('Reciprocating bolt',(.06,-.014,.105),(.014,.046,.012),'steel');moving['mechanism']='bolt'
        b('Magazine well',(0,.09,.018),(.05,.066,.045),'dark')
        b('Compact magazine',(0,.09,-.035),(.047,.064,.07),'receiver_side')
        handle(-.016);fasteners(.06,.09,.061);muzzle=(0,.352,.10)
    elif name=='plasma':
        p('Machine gun armored receiver',[(-.13,.12,.10,.08),(-.08,.161,.126,.09),(.10,.16,.126,.09),(.19,.105,.09,.103)])
        p('Upper spine',[(.03,.05,.038,.166),(.18,.045,.033,.16)],'dark')
        hollow('Emitter bore and collar',.32,.483,.047,.027,0,.116)
        c('Blue longitudinal core',[(.08,.028),(.39,.028)],0,.116,'accent')
        for side in (-1,1):
            t('Curved machined emitter arm',[(side*.065,.08,.126),(side*.093,.16,.15),(side*.101,.31,.164),(side*.087,.405,.155),(side*.039,.466,.132)],.0125,'steel')
            for y in (.19,.27,.35):c('Arm retaining collar',[(y,.018),(y+.012,.018)],side*.098,.16,'dark')
            b('Blue receiver cheek',(side*.083,.005,.098),(.008,.111,.035),'accent')
            b('Lower cooling recess',(side*.081,.009,.057),(.005,.105,.018),'vents')
        b('Rear power-cell socket',(0,-.137,.083),(.11,.019,.085),'dark')
        b('Power cell',(0,-.121,.145),(.07,.075,.033),'receiver_top')
        b('Inset blue core window',(0,.04,.158),(.038,.09,.006),'accent',lit=True)
        moving=b('Charging slide',(.091,.04,.122),(.016,.055,.015),'steel');moving['mechanism']='bolt'
        handle(-.025);fasteners(.061,.097,.087);muzzle=(0,.485,.116)
    elif name=='rocket':
        c('Olive launcher pressure shell',[(-.17,.082),(-.13,.103),(-.05,.106),(.48,.098),(.59,.105),(.63,.101)],0,.12,'receiver_side',sides=24)
        hollow('Steel muzzle sleeve and deep bore',.575,.682,.109,.076,0,.12)
        hollow('Rear exhaust socket',-.20,-.15,.086,.062,0,.12)
        for y in (-.035,.21,.48):c('Raised olive reinforcement band',[(y,.108),(y+.018,.108)],0,.12,'accent',sides=24)
        for side in (-1,1):
            b('Inset side service plate',(side*.104,.12,.12),(.009,.24,.079),'panel')
            b('Exhaust vent bank',(side*.099,.39,.12),(.006,.105,.045),'vents')
            b('Safety marking',(side*.108,.03,.16),(.005,.064,.03),'marking')
        p('Sight rail',[(0,.039,.024,.236),(.29,.039,.024,.236)],'dark')
        b('Optical sight housing',(0,.08,.258),(.059,.115,.043),'receiver_side')
        b('Green optical inset',(0,.023,.264),(.039,.006,.021),'accent',lit=True)
        b('Front sight hood',(0,.42,.232),(.035,.018,.046),'dark')
        handle(.035);b('Forward composite grip',(0,.39,-.059),(.053,.065,.157),'rubber')
        fasteners(.08,.12,.113);muzzle=(0,.684,.12)
    elif name=='grenade':
        p('Grenade receiver',[(-.13,.092,.09,.074),(-.06,.12,.121,.087),(.13,.12,.121,.087),(.22,.10,.095,.104)])
        hollow('Large grenade barrel and crown',.15,.532,.063,.044,0,.12)
        moving=c('Sliding ribbed fore-end',[(.23,.065),(.35,.065)],0,.12,'rubber');moving['mechanism']='pump'
        for y in (.239,.262,.285,.308,.331):c('Fore-end raised ring',[(y,.068),(y+.008,.068)],0,.12,'dark',moving)
        for side in (-1,1):
            b('Receiver service cover',(side*.063,.037,.092),(.007,.114,.055),'panel')
            b('Breech hinge cover',(side*.063,.15,.095),(.013,.029,.03),'bolts')
        p('Upper breech rail',[(-.04,.025,.017,.154),(.15,.025,.017,.159)],'steel')
        b('Front ladder sight',(0,.42,.195),(.023,.03,.034),'dark')
        b('Rear sight notch',(0,-.05,.168),(.042,.02,.013),'steel')
        p('Rear support',[(-.195,.071,.081,.037),(-.13,.08,.074,.064)],'stock')
        handle(-.02);fasteners(.035,.095,.068);muzzle=(0,.534,.12)
    else:
        p('Double-barrel breech receiver',[(-.115,.08,.084,.078),(-.035,.092,.105,.088),(.137,.084,.093,.10)])
        # One rigid break-action assembly: barrels, rib and fore-end move
        # together around the breech hinge, not as a shotgun pump.
        assembly=[]
        for x in (-.024,.024):assembly.append(hollow('Hollow twin steel barrel',.126,.822,.023,.0155,x,.121))
        assembly.append(b('Twin barrel upper rib',(0,.46,.145),(.011,.64,.009),'steel'))
        assembly.append(b('Barrel lower rib',(0,.46,.098),(.013,.61,.008),'dark'))
        assembly.append(p('Composite barrel fore-end',[(.22,.08,.034,.083),(.40,.075,.035,.083)],'rubber'))
        for y in (.235,.265,.295,.325,.355,.385):assembly.append(b('Fore-end rib',(0,y,.065),(.078,.007,.006),'dark'))
        assembly.append(b('Front bead sight',(0,.78,.157),(.008,.012,.013),'edge'))
        bpy.ops.object.select_all(action='DESELECT')
        for o in assembly:o.select_set(True)
        bpy.context.view_layer.objects.active=assembly[0];bpy.ops.object.join();moving=assembly[0]
        pivot=Vector((0,.126,.10));moving.data.transform(Matrix.Translation(-pivot));moving.location=pivot
        moving.name='Hinged double barrel assembly';moving['mechanism']='break_action'
        for side in (-1,1):b('Breech side lock',(side*.046,.035,.095),(.008,.093,.048),'panel')
        p('Composite shoulder stock',[(-.30,.086,.106,.01),(-.24,.067,.078,.022),(-.12,.06,.055,.061)],'stock')
        b('Recoil pad',(0,-.31,.01),(.093,.02,.115),'rubber')
        b('Breech latch',(0,.035,.151),(.017,.065,.015),'steel')
        handle(-.026);fasteners(.067,.095,.05);muzzle=(0,.824,.121)
    marker=empty('muzzle',root);marker.location=muzzle;marker['socket']='muzzle'
    grip=empty('grip',root);grip['socket']='grip'
    import json
    position=json.loads((api['OUT']/'support-grips.json').read_text())[name]
    support=empty('support_grip',root);support['socket']='support_grip'
    support['purpose']='Optional physical support hand; orientation only'
    support.location=(position[0],-position[2],position[1])
    root['depth_sections']=str(sections)
    flashes={state:flash(item,state,root,muzzle) for state in (2,3)}
    body=[o for o in list(root.children) if o.type=='MESH' and o not in flashes.values()]
    original_children={o:list(o.children) for o in root.children_recursive}
    variants=[];mechanisms=[]
    for state in range(4):
        variant=empty('sprite_frame_'+str(state),root);variants.append(variant)
        def duplicate(original,parent):
            if state==0:obj=original;obj.parent=parent
            else:
                obj=original.copy();obj.data=original.data.copy();api['models'].objects.link(obj);obj.parent=parent
                if obj.type=='MESH':
                    for value in obj.data.uv_layers.active.data:value.uv.x+=state*.25
            if original==moving:mechanisms.append(obj)
            for child in original_children.get(original,[]):duplicate(child,obj)
            return obj
        for original in body:duplicate(original,variant)
    # Duplicate every state before joining; joining consumes source objects.
    for variant in variants:
        # Batch static detail into one mesh per state; the actual moving
        # mechanism remains independent for native glTF transform animation.
        static=[o for o in variant.children if o.type=='MESH' and o not in mechanisms]
        bpy.ops.object.select_all(action='DESELECT')
        for o in static:o.select_set(True)
        if static:
            bpy.context.view_layer.objects.active=static[0];bpy.ops.object.join();static[0].name='Detailed weapon shell'
    for mechanism in mechanisms:
        pieces=[mechanism]+[o for o in mechanism.children_recursive if o.type=='MESH']
        bpy.ops.object.select_all(action='DESELECT')
        for o in pieces:o.select_set(True)
        bpy.context.view_layer.objects.active=mechanism
        if len(pieces)>1:bpy.ops.object.join()
    return root,mechanisms,flashes,variants
