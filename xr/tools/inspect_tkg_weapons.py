#!/usr/bin/env python3
"""Validate/export original TKG vector weapons; no game/runtime changes.

Supply a local directory containing the compiled media/vectobj weapons from
mheyer32/alienbreed3d2. Does not download or relicense game data. OBJ exports
retain raw Amiga coordinates and polygon UVs; textures are not exported.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

WEAPONS = ('pistol', 'shotgun', 'plasmagun', 'grenadelauncher',
           'rocketlauncher', 'blaster', 'rifle', 'sawgun')


class VectorModel:
    def __init__(self, data):
        self.data = data
        self.points = self.word(2)
        self.frame_count = self.word(4)
        if self.word(0) not in (0, 65535) or not 1 <= self.points <= 4096 or not 1 <= self.frame_count <= 256:
            raise ValueError('Not a supported compiled TKG vector model')
        self.parts = []
        offset = 6 + self.frame_count * 4
        for _ in range(32):
            polygon = self.word(offset)
            if polygon & 32768:
                break
            sort_point = self.word(offset + 2)
            if sort_point % 10 or sort_point // 10 >= self.points:
                raise ValueError('Invalid part sort vertex')
            self.parts.append(self.polygons(2 + polygon))
            offset += 4
        else:
            raise ValueError('Missing part terminator or more than 32 visibility bits')
        if not self.parts:
            raise ValueError('Empty model')
        self.frames = []
        self.legacy = None
        for frame in range(self.frame_count):
            # Offsets are relative to the byte after the initial sort flag.
            point_offset = self.word(6 + frame * 4) + 2
            normal_offset = self.word(8 + frame * 4) + 2
            angle_bytes = (self.points + 1) // 2 * 2
            span = normal_offset - point_offset
            modern = span == 4 + angle_bytes + self.points * 6
            legacy = span == angle_bytes + self.points * 6
            if not modern and not legacy:
                raise ValueError('Unrecognized point-angle/visibility frame layout')
            if self.legacy is not None and self.legacy != legacy:
                raise ValueError('Mixed frame layouts')
            self.legacy = legacy
            mask = (1 << len(self.parts)) - 1 if legacy else self.longword(point_offset)
            vertices_at = point_offset + angle_bytes + (0 if legacy else 4)
            self.require(vertices_at, self.points * 6)
            vertices = [struct.unpack_from('>hhh', data, vertices_at + i * 6) for i in range(self.points)]
            # One polygon normal-angle byte per polygon, padded to a word.
            polygons = sum(map(len, self.parts))
            self.require(normal_offset, (polygons + 1) // 2 * 2)
            self.frames.append((mask, vertices))

    def require(self, offset, size):
        if offset < 0 or offset + size > len(self.data):
            raise ValueError('Truncated vector data')

    def word(self, offset):
        self.require(offset, 2)
        return struct.unpack_from('>H', self.data, offset)[0]

    def longword(self, offset):
        self.require(offset, 4)
        return struct.unpack_from('>I', self.data, offset)[0]

    def polygons(self, offset):
        polygons = []
        for _ in range(4096):
            edges = self.word(offset)
            if edges & 32768:
                return polygons
            count = edges + 1
            if not 3 <= count <= 64:
                raise ValueError('Invalid polygon size')
            self.require(offset, 18 + edges * 4)
            indices, uvs = [], []
            for i in range(count):
                at = offset + 4 + i * 4
                index = self.word(at)
                if index >= self.points:
                    raise ValueError('Polygon vertex outside point table')
                indices.append(index)
                uvs.append(tuple(self.data[at + 2:at + 4]))
            if self.word(offset + 4 + count * 4) != indices[0]:
                raise ValueError('Polygon stream is not closed')
            texture = self.word(offset + 12 + edges * 4)
            polygons.append((indices, uvs, texture))
            offset += 18 + edges * 4
        raise ValueError('Missing polygon terminator')

    def summary(self):
        bounds = []
        for mask, vertices in self.frames:
            bounds.append([[min(v[i] for v in vertices) for i in range(3)],
                           [max(v[i] for v in vertices) for i in range(3)]])
        return dict(points=self.points, frames=self.frame_count, parts=len(self.parts),
                    polygons=sum(map(len, self.parts)),
                    triangles=sum(len(p[0]) - 2 for part in self.parts for p in part),
                    distinct_geometry_frames=len({str(v) for _, v in self.frames}),
                    distinct_visibility_masks=sorted({mask for mask, _ in self.frames}),
                    legacy_without_visibility_mask=self.legacy, bounds=bounds,
                    texture_words=sorted({p[2] for part in self.parts for p in part}),
                    sha256=hashlib.sha256(self.data).hexdigest())

    def export(self, target, frame):
        mask, vertices = self.frames[frame]
        with target.open('w') as out:
            out.write('# TKG inspection export: raw Amiga coordinates, Y down; no textures\n')
            out.write('# Original 64x64 tile UV bytes divided by 64; texture words preserved in groups\n')
            for v in vertices:
                out.write('v %s %s %s\n' % v)
            uv_index = 1
            for part_index, part in enumerate(self.parts):
                if not mask & (1 << part_index):
                    continue
                for indices, uvs, texture in part:
                    out.write(f'g part_{part_index}_texture_{texture:04x}\n')
                    for u, v in uvs:
                        out.write(f'vt {u/64:g} {1-v/64:g}\n')
                    out.write('f ' + ' '.join(f'{idx+1}/{uv_index+i}' for i, idx in enumerate(indices)) + '\n')
                    uv_index += len(uvs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--all-frames', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(coordinates='Raw authored Amiga units; Y down; no controller mount or texture reconstruction', weapons={})
    for name in WEAPONS:
        model = VectorModel((args.source / name).read_bytes())
        report['weapons'][name] = model.summary()
        for frame in range(model.frame_count if args.all_frames else 1):
            model.export(args.output / f'{name}-{frame:02}.obj', frame)
        s = model.summary()
        print(f'{name}: {s["points"]} vertices, {s["polygons"]} polygons, {s["frames"]} frames, legacy={model.legacy}')
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
