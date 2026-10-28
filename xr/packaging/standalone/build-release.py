#!/usr/bin/env python3
"""Build standalone release artifacts in rootless distro containers as the caller."""
import argparse,hashlib,json,os,shutil,subprocess,tarfile
from pathlib import Path
here=Path(__file__).resolve().parent
meta=json.loads((here/'release.json').read_text())
root=next(p for p in here.parents if (p/'CMakeLists.txt').is_file())
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('platform',choices=['ubuntu','fedora','arch','windows'])
p.add_argument('--output',type=Path,default=root/'dist')
p.add_argument('--image',help='Use an already built builder image')
p.add_argument('--windows-sdk',type=Path,help='Extracted MSYS2 mingw64 tree (contains mingw64/)')
p.add_argument('--jobs',type=int,default=4)
a=p.parse_args()
if os.getuid()==0:raise SystemExit('Build as a regular user with rootless Podman.')
work=a.output.resolve()/('work-'+meta['package']+'-'+a.platform)
work.mkdir(parents=True,exist_ok=True)
source=work/'source'
if source.exists():shutil.rmtree(source)
source.mkdir()
tracked=subprocess.check_output(['git','-C',str(root),'ls-files','--cached','--others','--exclude-standard','-z']).decode().split('\0')
for name in tracked:
 if not name or name.split('/')[0].startswith(('build','dist','artifacts')):continue
 src=root/name
 if src.is_file():
  dst=source/name;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
# The second game's original local assets are deliberately not tracked in Git.
if meta['package']=='alien-breed3d2-xr':
 if not (root/'assets/native/manifest.json').exists():raise SystemExit('Provide the original converted game assets before packaging.')
 shutil.copytree(root/'assets',source/'assets',dirs_exist_ok=True)
image=a.image or ('localhost/de-xr-standalone:'+a.platform)
if not a.image:subprocess.run(['podman','build','-t',image,'-f',str(here/(a.platform+'.Containerfile')),str(here)],check=True)
tree=hashlib.sha256()
for file in sorted(source.rglob('*')):
 if file.is_file():tree.update(str(file.relative_to(source)).encode()+b'\0'+hashlib.sha256(file.read_bytes()).digest())
