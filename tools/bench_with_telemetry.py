"""Run the unchanged GEMM benchmark while sampling normal NVML telemetry."""
import argparse
import csv
import datetime
import json
import os
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser()
p.add_argument("--label-prefix",default="power-unlocked")
p.add_argument("--runs",type=int,default=3)
p.add_argument("--limit-reasons",action="store_true")
a=p.parse_args()
if not a.label_prefix.replace("-","").isalnum():raise ValueError("Invalid label")
root=Path(__file__).resolve().parents[1]
label=a.label_prefix+"-"+datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S")
temporary=root/"build/ga104-unlock"/(label+"-telemetry.csv")
dest=root/"evidence"/("windows-"+label)
fields="timestamp,power.draw,temperature.gpu,clocks.sm,utilization.gpu,power.limit"
columns=["timestamp","power_w","temperature_c","sm_clock_mhz","gpu_util_percent","requested_power_limit_w"]
extra=[("instant_power_w","power.draw.instant"),("average_power_w","power.draw.average"),
       ("current_power_limit_w","gpu.power.current_limit"),
       ("sw_power_cap","clocks_event_reasons.sw_power_cap"),
       ("hw_power_brake","clocks_event_reasons.hw_power_brake_slowdown"),
       ("hw_thermal","clocks_event_reasons.hw_thermal_slowdown"),
       ("sw_thermal","clocks_event_reasons.sw_thermal_slowdown"),
       ("board_limit","clocks_event_reasons.board_limit"),
       ("reliability","clocks_event_reasons.reliability"),
       ("sw_power_cap_us","clocks_event_reasons_counters.sw_power_cap")]
if a.limit_reasons:
    columns.extend(name for name,_ in extra)
    fields+=","+",".join(query for _,query in extra)
subprocess.run(["nvidia-smi","--query-gpu="+fields,"--format=csv,noheader,nounits"],stdout=subprocess.DEVNULL,check=True)
with temporary.open("x",newline="",encoding="utf-8") as log:
    monitor=subprocess.Popen(["nvidia-smi","--query-gpu="+fields,"--format=csv,noheader,nounits","--loop-ms=500"],
        stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
    try:
        result=subprocess.run([sys.executable,"-u",str(root/"tools/run_ga104_benchmark.py"),"--label",label,"--runs",str(a.runs)],cwd=root)
    finally:
        monitor.terminate()
        try:monitor.wait(timeout=5)
        except subprocess.TimeoutExpired:monitor.kill();monitor.wait()
if dest.exists():
    data=temporary.read_text(encoding="utf-8")
    dest.joinpath("telemetry.csv").write_text(",".join(columns)+"\n"+data,encoding="utf-8")
    rows=list(csv.reader(data.splitlines()))
    def values(i):
        output=[]
        for row in rows:
            if len(row)<=i:continue
            try:output.append(float(row[i].strip()))
            except ValueError:pass
        return output
    telemetry={"sample_interval_ms":500,"samples":len(rows)}
    for name,i in [("power_w",1),("temperature_c",2),("sm_clock_mhz",3),("gpu_util_percent",4)]:
        v=values(i)
        if v:telemetry[name]={"min":min(v),"max":max(v)}
    if a.limit_reasons:
        for name,_ in extra:
            i=columns.index(name);v=values(i)
            if v:telemetry[name]={"min":min(v),"max":max(v)}
            else:telemetry[name]={"active_samples":sum(1 for r in rows if len(r)>i and r[i].strip()=="Active")}
    dest.joinpath("telemetry-summary.json").write_text(json.dumps(telemetry,indent=2),encoding="utf-8")
    print("Evidence:",dest,flush=True)
    print(json.dumps(telemetry),flush=True)
if result.returncode:raise SystemExit(result.returncode)
