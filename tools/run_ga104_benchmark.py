"""Record Windows GEMM measurements with the same binary before/after the boot test."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess

p=argparse.ArgumentParser()
p.add_argument("--label",required=True)
p.add_argument("--runs",type=int,default=3)
a=p.parse_args()
if not a.label.replace("-","").replace("_","").isalnum() or not 1<=a.runs<=5: raise ValueError("Invalid run arguments")
root=Path(__file__).resolve().parents[1]
binary=root/"build/ga104-unlock/ga104-gemm.exe"
dest=root/"evidence"/("windows-"+a.label)
dest.mkdir(parents=True,exist_ok=False)
env=os.environ.copy()
env["PATH"]=r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4\bin\x64"+os.pathsep+env["PATH"]
def gpu_status():
    return subprocess.run(["nvidia-smi","--query-gpu=name,pci.device_id,vbios_version,driver_version,power.draw,temperature.gpu,clocks.sm,utilization.gpu","--format=csv"],capture_output=True,text=True,check=True).stdout
metadata={"utc":datetime.datetime.now(datetime.timezone.utc).isoformat(),"executable_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),"gpu_before":gpu_status()}
records=[]
for i in range(a.runs):
    result=subprocess.run([str(binary)],env=env,cwd=root,text=True,capture_output=True)
    if result.returncode:
        dest.joinpath("error.txt").write_text(result.stdout+result.stderr,encoding="utf-8")
        raise RuntimeError("Benchmark failed; see "+str(dest/"error.txt"))
    record=json.loads(result.stdout)
    dest.joinpath(f"run-{i+1}.json").write_text(json.dumps(record,indent=2),encoding="utf-8")
    records.append(record)
    print(f"Windows benchmark {i+1}/{a.runs} complete",flush=True)
metadata["gpu_after"]=gpu_status()
summary=[]
for row in records[0]["results"]:
    values=[next(x["median_tflops"] for x in r["results"] if x["n"]==row["n"] and x["mode"]==row["mode"]) for r in records]
    summary.append({"n":row["n"],"mode":row["mode"],"median_tflops":statistics.median(values)})
metadata["results"]=summary
dest.joinpath("summary.json").write_text(json.dumps(metadata,indent=2),encoding="utf-8")
for row in summary:print(f"{row['n']} {row['mode']}: {row['median_tflops']:.2f} TFLOPS")
