"""Build a complete, hash-verified GA104 USB release without touching a USB."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'unlock'))
from verify_pe import inspect

EFI_SHA256 = '4b58554e18b0a2e987040cf3c006aa122bfd84ccd0f2dbee11cb225da7340000'
VERSION = 'v0.4.1-experimental'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(assets, output):
    if subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=normal'], cwd=ROOT).strip():
        raise ValueError('Commit the reviewed source first; release source must match the executable.')
    efi_dir = ROOT / 'build/ga104-cold-selectors'
    verification = inspect(efi_dir / 'GA104-UNLOCK.EFI', efi_dir / 'unlock.map')
    if verification['sha256'] != EFI_SHA256:
        raise ValueError('EFI differs from the reviewed cold-selector release build')
    specs = json.loads((ROOT / 'unlock/assets.json').read_text(encoding='utf-8'))
    for spec in specs:
        path = assets / spec['Name']
        if path.stat().st_size != spec['Bytes'] or digest(path) != spec['Sha256']:
            raise ValueError('Wrong firmware: ' + spec['Name'])
    # A fresh output prevents stale files from leaking into a release.
    output.mkdir(parents=True, exist_ok=False)
    package = output / 'USB'
    package.mkdir()

    def copy(source, relative):
        dest = package / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
        if digest(source) != digest(dest):
            raise ValueError('Copy hash mismatch: ' + relative)

    copy(efi_dir / 'GA104-UNLOCK.EFI', 'EFI/BOOT/BOOTX64.EFI')
    for spec in specs:
        copy(assets / spec['Name'], 'RTX3080-UNLOCK/ASSETS/' + spec['Name'])
    for name in ('Install-USB.ps1', 'LICENSE', 'THIRD_PARTY_NOTICES.md'):
        copy(ROOT / name, name)
    copy(ROOT / 'docs/INSTALL.md', 'INSTALL.md')
    for path in (ROOT / 'LICENSES').iterdir():
        if path.is_file():
            copy(path, 'LICENSES/' + path.name)
    source_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    subprocess.run(['git', 'archive', '--format=zip', '--output=' + str(package / 'SOURCE.zip'), source_commit], cwd=ROOT, check=True)
    with zipfile.ZipFile(package / 'SOURCE.zip') as source:
        if source.testzip():
            raise ValueError('Invalid source archive')
    (package / 'BUILD.json').write_text(json.dumps({
        'version': VERSION, 'source_commit': source_commit,
        'variant': 'GU_COLD_SELECTOR_VARIANTS', 'verification': verification,
        'firmware_manifest': specs,
        'hardware_scope': 'Lenovo 82N6, 10de:24dc, 17aa:3a58, VBIOS 94.04.3f.00.c5',
        'hardware_boot_of_this_new_archive_tested': False,
    }, indent=2) + '\n', encoding='utf-8')
    manifest = {p.relative_to(package).as_posix(): digest(p)
                for p in sorted(package.rglob('*')) if p.is_file()}
    (package / 'SHA256-MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    archive = output / ('GA104-UNLOCK-USB-' + VERSION + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(package.rglob('*')):
            if path.is_file():
                bundle.write(path, path.relative_to(package).as_posix())
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip():
            raise ValueError('Corrupt release archive')
        for relative, expected in manifest.items():
            if hashlib.sha256(bundle.read(relative)).hexdigest() != expected:
                raise ValueError('Archive mismatch: ' + relative)
    (output / 'SHA256SUMS.txt').write_text(digest(archive) + '  ' + archive.name + '\n', encoding='ascii')
    print(f'Verified complete USB release: {archive} ({archive.stat().st_size} bytes)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    build(args.assets.resolve(), args.output.resolve())
