"""Record the extended precision suite and optionally compare a matching run."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--state', choices=['stock', 'unlocked', 'unspecified'], required=True)
p.add_argument('--compare', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
dest = root / 'evidence' / ('windows-precision-' + a.state + '-' + datetime.datetime.now().strftime('%Y-%m-%d-%H%M%S'))
dest.mkdir(exist_ok=False)
binary = root / 'build/ga104-precision-suite/ga104-precision-suite.exe'
env = os.environ.copy()
env['PATH'] = r'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4\bin\x64' + os.pathsep + env['PATH']
fields = 'timestamp,power.draw,power.draw.instant,temperature.gpu,clocks.sm,utilization.gpu,gpu.power.current_limit,clocks_event_reasons.sw_power_cap,clocks_event_reasons.hw_thermal_slowdown'
query = ['nvidia-smi', '--query-gpu=' + fields, '--format=csv,nounits']
identity = subprocess.run(['nvidia-smi', '--query-gpu=name,pci.device_id,vbios_version,driver_version', '--format=csv'], capture_output=True, text=True, check=True).stdout
records = []
print('Evidence:', dest, flush=True)
with (dest / 'telemetry.csv').open('w', encoding='utf-8') as telemetry, (dest / 'stderr.txt').open('w', encoding='utf-8') as errors, (dest / 'raw.jsonl').open('w', encoding='utf-8') as raw:
    monitor = subprocess.Popen(query + ['--loop-ms=500'], stdout=telemetry, stderr=subprocess.STDOUT, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    try:
        process = subprocess.Popen([str(binary)], cwd=root, env=env, stdout=subprocess.PIPE, stderr=errors, text=True)
        for line in process.stdout:
            raw.write(line)
            raw.flush()
            record = json.loads(line)
            records.append(record)
            if 'scenario' in record:
                print(f"{record['n']} {record['scenario']}: {record['median_rate']:.2f} {record['unit']}", flush=True)
        code = process.wait()
    finally:
        monitor.terminate()
        try:
            monitor.wait(timeout=5)
        except subprocess.TimeoutExpired:
            monitor.kill()
            monitor.wait()
metadata = records[0] if records else {}
summary = {'state': a.state, 'state_basis': 'operator-supplied; no GPU register writes or override reads by this tool',
           'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'executable_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
           'source_sha256': hashlib.sha256((root / 'tools/ga104_precision_suite.cu').read_bytes()).hexdigest(),
           'gpu_identity': identity, 'metadata': metadata, 'exit_code': code, 'complete': code == 0 and len(records) == 37,
           'cuda_overrides': {k: env.get(k) for k in ['NVIDIA_TF32_OVERRIDE', 'CUDA_TF32_OVERRIDE', 'CUBLAS_WORKSPACE_CONFIG']},
           'results': records[1:]}
summary['accuracy_warnings'] = [r for r in records[1:] if r['status'] != 'passed']
(dest / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
if not summary['complete']:
    raise RuntimeError(f'Incomplete suite: {dest}; inspect stderr.txt. Do not treat as a completed baseline.')
lines = ['# Suite de precisions GA104', '', f"Etat declare : {a.state}. 5 mesures de 4 GEMM, chauffe 1 seconde par cas.", '',
         '| Scenario | 2048 | 4096 | 8192 | Unite |', '|---|---:|---:|---:|---|']
for name in dict.fromkeys(r['scenario'] for r in summary['results']):
    rows = [r for r in summary['results'] if r['scenario'] == name]
    lines.append('| ' + name + ' | ' + ' | '.join(f"{r['median_rate']:.2f}" for r in rows) + ' | ' + rows[0]['unit'] + ' |')
lines += ['', 'FP8 : stockage E4M3/E5M2 puis conversion explicite en FP16 a chaque GEMM, incluse dans le temps. Aucun calcul Tensor FP8 natif.',
          'INT4 : CUTLASS 3.9.2, bloc 128x128x128, warp 64x64x128, instruction 16x8x64 ; pas de recherche du meilleur kernel.',
          'INT8 CUTLASS : bloc 128x128x64, warp 64x64x64, instruction 16x8x32. Le chemin cuBLAS est mesure separement.',
          'Autres cas : cuBLAS. Donnees deterministes variant dans les deux dimensions ; neuf sorties comparees a une reference CPU avant et apres mesure.',
          'FP32 strict utilise COMPUTE_32F_PEDANTIC ; les calculs flottants interdisent les reductions de precision reduite.',
          'Ces chiffres mesurent les implementations selectionnees, pas des pics theoriques. TOPS entiers et TFLOPS flottants ont des significations numeriques differentes.',
          'Matrices denses ; preparation initiale et transferts CPU/GPU exclus du temps. Pour FP8, seule la conversion GPU FP8->FP16 est incluse.',
          'Frequences et temperatures libres. Le label stock/unlocked ne remplace pas une preuve de l etat materiel.']
if summary['accuracy_warnings']:
    lines += ['', '## Ecarts numeriques a examiner', '', '| Scenario | Taille | Erreur relative maximale des neuf sondes |', '|---|---:|---:|']
    for r in summary['accuracy_warnings']:
        lines.append(f"| {r['scenario']} | {r['n']} | {100*r['max_relative_error']:.3f} % |")
    lines.append('Seuils : entiers exacts ; FP32 0.01 %, sortie BF16 0.5 %, sortie FP16 2.5 %. Un debit avec avertissement ne prouve pas une precision acceptable pour un modele.')
if a.compare:
    previous = json.loads((a.compare / 'summary.json').read_text())
    for key in ['executable_sha256', 'source_sha256', 'gpu_identity', 'metadata', 'cuda_overrides']:
        if previous[key] != summary[key]:
            raise RuntimeError('Comparison refused, mismatch: ' + key)
    if not previous['complete'] or previous['state'] == summary['state']:
        raise RuntimeError('Comparison requires completed runs with distinct state labels')
    comparisons = []
    for r in summary['results']:
        old = next(x for x in previous['results'] if x['n'] == r['n'] and x['scenario'] == r['scenario'])
        comparisons.append({'n': r['n'], 'scenario': r['scenario'], 'previous': old['median_rate'], 'current': r['median_rate'], 'unit': r['unit'], 'change_percent': 100 * (r['median_rate'] / old['median_rate'] - 1)})
    (dest / 'comparison.json').write_text(json.dumps({'previous_state': previous['state'], 'current_state': a.state, 'previous_path': str(a.compare), 'results': comparisons}, indent=2) + '\n')
    lines += ['', '## Comparaison a ' + previous['state'], '', '| Scenario 8192 | Precedent | Actuel | Ecart |', '|---|---:|---:|---:|']
    for r in comparisons:
        if r['n'] == 8192:
            lines.append(f"| {r['scenario']} | {r['previous']:.2f} | {r['current']:.2f} | {r['change_percent']:+.1f} % |")
(dest / 'RESULTATS.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('Complete:', dest, flush=True)
