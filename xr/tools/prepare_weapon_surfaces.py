#!/usr/bin/env python3
"""Build palette-faithful weapon UV surfaces and explicit single-view depth guides.

The original sprite supplies silhouette, metal pixels and colour accents.
Hand pixels are excluded. Hidden side/bottom panels are authored continuations,
not claimed recovered detail. Run before build_sprite_weapons_blender.py.
"""
import hashlib
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'xr/assets/weapons'
NAMES=('pistol','shotgun','plasma','grenade','rocket')
TILES=('receiver_side','receiver_top','receiver_end','steel','rubber','vents','accent','dark',
       'barrel','bore','bolts','panel','stock','heatshield','marking','edge')

def hand(c):
    return c[0]>c[1]*1.25 and c[1]>c[2]*1.2 and c[0]>40

def prepare(name):
    frames=[Image.open(OUT/'sprites'/f'{name}-{n}.png').convert('RGBA') for n in range(4)]
    neutral=frames[0]
    points=[(x,y,neutral.getpixel((x,y))) for y in range(58) for x in range(96)
            if neutral.getpixel((x,y))[3] and not hand(neutral.getpixel((x,y)))]
    palette=sorted({p[2][:3] for p in points})
    gray=[c for c in palette if max(c)-min(c)<24]
    assert gray
    def nearest(value):return min(gray,key=lambda c:sum((v-value)**2 for v in c))
    def rgb(value):return nearest(value) if isinstance(value,(int,float)) else min(palette,key=lambda c:sum((c[i]-value[i])**2 for i in range(3)))
    black=nearest(17);dark=nearest(34);mid=nearest(85);light=nearest(153);shine=nearest(204)
    accent=max(palette,key=lambda c:c[2]-max(c[0],c[1])) if name=='plasma' else max(palette,key=lambda c:c[1]-max(c[0],c[2]))
    if name in ('grenade','shotgun'):accent=light
    # Row extents after hand removal. These constrain the visible surface and
    # document where the unobserved depth/underside must be inferred.
    rows=[]
    for y in sorted({p[1] for p in points}):
        xs=[p[0] for p in points if p[1]==y]
        rows.append(dict(y=y,left=min(xs),right=max(xs),width=max(xs)-min(xs)+1))
    atlas=Image.new('RGB',(1024,256),black)
    for state in range(4):
        state_accent=accent
        if name=='plasma' and state==1:
            colors=[c[:3] for c in frames[1].getdata() if c[3] and c[0]>c[2]*1.2 and not hand(c)]
            if colors:state_accent=max(colors,key=lambda c:c[0]-c[2])
        for index,kind in enumerate(TILES):
            tile=Image.new('RGB',(64,64),mid);d=ImageDraw.Draw(tile)
            for y in range(64):
                for x in range(64):
                    # Broad, deliberately pixelated ramps; no filtered photo noise.
                    value=68+28*math.sin(x/63*math.pi)+8*((x//8+y//8)%2)
                    if kind in ('steel','barrel','edge'):value+=30
                    if kind in ('rubber','dark','bore','stock'):value=20+14*math.sin(x/63*math.pi)
                    color=nearest(value)
                    if name=='rocket' and kind in ('receiver_side','receiver_end','panel','stock','heatshield'):
                        color=rgb((value*.45,value*.85,value*.4))
                    tile.putpixel((x,y),color)
            d.rectangle((1,1,62,62),outline=dark,width=2)
            d.line((3,3,60,3),fill=light);d.line((3,4,3,60),fill=nearest(119))
            d.line((4,60,60,60),fill=black)
            if kind in ('receiver_side','receiver_end','panel','stock','marking'):
                d.rectangle((7,12,56,51),fill=nearest(68),outline=dark,width=2)
                d.line((9,14,53,14),fill=nearest(136))
                for x,y in ((5,7),(58,7),(5,56),(58,56)):
                    d.rectangle((x-1,y-1,x+1,y+1),fill=black);d.point((x-1,y-1),fill=light)
                for x,y,l in ((13,23,9),(28,38,13),(39,28,6),(17,47,5)):
                    d.line((x,y,x+l,y),fill=nearest(102));d.line((x,y+1,x+l//2,y+1),fill=dark)
            if kind in ('steel','barrel','heatshield'):
                for x in (12,29,47):d.line((x,7,x,56),fill=nearest(153));d.line((x+1,7,x+1,56),fill=nearest(51))
                for y in (7,55):d.line((4,y,59,y),fill=dark);d.line((4,y+1,59,y+1),fill=light)
            if kind in ('vents','heatshield'):
                for x in range(9,56,7):
                    d.rectangle((x,18,x+3,46),fill=black);d.line((x,17,x+4,17),fill=light)
            if kind in ('rubber','stock'):
                for y in range(8,58,5):d.line((6,y,57,y),fill=black);d.line((6,y+1,57,y+1),fill=nearest(51))
            if kind=='accent':
                d.rectangle((7,7,56,56),fill=state_accent,outline=black,width=3)
                for y in (13,48):d.line((11,y,52,y),fill=light)
                d.rectangle((16,20,47,42),fill=rgb(tuple(v*.65 for v in state_accent)))
                for x in range(21,45,6):d.rectangle((x,27,x+2,36),fill=state_accent)
            if kind=='bore':
                d.rectangle((0,0,63,63),fill=black)
                for x in (7,23,39,55):d.line((x,0,x,63),fill=dark)
            if kind=='bolts':
                d.ellipse((6,6,57,57),fill=dark,outline=light,width=3)
                d.line((16,16,47,47),fill=black,width=4);d.line((17,15,48,46),fill=nearest(119),width=1)
            if kind=='receiver_top':
                # Deproject successive sprite cross-sections onto the top UV
                # surface. Side UVs use authored panels instead of stretching
                # this perspective image over every face.
                for y in range(5,59):
                    row=rows[round((1-(y-5)/53)*(len(rows)-1))]
                    for x in range(5,59):
                        sx=round(row['left']+(x-5)/53*(row['right']-row['left']))
                        c=neutral.getpixel((sx,row['y']))
                        if not c[3] or hand(c):
                            c=min(points,key=lambda p:(p[0]-sx)**2+(p[1]-row['y'])**2)[2]
                        tile.putpixel((x,y),c[:3])
                d.rectangle((4,4,59,59),outline=dark)
            if kind=='marking':
                # Small original-palette industrial warning stripes, not a
                # modern high-resolution decal or a newly invented logo.
                for x in range(12,50,9):d.polygon([(x,20),(x+4,20),(x+12,39),(x+8,39)],fill=state_accent)
            atlas.paste(tile,(state*256+(index%4)*64,(index//4)*64))
    target=OUT/'textures'/f'{name}-surfaces.png';atlas.save(target)
    depth=Image.new('L',neutral.size)
    for row in rows:
        progress=(row['y']-rows[0]['y'])/max(1,rows[-1]['y']-rows[0]['y'])
        center=(row['left']+row['right'])/2;half=max(.5,(row['right']-row['left'])/2)
        for x in range(row['left'],row['right']+1):
            c=neutral.getpixel((x,row['y']))
            if c[3] and not hand(c):
                section=math.sqrt(max(0,1-((x-center)/half)**2))
                depth.putpixel((x,row['y']),round(32+progress*170+section*45))
    depth.save(OUT/'textures'/f'{name}-depth-guide.png')
    return dict(name=name,atlas='textures/'+target.name,atlas_size=list(atlas.size),tiles=list(TILES),
                palette=palette,hand_pixels_removed=sum(1 for c in neutral.getdata() if c[3] and hand(c)),
                source_rows=rows,source_sha256=hashlib.sha256((OUT/'sprites'/f'{name}-0.png').read_bytes()).hexdigest(),
                depth_model='Authored longitudinal depth plus rounded transverse sections; single-view inference, not measured hidden geometry',
                hidden_surfaces='Continuous hard-surface panels, inset seams, grip ribs, bores and fasteners; hand colours excluded')

if __name__=='__main__':
    report=dict(version=2,texel_filter='nearest',source='Original AB3D I sprites; authored depth and missing surfaces',weapons=[prepare(n) for n in NAMES])
    (OUT/'surfaces.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Prepared five original-palette UV atlases and explicit silhouette/depth guides')
