"""Export reviewed source/evidence only, without history or private binary assets."""
import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / ("public-source-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
ALLOWED = {".c", ".h", ".inc", ".cu", ".cpp", ".py", ".cmd", ".ps1", ".md", ".json", ".txt"}
LOCAL_TOOLS = {"recover_linux_usb.py", "download_fasth3_omniref.py", "download_qwen3_4b.py",
               "qwen3_4b_bench.py", "build_ga104_register_catalog.py"}
EVIDENCE = (
    "lenovo-ga104-boot01", "lenovo-ga104-boot02", "lenovo-ga104-handoff01",
    "lenovo-ga104-unlock01", "lenovo-ga104-unlock-run08",
    "lenovo-ga104-cold-selectors-success01", "windows-baseline-2026-10-23",
    "windows-after-2026-10-23", "windows-precision-stock-2026-10-24-164155",
    "windows-precision-unlocked-2026-10-24-165128",
    "usb-boot-2026-10-27",
)
SECRET = re.compile(r"(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|"
                    r"hf_[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)")


def main():
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    selected = {ROOT / p for p in tracked if p}
    for path in ROOT.iterdir():
        if path.is_file() and path.suffix.lower() in ALLOWED and path.name != "QWEN3-4B-BENCH.md":
            selected.add(path)
    for name in ("tools", "unlock", "docs"):
        for path in (ROOT / name).rglob("*"):
            parts = path.relative_to(ROOT / name).parts
            if (path.is_file() and path.suffix.lower() in ALLOWED
                    and not {"build", "__pycache__", "releases"}.intersection(parts)
                    and path.name not in LOCAL_TOOLS):
                selected.add(path)
    for name in EVIDENCE:
        for path in (ROOT / "evidence" / name).rglob("*"):
            # Raw report text and JSON only; no XML hardware inventory or dumps.
            if path.is_file() and path.suffix.lower() in {".txt", ".json", ".jsonl", ".md", ".csv"}:
                selected.add(path)
    files = {}
    # Validate every file before creating the export. Never silently publish a secret.
    for path in sorted(selected):
        relative = path.relative_to(ROOT).as_posix()
        original = path.read_bytes()
        text = original.decode("utf-8-sig")
        if SECRET.search(text):
            raise ValueError(f"Possible credential in {relative}; export stopped")
        # Source logic is kept intact. Sanitize only published measurement text.
        if relative.startswith("evidence/"):
            text = re.sub(r"GPU-[0-9a-fA-F-]{16,}", "GPU-REDACTED", text)
            text = re.sub(r"(?i)([A-Z]:[/\\]+Users[/\\]+)[^/\\\s\"']+", r"\1REDACTED", text)
            text = re.sub(r"/home/[^/\s\"']+", "/home/REDACTED", text)
        files[relative] = text.encode("utf-8") if relative.startswith("evidence/") else original
    # Fail instead of merging with a previous export or accidentally carrying files over.
    OUT.mkdir(parents=True, exist_ok=False)
    manifest = {}
    for relative, data in files.items():
        dest = OUT / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        manifest[relative] = hashlib.sha256(data).hexdigest()
    (OUT / "PUBLIC-SOURCE-MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    archive = ROOT / "build/rtx3080-laptop-ga104-unlock-source.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(OUT.rglob("*")):
            if path.is_file():
                bundle.write(path, path.relative_to(OUT).as_posix())
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None:
            raise ValueError("Corrupt ZIP")
        for relative, digest in manifest.items():
            if hashlib.sha256(bundle.read(relative)).hexdigest() != digest:
                raise ValueError(f"Archive mismatch: {relative}")
    print(f"Verified {len(files)} source/evidence files: {archive}")
    print("No Git history, firmware/VBIOS binaries, EFI, model weights or local environments included.")


if __name__ == "__main__":
    main()
