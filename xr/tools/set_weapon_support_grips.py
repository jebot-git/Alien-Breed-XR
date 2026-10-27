#!/usr/bin/env python3
"""Add editable rest support sockets without re-exporting weapon geometry/animation."""
from pathlib import Path
import json
import struct

root=Path(__file__).resolve().parents[2]/'xr/assets/weapons'
points=json.loads((root/'support-grips.json').read_text())
for name,position in points.items():
    if name=='description':continue
    path=root/(name+'.glb');raw=path.read_bytes()
    length=struct.unpack_from('<I',raw,12)[0]
    document=json.loads(raw[20:20+length]);tail=raw[20+length:]
    parents=[n for n in document['nodes'] if 'weapon_slot' in n.get('extras',{})]
    assert len(parents)==1
    indices=[i for i,n in enumerate(document['nodes']) if n.get('name')=='support_grip']
    assert len(indices)<=1
    node=dict(name='support_grip',translation=position,extras=dict(socket='support_grip',purpose='Optional physical support hand; orientation only'))
    if indices:document['nodes'][indices[0]]=node
    else:
        parents[0].setdefault('children',[]).append(len(document['nodes']))
        document['nodes'].append(node)
    encoded=json.dumps(document,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
    contents=struct.pack('<II',len(encoded),0x4e4f534a)+encoded+tail
    path.write_bytes(struct.pack('<III',0x46546c67,2,len(contents)+12)+contents)
    print('Support socket:',name,position)
