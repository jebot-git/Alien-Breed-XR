#!/usr/bin/env python3
"""Check serialized glTF firing states against the original 50Hz timeline."""
import json
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[2]/'xr/assets/weapons'
manifest=json.loads((ROOT/'reconstruction.json').read_text())
for item in manifest['weapons']:
    raw=(ROOT/(item['name']+'.glb')).read_bytes()
    assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
    length=struct.unpack_from('<I',raw,12)[0]
    g=json.loads(raw[20:20+length]);binary=raw[28+length:]
    def accessor(index):
        a=g['accessors'][index];v=g['bufferViews'][a['bufferView']]
        n={'SCALAR':1,'VEC3':3,'VEC4':4}[a['type']]
        return [struct.unpack_from('<'+'f'*n,binary,v.get('byteOffset',0)+a.get('byteOffset',0)+i*v.get('byteStride',n*4)) for i in range(a['count'])]
    assert all('bufferView' in image and 'uri' not in image for image in g['images'])
    assert all('uri' not in b for b in g['buffers'])
    anim=g['animations'][0];assert anim['name']=='Fire'
    channels={g['nodes'][c['target']['node']]['name'].split('.')[0]:anim['samplers'][c['sampler']] for c in anim['channels'] if c['target']['path']=='scale' and g['nodes'][c['target']['node']].get('name','').startswith('sprite_frame_')}
    assert len(channels)==4
    for tick,expected in enumerate(item['sequence']):
        # Sample on ticks and halfway between ticks: appearance must not blend.
        for subframe in (0,.5):
            values=[]
            for frame in range(4):
                s=channels[f'sprite_frame_{frame}'];assert s['interpolation']=='STEP'
                times=[x[0] for x in accessor(s['input'])];samples=accessor(s['output'])
                at=max(i for i,t in enumerate(times) if t<=(tick+subframe)/50+1e-6)
                values.append(samples[at][0])
            assert values[expected]==1 and sum(values)==1,(item['name'],tick,expected,values)
    for node in g['nodes']:
        assert not node.get('name','').startswith('REFERENCE')
        if node.get('name','').startswith('sprite_frame_'):
            frame=int(node['name'].split('.')[0].split('_')[-1])
            assert node.get('scale',[1,1,1])==([1,1,1] if frame==0 else [0,0,0])
    assert sum(n.get('name')=='grip' for n in g['nodes'])==1
    assert sum(n.get('name')=='muzzle' for n in g['nodes'])==1
    print('PASS',item['name'],len(item['sequence']),'engine ticks, STEP appearance, neutral default, embedded textures and sockets')