provenance=dict(project=meta['project'],source_commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),source_tree_sha256=tree.hexdigest(),platform=a.platform,architecture='x86_64',standalone=True,optimization='Release',builder_image=image)
(work/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
flags=['-DDE_XR_BUILD_INFO=/output/provenance.json','-DDE_XR_RELEASE_VERSION='+meta['version'],'-DCMAKE_BUILD_TYPE=Release','-DDE_XR_STANDALONE=ON','-DBUILD_TESTING=OFF','-DCMAKE_INSTALL_PREFIX=/usr','-DCMAKE_INSTALL_DOCDIR=share/doc/'+meta['package'],*meta['cmake_options']]
commands=[['cmake','-S','/output/source','-B','/output/build','-G','Ninja',*flags],['cmake','--build','/output/build','--parallel',str(a.jobs)],['cmake','--install','/output/build','--prefix','/usr']]
# Install is always staged, never into the container's or host's live filesystem.
script='set -euo pipefail\nrm -rf /output/stage /output/stage-root /output/packages\n'
import shlex
if a.platform=='windows':
 if not a.windows_sdk:raise SystemExit('--windows-sdk is required; see README.md')
 commands[0]+=['-DCMAKE_TOOLCHAIN_FILE=/toolchain/mingw-toolchain.cmake','-DCMAKE_INSTALL_PREFIX=/standalone','-DCMAKE_INSTALL_DATADIR=share','-DCMAKE_INSTALL_BINDIR=bin','-DCMAKE_INSTALL_LIBEXECDIR=libexec']
 commands[2]=['cmake','--install','/output/build','--prefix','/standalone']
for i,c in enumerate(commands):script+=(('DESTDIR=/output/stage-root ' if a.platform=='windows' else 'DESTDIR=/output/stage ') if i==2 else '')+shlex.join(c)+'\n'
if a.platform=='windows':
 script+='mv /output/stage-root/standalone /output/stage\n'
 script+='python3 /toolchain/test-windows-runtime.py\n'
 script+='python3 /toolchain/build-windows-loader.py /output/windows-loader --jobs '+str(a.jobs)+'\n'
 script+='python3 /toolchain/windows-runtime.py /output/stage/bin --sdk /sdk/mingw64/bin --loader /output/windows-loader/libopenxr_loader.dll --report /output/stage/share/doc/'+meta['package']+'/WINDOWS-RUNTIME.json'
 if meta['package']=='tempest-xr':script+=' --extra libopenmpt-0.dll'
 script+='\n'
 script+='mkdir -p /output/stage/share/licenses/windows-toolchain\ncp /output/windows-loader/LICENSE.* /output/stage/share/licenses/windows-toolchain/\ncp /usr/share/doc/gcc-mingw-w64-base/copyright /output/stage/share/licenses/windows-toolchain/GCC-runtime-copyright\n'

if a.platform in ['ubuntu','fedora']:
 generator='DEB' if a.platform=='ubuntu' else 'RPM'
 script+='cd /output/build\ncpack -G '+generator+' -B /output/packages\n'
elif a.platform=='arch':
 # makepkg creates and validates the native package metadata as a non-root user.
 package=work/'arch';package.mkdir(exist_ok=True)
 dependencies=' '.join(shlex.quote(x) for x in meta['arch_dependencies'])
 (package/'PKGBUILD').write_text(f"pkgname={meta['package']}\npkgver={meta['version']}\npkgrel=1\npkgdesc='Standalone desktop and generic OpenXR game'\narch=('x86_64')\nurl='https://github.com/jebot-git/{meta['project']}'\nlicense=('custom:Original-Game-Rights')\ndepends=({dependencies})\noptions=('!strip' '!debug')\npackage() {{\n cp -a /output/stage/. \"$pkgdir/\"\n}}\n")
 script+='cd /output/arch\nmakepkg --nodeps --noconfirm --force\nmkdir -p /output/packages\ncp *.pkg.tar.zst /output/packages/\n'
args=['podman','run','--rm','--userns=keep-id','--user',f'{os.getuid()}:{os.getgid()}','--security-opt','label=disable','-v',str(work)+':/output:rw','-v',str(here)+':/toolchain:ro']
if a.platform=='windows':args+=['-v',str(a.windows_sdk.resolve())+':/sdk:ro']
with (work/'build.log').open('w') as log:
 subprocess.run([*args,image,'bash','-c',script],stdout=log,stderr=subprocess.STDOUT,check=True)
version=meta['version'];output=a.output.resolve()
if a.platform=='windows':
 sdk=a.windows_sdk.resolve()/'mingw64'
 for mode,flag in [('XR','--xr'),('desktop','--desktop')]:
  (work/'stage'/('Play in '+mode+'.cmd')).write_bytes(('@echo off\r\nstart "" "%~dp0bin\\'+meta['package']+'.exe" '+flag+' %*\r\n').encode())
 for unused in ['share/applications','share/icons']:
  shutil.rmtree(work/'stage'/unused,ignore_errors=True)
 licenses=work/'stage/share/licenses/windows-libraries';licenses.mkdir(parents=True,exist_ok=True)
 if (sdk/'share/licenses').exists():shutil.copytree(sdk/'share/licenses',licenses,dirs_exist_ok=True)
 archive=output/f"{meta['package']}-{version}-windows-x86_64"
 shutil.make_archive(str(archive),'zip',work/'stage')
 artifacts=[Path(str(archive)+'.zip')]
else:
 artifacts=[]
 for file in (work/'packages').iterdir():
  if file.is_file():
   dest=output/(meta['package']+'-'+version+'-'+a.platform+'-x86_64'+(''.join(file.suffixes) if file.suffix!='.rpm' else '.rpm'))
   # Arch versions contain dots, so choose extension explicitly.
   suffix='.deb' if a.platform=='ubuntu' else '.rpm' if a.platform=='fedora' else '.pkg.tar.zst'
   dest=output/(meta['package']+'-'+version+'-'+a.platform+'-x86_64'+suffix);shutil.copy2(file,dest);artifacts.append(dest)
report=dict(**provenance,version=version,artifacts=[dict(file=x.name,sha256=hashlib.sha256(x.read_bytes()).hexdigest()) for x in artifacts])
(work/'release.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
