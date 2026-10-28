#!/usr/bin/env python3
"""Build the pinned Khronos loader with the game's cross compiler, never as root."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import urllib.request

here = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
parser.add_argument('--jobs', type=int, default=4)
args = parser.parse_args()
if os.getuid() == 0:
    parser.error('Build as the regular user')
args.output.mkdir(parents=True, exist_ok=True)
spec = json.loads((here/'openxr-loader-source.json').read_text())
archive = args.output/'source.tar.gz'
if not archive.exists():
    with urllib.request.urlopen(spec['url'], timeout=120) as response:
        archive.write_bytes(response.read())
if hashlib.sha256(archive.read_bytes()).hexdigest() != spec['sha256']:
    raise SystemExit('OpenXR loader source checksum mismatch')
source = args.output/('OpenXR-SDK-'+spec['commit'])
if not source.exists():
    with tarfile.open(archive) as tar:
        tar.extractall(args.output, filter='data')
# Ubuntu's older MinGW headers predate the OneCore SYSTEM partition. For our
# Win32 desktop loader an unavailable SYSTEM partition is correctly zero.
compat = args.output/'winapi-compat.h'
compat_text = '#include <winapifamily.h>\n#ifndef WINAPI_PARTITION_SYSTEM\n#define WINAPI_PARTITION_SYSTEM 0\n#endif\n'
if not compat.exists() or compat.read_text() != compat_text:
    compat.write_text(compat_text)
build = args.output/'build'
subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                '-DCMAKE_TOOLCHAIN_FILE='+str(here/'mingw-toolchain.cmake'),
                '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS=-include '+str(compat), '-DDYNAMIC_LOADER=ON',
                '-DBUILD_LOADER=ON', '-DBUILD_TESTS=OFF', '-DBUILD_API_LAYERS=OFF',
                '-DBUILD_CONFORMANCE_TESTS=OFF', '-DBUILD_WITH_SYSTEM_JSONCPP=OFF'], check=True)
subprocess.run(['cmake', '--build', str(build), '--target', 'openxr_loader',
                '--parallel', str(args.jobs)], check=True)
loaders = list(build.rglob('libopenxr_loader.dll'))
if len(loaders) != 1:
    raise SystemExit('Expected exactly one rebuilt libopenxr_loader.dll')
import shutil
shutil.copy2(loaders[0], args.output/'libopenxr_loader.dll')
shutil.copy2(source/'LICENSE', args.output/'LICENSE.OpenXR')
shutil.copy2(source/'src/external/jsoncpp/LICENSE', args.output/'LICENSE.JsonCpp')
(args.output/'provenance.json').write_text(json.dumps(spec, indent=2)+'\n')
