#!/usr/bin/env python3
"""Bundle compiler-matched runtimes and verify actual PE imports/exports before ZIP creation."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import pefile

# Only OS components may be omitted; unknown DLLs fail closed.
SYSTEM_DLLS = set('advapi32 avrt bcrypt cfgmgr32 combase comctl32 comdlg32 crypt32 '
                 'd3d11 d3d12 dbghelp dinput8 dnsapi dsound dwmapi dxgi gdi32 glu32 '
                 'hid imm32 iphlpapi kernel32 msimg32 msvcrt ntdll ole32 oleaut32 '
                 'opengl32 powrprof psapi rpcrt4 secur32 setupapi shell32 shlwapi '
                 'ucrtbase user32 userenv usp10 uxtheme version winhttp wininet winmm '
                 'winspool ws2_32 wtsapi32'.split())
RUNTIMES = ('libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll')

def system_dll(name):
    name = name.lower()
    return name.removesuffix('.dll') in SYSTEM_DLLS or name.startswith(('api-ms-win-', 'ext-ms-win-'))

def image(path):
    pe = pefile.PE(str(path), fast_load=True)
    try:
        if pe.FILE_HEADER.Machine != 0x8664 or pe.OPTIONAL_HEADER.Magic != 0x20b:
            raise ValueError(f'{path.name}: expected Windows x86_64 PE32+')
        pe.parse_data_directories(directories=[0, 1, 13])
        imports = []
        for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []) + getattr(pe, 'DIRECTORY_ENTRY_DELAY_IMPORT', []):
            imports.extend((entry.dll.decode().lower(), imp.name.decode() if imp.name else imp.ordinal)
                           for imp in entry.imports)
        exports = {}
        table = getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None)
        for symbol in table.symbols if table else []:
            if not symbol.address:
                continue
            forward = symbol.forwarder.decode() if symbol.forwarder else None
            exports[symbol.ordinal] = forward
            if symbol.name:
                exports[symbol.name.decode()] = forward
        return imports, exports
    finally:
        pe.close()

def audit(directory):
    paths = {}
    for path in sorted(directory.iterdir()):
        if path.suffix.lower() in ('.dll', '.exe'):
            if path.name.lower() in paths:
                raise ValueError('Case-colliding PE filenames: '+path.name)
            paths[path.name.lower()] = path
    images = {name: image(path) for name, path in paths.items()}
    errors, os_dlls = [], set()
    def exported(dll, symbol, visited):
        if system_dll(dll):
            os_dlls.add(dll)
            return True
        if dll not in images or symbol not in images[dll][1]:
            return False
        forward = images[dll][1][symbol]
        if not forward:
            return True
        key = (dll, symbol)
        if key in visited:
            return False
        target, symbol = forward.rsplit('.', 1)
        target = target.lower()
        if not target.endswith('.dll'):
            target += '.dll'
        return exported(target, int(symbol[1:]) if symbol.startswith('#') else symbol, visited | {key})
    count = 0
    for name, (imports, _) in images.items():
        for dll, symbol in imports:
            count += 1
            if not exported(dll, symbol, set()):
                errors.append(f'{name} imports {dll}!{symbol}, which is not supplied by the bundle')
    if errors:
        raise ValueError('\n'.join(errors))
    return {'architecture': 'x86_64', 'pe_files': len(images), 'imports_checked': count,
            'system_dlls': sorted(os_dlls),
            'sha256': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in paths.items()}}

def bundle(directory, sdk, loader, extras):
    available = {p.name.lower(): p for p in sdk.glob('*.dll')}
    for name in RUNTIMES:
        path = Path(subprocess.check_output(['x86_64-w64-mingw32-g++', '-print-file-name='+name], text=True).strip())
        if not path.is_absolute() or not path.is_file():
            raise ValueError('Compiler runtime not found: '+name)
        available[name] = path
    available['libopenxr_loader.dll'] = loader
    queue = list(directory.glob('*.exe')) + [available[name.lower()] for name in extras]
    seen = set()
    while queue:
        path = queue.pop()
        name = path.name.lower()
        if name in seen:
            continue
        seen.add(name)
        dest = directory/path.name
        if path.resolve() != dest.resolve():
            shutil.copy2(path, dest)
        for dep, _ in image(dest)[0]:
            if system_dll(dep):
                continue
            if dep not in available:
                raise ValueError(f'{path.name}: missing non-system DLL {dep}')
            queue.append(available[dep])
    return audit(directory)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--loader', type=Path)
    parser.add_argument('--extra', action='append', default=[])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    try:
        if bool(args.sdk) != bool(args.loader):
            parser.error('--sdk and --loader must be supplied together')
        result = bundle(args.directory, args.sdk, args.loader, args.extra) if args.sdk else audit(args.directory)
        if args.sdk:
            source = args.loader.parent/'provenance.json'
            if source.exists():
                result['openxr_loader_source'] = json.loads(source.read_text())
            result['compiler'] = subprocess.check_output(['x86_64-w64-mingw32-g++', '--version'], text=True).splitlines()[0]
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(result, indent=2)+'\n')
        print(f"PASS: {result['pe_files']} x86_64 PE files, {result['imports_checked']} named/ordinal imports verified")
    except (ValueError, pefile.PEFormatError) as error:
        parser.exit(1, str(error)+'\n')

if __name__ == '__main__':
    main()
