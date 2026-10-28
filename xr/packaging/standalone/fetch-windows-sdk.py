#!/usr/bin/env python3
"""Fetch the pinned generic Windows SDK libraries from the official MSYS2 mirror."""
import argparse,hashlib,json,subprocess,urllib.request
from pathlib import Path
here=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--manifest',type=Path,default=here/'windows-sdk-manifest.json')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
cache=a.output.parent/'windows-downloads';cache.mkdir(exist_ok=True)
for row in json.loads(a.manifest.read_text()):
 name=row['url'].rsplit('/',1)[-1];archive=cache/name
 if not archive.exists():
  with urllib.request.urlopen(row['url'],timeout=120) as response:archive.write_bytes(response.read())
 if hashlib.sha256(archive.read_bytes()).hexdigest()!=row['sha256']:raise SystemExit('Checksum mismatch: '+name)
 subprocess.run(['tar','--zstd','-xf',str(archive),'-C',str(a.output),'--exclude=.PKGINFO','--exclude=.BUILDINFO','--exclude=.MTREE'],check=True)
 print('Verified '+name,flush=True)
