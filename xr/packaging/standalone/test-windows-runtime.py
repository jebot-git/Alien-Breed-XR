#!/usr/bin/env python3
"""Build small real PE fixtures to test symbol-level Windows package validation."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('windows_runtime', here/'windows-runtime.py')
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)

class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='windows-runtime-test-')
        self.root = Path(self.temp.name)
        self.good = self.root/'good'; self.good.mkdir()
        self.package = self.root/'package'; self.package.mkdir()
        (self.root/'good.c').write_text('__declspec(dllexport) int expected_value(void){return 42;}')
        (self.root/'bad.c').write_text('__declspec(dllexport) int wrong_value(void){return 0;}')
        (self.root/'main.c').write_text('__declspec(dllimport) int expected_value(void); int main(void){return expected_value()!=42;}')
        self.compile('-shared', self.root/'good.c', '-o', self.good/'fixture.dll',
                     '-Wl,--out-implib,'+str(self.good/'libfixture.dll.a'))
        self.compile(self.root/'main.c', '-L'+str(self.good), '-lfixture', '-o', self.package/'game.exe')
        shutil.copy2(self.good/'fixture.dll', self.package/'fixture.dll')
    def tearDown(self):
        self.temp.cleanup()
    def compile(self, *args):
        subprocess.run(['x86_64-w64-mingw32-gcc', *map(str, args)], check=True)
    def test_compatible_bundle(self):
        result = runtime.audit(self.package)
        self.assertEqual(result['pe_files'], 2)
        self.assertGreater(result['imports_checked'], 0)
    def test_wrong_runtime_with_same_filename(self):
        self.compile('-shared', self.root/'bad.c', '-o', self.package/'fixture.dll')
        with self.assertRaisesRegex(ValueError, 'fixture.dll!expected_value'):
            runtime.audit(self.package)
    def test_missing_non_system_dll(self):
        (self.package/'fixture.dll').unlink()
        with self.assertRaisesRegex(ValueError, 'fixture.dll'):
            runtime.audit(self.package)
    def test_wrong_architecture(self):
        data = bytearray((self.package/'fixture.dll').read_bytes())
        pe_offset = int.from_bytes(data[0x3c:0x40], 'little')
        data[pe_offset+4:pe_offset+6] = (0x14c).to_bytes(2,'little')
        (self.package/'fixture.dll').write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'expected Windows x86_64'):
            runtime.audit(self.package)
    def test_ordinal_import(self):
        definitions = self.root/'ordinal.def'
        definitions.write_text('LIBRARY fixture\nEXPORTS\nexpected_value @7 NONAME\n')
        self.compile('-shared', self.root/'good.c', definitions, '-o', self.good/'fixture.dll',
                     '-Wl,--out-implib,'+str(self.good/'libfixture.dll.a'))
        self.compile(self.root/'main.c', '-L'+str(self.good), '-lfixture', '-o', self.package/'game.exe')
        shutil.copy2(self.good/'fixture.dll',self.package/'fixture.dll')
        runtime.audit(self.package)
        self.compile('-shared',self.root/'bad.c','-o',self.package/'fixture.dll')
        with self.assertRaisesRegex(ValueError,'fixture.dll!7'):
            runtime.audit(self.package)
    def test_forwarded_export(self):
        forward = self.root/'forward.def'
        forward.write_text('LIBRARY fixture\nEXPORTS\nexpected_value=other.expected_value\n')
        self.compile('-shared', self.root/'bad.c', forward, '-o', self.package/'fixture.dll')
        shutil.copy2(self.good/'fixture.dll',self.package/'other.dll')
        runtime.audit(self.package)
        self.compile('-shared',self.root/'bad.c','-o',self.package/'other.dll')
        with self.assertRaisesRegex(ValueError,'fixture.dll!expected_value'):
            runtime.audit(self.package)

if __name__ == '__main__':
    unittest.main()
