#!/usr/bin/env python3
"""Extract AB3D I sprite textures, animation timing and optional TKG references."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
NAMES={0:'pistol',1:'plasma',2:'rocket',3:'flamethrower',4:'grenade',7:'shotgun'}
SIZES={0:(.115,.34,.22),1:(.20,.48,.25),2:(.25,.68,.35),3:(.18,.48,.27),4:(.18,.53,.27),7:(.13,.82,.27)}


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data',type=Path,required=True)
    p.add_argument('--tkg',type=Path)
    p.add_argument('--output',type=Path,default=ROOT/'xr/assets/weapons')
    args=p.parse_args();out=args.output;out.mkdir(parents=True,exist_ok=True)
    (out/'sprites').mkdir(exist_ok=True);(out/'textures').mkdir(exist_ok=True)
    decoder=module('frames',ROOT/'tools/gun_frames_png.py')
    wad=(args.data/'includes/newgunsinhand.wad').read_bytes();ptr=(args.data/'includes/newgunsinhand.ptr').read_bytes()
    pal=next(x for x in (args.data/'pal').iterdir() if x.name.lower()=='newgunsinhand.pal')
    colors=decoder.read_palette(pal)
    source=(ROOT/'src/game_data.c').read_text().split('const GunAnim gun_anims[8] = {',1)[1].split('\n};',1)[0]
    source=re.sub(r'/\*.*?\*/','',source,flags=re.S)
    entries=re.findall(r'\.frames\s*=\s*\{([^}]*)\}\s*,\s*\.num_frames\s*=\s*(\d+)',source,re.S)
    assert len(entries)==8
    manifest={'source':'AB3D I original NewGunsInHand sprites and gun_anims[]','tick_hz':50,'weapons':[],'hashes':{}}
    for label,data in [('wad',wad),('ptr',ptr),('pal',pal.read_bytes())]:manifest['hashes'][label]=hashlib.sha256(data).hexdigest()
    for slot,name in NAMES.items():
        images=[]
        for f in range(4):
            rgba=decoder.decode_slot_to_rgba(slot*4+f,wad,ptr,colors)
            im=Image.frombytes('RGBA',(96,58),rgba);im.save(out/'sprites'/f'{name}-{f}.png');images.append(im)
        bbox=images[0].getbbox();atlas=Image.new('RGBA',(384,74))
        for f,im in enumerate(images):
            # Nearest opaque original texel fills projection gutters. No AI art,
            # smoothing, new color palette or generated highlights are used.
            points=[(x,y,im.getpixel((x,y))) for y in range(58) for x in range(96) if im.getpixel((x,y))[3]]
            filled=Image.new('RGBA',im.size)
            for y in range(58):
                for x in range(96):
                    rgba=im.getpixel((x,y))
                    if not rgba[3]:rgba=min(points,key=lambda p:(p[0]-x)**2+(p[1]-y)**2)[2]
                    filled.putpixel((x,y),rgba[:3]+(255,))
            atlas.paste(filled,(96*f,0))
        counts=Counter(images[0].getpixel((x,y))[:3] for y in range(58) for x in range(96) if images[0].getpixel((x,y))[3])
        palette=list(counts)
        gray=[c for c in palette if max(c)-min(c)<40] or palette
        swatches=[min(gray,key=sum),min(gray,key=lambda c:abs(sum(c)/3-90)),min(gray,key=lambda c:abs(sum(c)/3-170)),max(palette,key=lambda c:(c[1]-max(c[0],c[2])) if slot in (0,2) else ((c[2]-max(c[0],c[1])) if slot==1 else max(c)-min(c))),max(gray,key=sum)]
        for s,color in enumerate(swatches):atlas.paste(color+(255,),(s*16,58,(s+1)*16,74))
        for f,im in enumerate(images):
            frame_colors=[im.getpixel((x,y))[:3] for y in range(58) for x in range(96) if im.getpixel((x,y))[3]]
            accent=max(frame_colors,key=lambda c:(c[0]-max(c[1],c[2])) if slot==1 and f==1 else ((c[1]-max(c[0],c[2])) if slot in (0,2) else ((c[2]-max(c[0],c[1])) if slot==1 else max(c)-min(c))))
            for sw,color in enumerate(swatches):atlas.paste((accent if sw==3 else color)+(255,),(f*96+sw*16,58,f*96+(sw+1)*16,74))
        atlas.save(out/'textures'/f'{name}-sprite-atlas.png')
        for frame in (2,3):
            flash=Image.new('RGBA',(96,58))
            for y in range(58):
                for x in range(96):
                    c=images[frame].getpixel((x,y));b=images[0].getpixel((x,y))
                    changed=not b[3] or sum(abs(c[k]-b[k]) for k in range(3))>150
                    bright=(min(c[:3])>=200 and max(c[:3])>=238) or (slot==1 and c[2]>100 and c[2]>max(c[0],c[1])*1.5)
                    if c[3] and changed and bright and (frame==3 or slot==1):flash.putpixel((x,y),c)
            flash.save(out/'textures'/f'{name}-flash-{frame}.png')
        frames=[int(x) for x in re.findall(r'\d+',entries[slot][0])];duration=int(entries[slot][1])
        frames += [0] * max(0,duration+1-len(frames)) # C aggregate zero-initialized tail
        sequence=list(reversed(frames[:duration+1]))
        poses=[]
        # Grey metal pixel measurements provide repeatable pose estimates.
        for im in images:
            points=[(x,y) for y in range(58) for x in range(96) if (lambda c:c[3] and 34<max(c[:3])<220 and max(c[:3])-min(c[:3])<40)(im.getpixel((x,y)))]
            if not points:points=[(x,y) for y in range(58) for x in range(96) if im.getpixel((x,y))[3]]
            mx=sum(x for x,y in points)/len(points);my=sum(y for x,y in points)/len(points)
            xx=sum((x-mx)**2 for x,y in points);yy=sum((y-my)**2 for x,y in points);xy=sum((x-mx)*(y-my) for x,y in points)
            import math
            angle=.5*math.atan2(2*xy,yy-xx)
            poses.append({'center':[mx,my],'axis_angle':angle})
        item=dict(slot=slot,name=name,size_m=SIZES[slot],sprite_bounds=bbox,sprite_bounds_per_frame=[im.getbbox() for im in images],sequence=sequence,duration_s=duration/50,poses=poses,swatches=swatches)
        tkgname={'plasma':'plasmagun','rocket':'rocketlauncher','grenade':'grenadelauncher'}.get(name,name)
        if args.tkg and (args.tkg/tkgname).is_file():
            reader=module('tkg',ROOT/'xr/tools/inspect_tkg_weapons.py');model=reader.VectorModel((args.tkg/tkgname).read_bytes());mask,verts=model.frames[0]
            faces=[indices for pi,part in enumerate(model.parts) if mask&(1<<pi) for indices,uvs,texture in part]
            item['tkg_reference']={'name':tkgname,'vertices':verts,'faces':faces,'sha256':model.summary()['sha256']}
        manifest['weapons'].append(item)
    (out/'reconstruction.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Prepared six sprite atlases, 24 original frames, exact engine firing timelines and available TKG references')

if __name__=='__main__':main()
