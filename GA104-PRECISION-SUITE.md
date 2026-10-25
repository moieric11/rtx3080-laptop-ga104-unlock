# Suite de precisions GA104

Reference sans unlock terminee :
evidence/windows-precision-stock-2026-10-24-164155.
Comparaison avec unlock terminee :
evidence/windows-precision-unlocked-2026-10-24-165128.
RUN08 confirme le retour du GOP et SS0=0x88888888 / SS1=8.
Les empreintes executable/source, les versions et les parametres correspondent.
Sur 8192, FP16/BF16 avec accumulation FP32 gagnent environ 71 a 73 %, TF32
78 %, et les chemins FP8 convertis en FP16 environ 68 a 75 %. Les petites
variations FP16-accumulation-FP16, FP32 strict et INT4 ne permettent pas
d'attribuer un gain specifique a l'unlock ; INT8 reste presque identique.
L'avertissement FP16-accumulation-FP16 de 5.754 % apparait identiquement dans
les deux etats. Aucun nouveau probleme numerique n'est observe sur les sondes.
Les anciens benchmarks et leurs executables sont conserves.

Douze scenarios sur chacune des tailles 2048, 4096, 8192 :

- FP16, accumulation FP16, sortie FP16.
- FP16, accumulation FP32, sortie FP32 ou FP16.
- BF16, accumulation FP32, sortie FP32 ou BF16.
- Entrees FP32 avec mode TF32, accumulation/sortie FP32.
- FP32 strict (CUBLAS_COMPUTE_32F_PEDANTIC), accumulation/sortie FP32.
- INT8, accumulation/sortie INT32, deux implementations : cuBLAS et CUTLASS.
- INT4 signe, accumulation/sortie INT32, CUTLASS TensorOp SM80 sur SM86.
- Stockage FP8 E4M3 ou E5M2, conversion vers FP16 puis GEMM avec accumulation
  et sortie FP32. Le temps de conversion GPU des deux matrices est inclus
  a chaque appel, sans conversion fusionnee. Aucun GEMM FP8 natif sur GA104.

Les cas INT4/INT8 sont des calculs sur deux operandes entiers : ils ne
reproduisent pas un modele W4A16, une quantification ConvRot ou un workflow
ComfyUI complet. Ce sont des mesures des implementations selectionnees,
sans autotuning exhaustif. TOPS pour les entiers, TFLOPS pour les flottants.
Le MAC compte pour deux operations ; matrices denses, sans sparsification.

Protocole : chauffe d'une seconde par cas ; cinq mesures de quatre GEMM par
evenements CUDA ; mediane ; alpha=1, beta=0 ; preparation et transferts CPU/GPU
exclus ; horloges et temperatures libres. Les donnees varient dans les deux
dimensions et ont des valeurs exactement representables dans les formats testes.
Neuf sorties sont comparees a une reference CPU avant et apres chronometrage.
Ce controle n'est ni une validation de toute la matrice ni un test de stabilite.

Seuils numeriques : entiers exacts, sortie FP32 0.01 %, BF16 0.5 %, FP16 2.5 %.
Les ecarts superieurs sont marques accuracy_warning ; ils restent visibles,
sans etre convertis en un succes de precision. Un avertissement a ete observe
en accumulation FP16 sur la taille 8192 dans la reference sans unlock.

Sources :
- https://docs.nvidia.com/cuda/cublas/index.html
- https://github.com/NVIDIA/cutlass/tree/ad7b2f5e84fcfa124cb02b91d5bd26d238c0459e

## Refaire apres unlock

Arret complet, demarrer la cle en Discrete, appuyer sur U puis choisir Windows
Boot Manager sans redemarrer. Conserver le meme executable pour la comparaison.
Le label d'etat est fourni par l'operateur, le programme ne lit ni ne modifie
les overrides GPU. Le rapport UEFI et le profil de performances servent de
verification complementaire de l'etat.

```powershell
python -u tools/run_ga104_precision_suite.py --state unlocked --compare evidence/windows-precision-stock-2026-10-24-164155
```

Le script enregistre JSONL brut, erreurs, versions, empreintes, telemetrie,
resultats et ecarts. La comparaison exige les memes empreintes source/executable,
versions CUDA/cuBLAS, identite GPU, parametres et variables CUDA pertinentes.

## Reconstruction

Le clone NVIDIA CUTLASS 3.9.2 est dans analysis/research/cutlass, au commit
ad7b2f5e84fcfa124cb02b91d5bd26d238c0459e (sparse checkout du dossier include).
Build : build-ga104-precision-suite.cmd. CUDA 13.4, cible sm_86, MSVC 18.
Le compilateur hote utilise l'option nvcc allow-unsupported-compiler ; cette
combinaison n'est pas presentee comme une configuration officiellement validee.
Un essai 256 des onze premiers scenarios a reussi ; la suite finale complete
de 36 cas ajoute le second chemin INT8 et les avertissements numeriques.

L'empreinte de la reference est dans build/ga104-precision-suite/BASELINE.json.
Toute reconstruction doit etre comparee a cette empreinte avant de reutiliser
la reference, faute de quoi une nouvelle paire de mesures est necessaire.
