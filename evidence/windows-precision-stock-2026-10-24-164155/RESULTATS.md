# Suite de precisions GA104

Etat declare : stock. 5 mesures de 4 GEMM, chauffe 1 seconde par cas.

| Scenario | 2048 | 4096 | 8192 | Unite |
|---|---:|---:|---:|---|
| FP16_accFP16_outFP16 | 61.29 | 56.00 | 63.16 | TFLOPS |
| FP16_accFP32_outFP32 | 38.63 | 40.72 | 42.61 | TFLOPS |
| FP16_accFP32_outFP16 | 38.79 | 41.02 | 42.80 | TFLOPS |
| BF16_accFP32_outFP32 | 39.08 | 40.09 | 42.84 | TFLOPS |
| BF16_accFP32_outBF16 | 39.31 | 40.76 | 42.88 | TFLOPS |
| TF32_accFP32_outFP32 | 19.43 | 20.42 | 21.33 | TFLOPS |
| FP32_strict_accFP32_outFP32 | 11.77 | 10.47 | 14.75 | TFLOPS |
| INT8_cuBLAS_accINT32_outINT32 | 35.91 | 34.00 | 34.70 | TOPS |
| FP8_E4M3_decodeFP16_accFP32 | 33.77 | 37.36 | 40.90 | TFLOPS |
| FP8_E5M2_decodeFP16_accFP32 | 33.96 | 37.38 | 39.21 | TFLOPS |
| INT4_accINT32_outINT32 | 186.41 | 191.06 | 180.12 | TOPS |
| INT8_CUTLASS_accINT32_outINT32 | 120.70 | 98.89 | 95.80 | TOPS |

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
