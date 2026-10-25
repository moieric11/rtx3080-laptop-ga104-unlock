# Premier unlock GA104 validé sous Windows

Même exécutable, mêmes versions CUDA/cuBLAS, médiane de trois processus par condition.

| Matrice | Mode | Avant TFLOPS | Après TFLOPS | Gain |
|---|---|---:|---:|---:|
| 2048² | FP32acc | 38.50 | 58.20 | +51.2 % |
| 2048² | FP16acc | 60.13 | 60.30 | +0.3 % |
| 4096² | FP32acc | 40.40 | 71.80 | +77.7 % |
| 4096² | FP16acc | 66.12 | 67.18 | +1.6 % |
| 8192² | FP32acc | 42.26 | 73.04 | +72.8 % |
| 8192² | FP16acc | 67.03 | 66.34 | -1.0 % |

Le mode FP32acc utilise des entrées FP16 et une accumulation FP32.
Ce résultat concerne un boot et ce benchmark GEMM. Veille, redémarrage et stabilité prolongée restent à tester.

Journal UEFI : ../lenovo-ga104-unlock01/REPORT.TXT. Comparaison brute : comparison.json.
