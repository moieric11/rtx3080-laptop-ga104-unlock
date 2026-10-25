# Suite de precisions GA104

Etat declare : unlocked. 5 mesures de 4 GEMM, chauffe 1 seconde par cas.

| Scenario | 2048 | 4096 | 8192 | Unite |
|---|---:|---:|---:|---|
| FP16_accFP16_outFP16 | 64.10 | 70.51 | 67.04 | TFLOPS |
| FP16_accFP32_outFP32 | 62.31 | 75.35 | 72.80 | TFLOPS |
| FP16_accFP32_outFP16 | 63.19 | 75.41 | 73.56 | TFLOPS |
| BF16_accFP32_outFP32 | 64.84 | 75.51 | 73.41 | TFLOPS |
| BF16_accFP32_outBF16 | 66.77 | 77.88 | 74.37 | TFLOPS |
| TF32_accFP32_outFP32 | 34.66 | 39.17 | 37.88 | TFLOPS |
| FP32_strict_accFP32_outFP32 | 11.22 | 11.56 | 15.12 | TFLOPS |
| INT8_cuBLAS_accINT32_outINT32 | 36.73 | 34.77 | 34.64 | TOPS |
| FP8_E4M3_decodeFP16_accFP32 | 49.82 | 61.25 | 68.73 | TFLOPS |
| FP8_E5M2_decodeFP16_accFP32 | 51.31 | 62.10 | 68.58 | TFLOPS |
| INT4_accINT32_outINT32 | 185.90 | 194.31 | 187.39 | TOPS |
| INT8_CUTLASS_accINT32_outINT32 | 120.70 | 115.36 | 95.63 | TOPS |

FP8 : stockage E4M3/E5M2 puis conversion explicite en FP16 a chaque GEMM, incluse dans le temps. Aucun calcul Tensor FP8 natif.
INT4 : CUTLASS 3.9.2, bloc 128x128x128, warp 64x64x128, instruction 16x8x64 ; pas de recherche du meilleur kernel.
INT8 CUTLASS : bloc 128x128x64, warp 64x64x64, instruction 16x8x32. Le chemin cuBLAS est mesure separement.
Autres cas : cuBLAS. Donnees deterministes variant dans les deux dimensions ; neuf sorties comparees a une reference CPU avant et apres mesure.
FP32 strict utilise COMPUTE_32F_PEDANTIC ; les calculs flottants interdisent les reductions de precision reduite.
Ces chiffres mesurent les implementations selectionnees, pas des pics theoriques. TOPS entiers et TFLOPS flottants ont des significations numeriques differentes.
Matrices denses ; preparation initiale et transferts CPU/GPU exclus du temps. Pour FP8, seule la conversion GPU FP8->FP16 est incluse.
Frequences et temperatures libres. Le label stock/unlocked ne remplace pas une preuve de l etat materiel.

## Ecarts numeriques a examiner

| Scenario | Taille | Erreur relative maximale des neuf sondes |
|---|---:|---:|
| FP16_accFP16_outFP16 | 8192 | 5.754 % |
Seuils : entiers exacts ; FP32 0.01 %, sortie BF16 0.5 %, sortie FP16 2.5 %. Un debit avec avertissement ne prouve pas une precision acceptable pour un modele.

## Comparaison a stock

| Scenario 8192 | Precedent | Actuel | Ecart |
|---|---:|---:|---:|
| FP16_accFP16_outFP16 | 63.16 | 67.04 | +6.1 % |
| FP16_accFP32_outFP32 | 42.61 | 72.80 | +70.9 % |
| FP16_accFP32_outFP16 | 42.80 | 73.56 | +71.9 % |
| BF16_accFP32_outFP32 | 42.84 | 73.41 | +71.4 % |
| BF16_accFP32_outBF16 | 42.88 | 74.37 | +73.4 % |
| TF32_accFP32_outFP32 | 21.33 | 37.88 | +77.6 % |
| FP32_strict_accFP32_outFP32 | 14.75 | 15.12 | +2.5 % |
| INT8_cuBLAS_accINT32_outINT32 | 34.70 | 34.64 | -0.2 % |
| FP8_E4M3_decodeFP16_accFP32 | 40.90 | 68.73 | +68.0 % |
| FP8_E5M2_decodeFP16_accFP32 | 39.21 | 68.58 | +74.9 % |
| INT4_accINT32_outINT32 | 180.12 | 187.39 | +4.0 % |
| INT8_CUTLASS_accINT32_outINT32 | 95.80 | 95.63 | -0.2 % |
