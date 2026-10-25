"""Estimate the current GA104 unlock effect using the matched local GEMM runs."""
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMPAT = ("gpu", "runtime", "driver_api", "cublas", "samples", "batch", "warmup_seconds")


def rate(record, n, mode):
    value = next(r["median_tflops"] for r in record["results"]
                 if r["n"] == n and r["mode"] == mode)
    if not math.isfinite(value) or value <= 0:
        raise ValueError("Mesure de performance invalide")
    return value


def classify(records, stock, unlocked):
    # Normalize by FP16 accumulation to reduce sensitivity to clock differences.
    # Require both sizes and every process to agree within 15% of one reference.
    rows = []
    votes = []
    for n in (4096, 8192):
        s = rate(stock, n, "FP32acc") / rate(stock, n, "FP16acc")
        u = rate(unlocked, n, "FP32acc") / rate(unlocked, n, "FP16acc")
        for i, record in enumerate(records, 1):
            ratio = rate(record, n, "FP32acc") / rate(record, n, "FP16acc")
            near_s = abs(ratio / s - 1) <= 0.15
            near_u = abs(ratio / u - 1) <= 0.15
            vote = "stock" if near_s and not near_u else "unlocked" if near_u and not near_s else "unknown"
            votes.append(vote)
            rows.append(dict(run=i, n=n, ratio=ratio, stock_ratio=s, unlocked_ratio=u))
    state = votes[0] if votes and len(set(votes)) == 1 else "unknown"
    return state, rows


def main():
    binary = ROOT / "build/ga104-unlock/ga104-gemm.exe"
    refs = [ROOT / "evidence" / name for name in
            ("windows-baseline-2026-10-23", "windows-after-2026-10-23")]
    summaries = [json.loads((p / "summary.json").read_text(encoding="utf-8")) for p in refs]
    environments = [json.loads((p / "run-1.json").read_text(encoding="utf-8")) for p in refs]
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    if any(s["executable_sha256"] != digest for s in summaries):
        raise ValueError("Executable different des references : comparaison impossible")
    if any(environments[0][k] != environments[1][k] for k in COMPAT):
        raise ValueError("Les environnements des references different")
    dest = ROOT / "evidence" / ("windows-switch-check-" + datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S-%f"))
    dest.mkdir(parents=True)
    print("Verification de l'effet unlock RTX 3080 Laptop (3 passages, environ 30 secondes).", flush=True)
    print("Brancher le secteur et fermer les applications qui utilisent le GPU.", flush=True)
    print("Ce test lance des calculs CUDA ; il ne modifie pas les registres du GPU.", flush=True)
    env = os.environ.copy()
    cuda_bin = Path(r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4\bin\x64")
    env["PATH"] = str(cuda_bin) + os.pathsep + env.get("PATH", "")
    records = []
    for i in range(1, 4):
        result = subprocess.run([str(binary)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
        (dest / f"run-{i}.stdout.txt").write_text(result.stdout, encoding="utf-8")
        (dest / f"run-{i}.stderr.txt").write_text(result.stderr, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(f"Echec CUDA (code {result.returncode}). Voir {dest}\n{result.stderr}")
        record = json.loads(result.stdout)
        records.append(record)
        print(f"Passage {i}/3 termine.", flush=True)
    compatible = all(r[k] == environments[0][k] for r in records for k in COMPAT)
    state, rows = classify(records, *summaries) if compatible else ("unknown", [])
    verdict = {
        "unlocked": "UNLOCK PROBABLEMENT ACTIF : performances conformes a la reference avec unlock.",
        "stock": "PROFIL SANS UNLOCK : performances conformes a la reference stock.",
        "unknown": "INDETERMINE : mesures intermediaires, instables ou environnement different.",
    }[state]
    lines = [verdict, "", "Inference par performances, pas une lecture directe du switch materiel."]
    if not compatible:
        lines.append("GPU, versions CUDA/cuBLAS/pilote ou parametres differents des references.")
    for n in (4096, 8192):
        f32 = statistics.median(rate(r, n, "FP32acc") for r in records)
        f16 = statistics.median(rate(r, n, "FP16acc") for r in records)
        lines.append(f"{n} : FP32acc {f32:.2f} TFLOPS / FP16acc {f16:.2f} TFLOPS")
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), state=state,
                  verdict=verdict, basis="performance inference; no register read", compatible=compatible,
                  executable_sha256=digest, references=[str(p) for p in refs], tolerance=0.15, comparisons=rows)
    (dest / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (dest / "RESULTAT.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n" + "\n".join(lines))
    print(f"\nRapport : {dest}")
    return {"unlocked": 0, "stock": 1, "unknown": 2}[state]


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, StopIteration, RuntimeError, subprocess.TimeoutExpired) as exc:
        print(f"\nINDETERMINE - verification impossible : {exc}", flush=True)
        raise SystemExit(2)
