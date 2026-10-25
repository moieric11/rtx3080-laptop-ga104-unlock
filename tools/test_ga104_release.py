"""Exercise firmware rejection and PowerShell package integrity checks without a USB."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleaseTests(unittest.TestCase):
    def test_reject_missing_and_wrong_firmware(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            assets = base / 'assets'
            assets.mkdir()
            command = [sys.executable, str(ROOT / 'tools/package_ga104_release.py'),
                       '--assets', str(assets), '--output', str(base / 'output')]
            missing = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(missing.returncode, 0)
            self.assertIn('booter_load_ga102.bin', missing.stderr)
            self.assertFalse((base / 'output').exists())
            (assets / 'booter_load_ga102.bin').write_bytes(b'wrong firmware')
            changed = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(changed.returncode, 0)
            self.assertIn('Wrong firmware', changed.stderr)
            self.assertFalse((base / 'output').exists())

    def test_installer_checks_and_refuses_non_fat32(self):
        with tempfile.TemporaryDirectory() as tmp:
            package = Path(tmp)
            script = package / 'Install-USB.ps1'
            script.write_bytes((ROOT / 'Install-USB.ps1').read_bytes())
            files = [script]
            for i in range(7):
                path = package / f'file{i}.txt'
                path.write_text(f'payload {i}', encoding='utf-8')
                files.append(path)
            manifest = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
            manifest_file = package / 'SHA256-MANIFEST.json'
            manifest_file.write_text(json.dumps(manifest), encoding='utf-8')
            command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script), '-VerifyOnly']
            valid = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(valid.returncode, 0, valid.stdout + valid.stderr)
            files[1].write_text('corrupt', encoding='utf-8')
            invalid = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(invalid.returncode, 0)
            self.assertIn('Missing or changed', invalid.stderr)
            files[1].write_text('payload 0', encoding='utf-8')
            manifest['../outside.txt'] = '0' * 64
            manifest_file.write_text(json.dumps(manifest), encoding='utf-8')
            traversal = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(traversal.returncode, 0)
            self.assertIn('Invalid manifest entry', traversal.stderr)
            del manifest['../outside.txt']
            manifest_file.write_text(json.dumps(manifest), encoding='utf-8')
            # Mock storage metadata; refusal happens before any filesystem copy.
            wrapper = package / 'refuse.ps1'
            wrapper.write_text('''function Get-Volume { param($DriveLetter) [pscustomobject]@{FileSystem='NTFS'} }
function Get-Partition { param($DriveLetter) [pscustomobject]@{IsBoot=$false; IsSystem=$false} }
function Get-Disk { [pscustomobject]@{BusType='USB'; IsBoot=$false; IsSystem=$false} }
& "$PSScriptRoot/Install-USB.ps1" -DriveLetter Z
exit $LASTEXITCODE
''', encoding='utf-8')
            refused = subprocess.run(command[:-2] + [str(wrapper)], capture_output=True, text=True)
            self.assertNotEqual(refused.returncode, 0)
            self.assertIn('FAT32 USB volume', refused.stderr)


if __name__ == '__main__':
    unittest.main()
