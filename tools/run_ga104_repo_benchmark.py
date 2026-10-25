"""Record an independent run matching the published 3090 Ti test parameters."""
import datetime
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument("--state", choices=["unlocked", "stock", "unspecified"], default="unspecified")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
stamp = datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S")
dest = root / "evidence" / ("windows-repo-protocol-" + stamp)
dest.mkdir(exist_ok=False)
binary = root / "build/ga104-repo-benchmark/ga104-repo-gemm.exe"
commit = "3422a3a833e8f7bb05ff25423542763d5701b3ca"
url = f"https://raw.githubusercontent.com/applepines/gpu-unlock/{commit}/docs/rtx3090ti-validation.md"
request = urllib.request.Request(url, headers={"User-Agent": "ga104-benchmark-comparison"})
with urllib.request.urlopen(request, timeout=30) as response:
    source = response.read()
(dest / "reference-validation.md").write_bytes(source)
fields = "timestamp,power.draw,power.draw.instant,temperature.gpu,clocks.sm,utilization.gpu,gpu.power.current_limit,clocks_event_reasons.sw_power_cap,clocks_event_reasons.hw_thermal_slowdown"
query = ["nvidia-smi", "--query-gpu=" + fields, "--format=csv,nounits"]
before = subprocess.run(query, capture_output=True, text=True, check=True).stdout
env = os.environ.copy()
env["PATH"] = r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4\bin\x64" + os.pathsep + env["PATH"]
with (dest / "telemetry.csv").open("w", encoding="utf-8") as log:
    monitor = subprocess.Popen(query + ["--loop-ms=500"], stdout=log, stderr=subprocess.STDOUT,
                               creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    try:
        result = subprocess.run([str(binary)], cwd=root, env=env, text=True, capture_output=True)
    finally:
        monitor.terminate()
        try:
            monitor.wait(timeout=5)
        except subprocess.TimeoutExpired:
            monitor.kill()
            monitor.wait()
(dest / "stdout.json").write_text(result.stdout, encoding="utf-8")
(dest / "stderr.txt").write_text(result.stderr, encoding="utf-8")
if result.returncode:
    raise RuntimeError(f"Benchmark failed: {dest}; {result.stderr}")
data = json.loads(result.stdout)
data.update({"utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
             "executable_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
             "source_sha256": hashlib.sha256((root / "tools/ga104_repo_gemm.cpp").read_bytes()).hexdigest(),
             "reference_url": url, "reference_commit": commit,
             "reference_source_sha256": hashlib.sha256(source).hexdigest(),
             "exact_original_benchmark_source_available": False,
             "extra_protocol_choices": "NN; alpha=1 beta=0; ten GEMMs/sample; 1s warmup/mode; disallow reduced-precision reductions; constant exactly representable inputs; four output probes",
             "gpu_before": before,
             "gpu_after": subprocess.run(query, capture_output=True, text=True, check=True).stdout,
             "unlock_state": args.state,
             "unlock_state_basis": "operator-supplied label; this tool does not read GPU override registers",
             "matching_pre_unlock_baseline_available": False})
(dest / "summary.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
lines = ["# Published-parameter GEMM comparison", "",
         "8192 x 8192 dense cuBLAS; FP32 accumulation/output; median of five event-timed samples.",
         "Reference: " + url, "",
         "The original benchmark source is absent from the referenced tree. Additional choices are recorded in summary.json.",
         "Windows and Linux/cuBLAS versions, GPU and power conditions differ. This is not an exact reproduction.", "",
         "| Input | RTX 3080 Laptop current TFLOPS | Reference 3090 Ti unlocked TFLOPS |", "|---|---:|---:|"]
reference = {"FP16": 118.39, "BF16": 157.41, "TF32": 82.61}
for row in data["results"]:
    lines.append(f"| {row['input']} | {row['median_tflops']:.2f} | {reference[row['input']]:.2f} |")
lines += ["", "No matched pre-unlock run exists for this new executable; do not derive an unlock gain from old benchmark results.",
          "Four output probes are basic correctness checks, not full matrix validation or a stability test."]
(dest / "RESULTATS.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
print(dest)
for row in data["results"]:
    print(f"{row['input']}: {row['median_tflops']:.2f} TFLOPS; samples={row['samples_tflops']}; relative error={row['max_sampled_relative_error']}")
