"""Compare the saved Windows runs, rejecting mismatched binaries or libraries."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
before=root/"evidence/windows-baseline-2026-10-23"
after=root/"evidence/windows-after-2026-10-23"
a=json.loads((before/"summary.json").read_text())
b=json.loads((after/"summary.json").read_text())
if a["executable_sha256"]!=b["executable_sha256"]:raise ValueError("Different benchmark executable")
for i in range(1,4):
    x=json.loads((before/f"run-{i}.json").read_text());y=json.loads((after/f"run-{i}.json").read_text())
    for k in ("gpu","runtime","driver_api","cublas","samples","batch","warmup_seconds"):
        if x[k]!=y[k]:raise ValueError("Benchmark environment changed: "+k)
rows=[]
for x in a["results"]:
    y=next(y for y in b["results"] if y["n"]==x["n"] and y["mode"]==x["mode"])
    rows.append({"n":x["n"],"mode":x["mode"],"before_tflops":x["median_tflops"],"after_tflops":y["median_tflops"],
                 "gain_percent":100*(y["median_tflops"]/x["median_tflops"]-1)})
out={"same_executable_sha256":a["executable_sha256"],"same_cuda_cublas":True,"processes_per_condition":3,
     "results":rows,"limitation":"One successful boot; GEMM microbenchmark only; suspend, reboot and long-term stability not validated."}
(after/"comparison.json").write_text(json.dumps(out,indent=2),encoding="utf-8")
text=["# Premier unlock GA104 validé sous Windows","", "Même exécutable, mêmes versions CUDA/cuBLAS, médiane de trois processus par condition.","",
      "| Matrice | Mode | Avant TFLOPS | Après TFLOPS | Gain |","|---|---|---:|---:|---:|"]
for r in rows:
    line=f"| {r['n']}² | {r['mode']} | {r['before_tflops']:.2f} | {r['after_tflops']:.2f} | {r['gain_percent']:+.1f} % |"
    text.append(line);print(line)
text.extend(["","Le mode FP32acc utilise des entrées FP16 et une accumulation FP32.","Ce résultat concerne un boot et ce benchmark GEMM. Veille, redémarrage et stabilité prolongée restent à tester.",
             "","Journal UEFI : ../lenovo-ga104-unlock01/REPORT.TXT. Comparaison brute : comparison.json."])
(after/"RESULTATS.md").write_text("\n".join(text)+"\n",encoding="utf-8")
