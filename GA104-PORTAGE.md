# RTX 3080 Laptop GA104 : état du portage Windows

23 octobre 2026. Premier unlock réel et gain sous Windows validés sur ce portable.
Le journal UEFI confirme une transaction et un FLR, un nettoyage terminé, le retour
de l'affichage et SS0=88888888 / SS1=8 après reconnexion. Le même benchmark sous
Windows mesure +51,2 %, +77,7 % et +72,8 % en FP16 avec accumulation FP32, pour
2048 / 4096 / 8192. FP16acc contrôle varie entre −1,0 % et +1,6 %.
Voir evidence/windows-after-2026-10-23/RESULTATS.md et comparison.json.
Un seul boot est validé ; la stabilité prolongée, la veille et la reprise restent
à tester. Le gain n'est pas une mesure de performances générales ou de jeu.

## Matériel ciblé

Recherche ECC en cours : une variante distincte observe FEATURE_READOUT avant et
après l'unlock de calcul. Voir GA104-ECC-READOUT.md. Elle n'active pas l'ECC.
L'EFI de calcul validé est conservé séparément ; la variante est sur la clé pour
le prochain relevé, avec RESTAURER-UNLOCK-VALIDE.cmd pour revenir à cet EFI.

Lenovo 82N6, Ryzen 9 5900HX, mode Discrete. RTX 3080 Laptop 16 GiB, PCI 10de:24dc,
subsystem 17aa:3a58, PMC_BOOT_0 b74000a1, VBIOS 94.04.3f.00.c5, pilote 617.14.
BAR0 d0000000 / 16 MiB, framebuffer f800000000, dalle 2560×1600.
Ce profil ne représente pas toutes les RTX 3080 Laptop.

## Résultats acquis

- Linux : l'utilisateur rapporte 36,92 → 57,55 TFLOPS en GEMM 8192 / accumulation
  FP32, environ 115 W. La clé Linux est intacte. Module, firmwares, configuration
  et dépôt copiés en lecture seule dans linux-usb-evidence, avec manifeste SHA256.
  Les sources exactes du module binaire Linux n'ont pas été identifiées.
- Sondes v0.1/v0.2 : evidence/lenovo-ga104-boot01 et boot02. PROM, ROMIMAGE,
  configuration et registres sont identiques. Le VBIOS passe le parseur FWSEC et
  les cinq firmwares correspondent aux tailles et SHA256 du port public.
- Test T v0.3 : evidence/lenovo-ga104-handoff01, récupéré de la clé RUN02.
  DisconnectController, ouverture/fermeture exclusive PciIo, ConnectController,
  retour du GOP et de la console : tous EFI_SUCCESS. L'utilisateur a observé le
  retour de l'écran après 5–6 secondes. Ce test n'effectuait ni unlock ni FLR.
- Baseline Windows : evidence/windows-baseline-2026-10-23/summary.json.
  Trois processus, médiane de leurs médianes. FP32acc : 38,50 / 40,40 / 42,26 TFLOPS
  pour 2048 / 4096 / 8192. FP16acc contrôle : 60,13 / 66,12 / 67,03 TFLOPS.
  Ces mesures Windows constituent la référence avant unlock ; elles ne sont pas
  directement comparables au benchmark Linux dont la méthode exacte manque.

## Version expérimentale v0.4

ga104_unlock_app.c orchestre le profil GA104, les contrôles PCI/ROM/firmwares et
la transition discrete_handoff.c. Le cœur GPL du port public reste utilisé.
Le pilote d'affichage est détaché, puis la transaction fonctionne sous un accès
PciIo exclusif. Les journaux continuent uniquement sur USB lorsque l'écran est
détaché. Le cœur exécute la préparation firmware, la modification des sélecteurs,
la restauration de signature et un FLR. Aucun flash VBIOS ni changement NVRAM.

Les buffers DMA et l'accès exclusif sont conservés en cas de nettoyage non prouvé.
Dans ce cas, aucune reconnexion n'est tentée et le programme attend un arrêt
physique. Si le nettoyage est confirmé, le pilote UEFI d'origine est reconnecté.
Le programme vérifie le GOP et la console, puis relit SS0/SS1 : ils doivent encore
valoir 88888888 et 8. Un retour à ces valeurs n'établit pas à lui seul un gain CUDA.
La conservation lors du chargement du pilote Windows reste à mesurer.

La reprise d'affichage après le FLR a réussi lors du premier essai U, journal
evidence/lenovo-ga104-unlock01/REPORT.TXT, issu de RTX3080-UNLOCK/RUN01.
RUN00 avait expiré sans tentative. Veille et reprise ne sont pas prises en charge.
Un redémarrage ou une coupure d'alimentation peut annuler les sélecteurs.

## Compilation et tests

- python tools/build_ga104_unlock.py : LLVM 21 dans WSL, EFI x64 sans imports,
  relocations, NX/ASLR et point d'entrée contrôlés par unlock/verify_pe.py.
- python tools/test_ga104_port.py : 261 contrôles GA104, 261 contrôles du profil
  MSI, 20 scénarios du cœur + compatibilité UEFI et 20 du callback réel de l'app.
- test-discrete-handoff.cmd : 21 scénarios de transition, cinq scénarios du
  callback générique (dont rétention sans reconnexion) et confirmation clavier.
- test-ga104-probe.cmd : collecte et sauvegarde, erreurs et absence d'écriture GPU.

Ces tests simulent le matériel et l'exécution des firmwares. Ils ne certifient
pas la réussite du prochain boot. Le profil actif nécessite explicitement le
drapeau GU_DISCRETE_EXPERIMENTAL ; il n'est pas activé par le build MSI public.

## Prochain test sur la clé FAT32

Le paquet actif est build/ga104-unlock/GA104-UNLOCK-USB.zip. Démarrer la clé en
UEFI et rester en Discrete. Après les contrôles, appuyer sur U pour une tentative.
Esc ou 30 secondes annule. Pendant l'essai, conserver la clé branchée. Si l'écran
reste noir plus de trois minutes, éteindre avec le bouton physique. En présence
d'un message STOP, éteindre également et ne pas continuer vers Windows.

En cas de réussite, choisir Windows Boot Manager dans la même session firmware,
sans redémarrage intermédiaire. Revenir dans la conversation avec la clé branchée.
Le nouveau journal est RTX3080-UNLOCK/RUN00/REPORT.TXT (dossier distinct des sondes).
Les anciens RTX3080-PROBE/RUN00, RUN01 et RUN02 sont conservés.

Pour rétablir le test T, utiliser RESTAURER-TEST-T.cmd à la racine de la clé depuis
Windows. Le démarrage Windows installé et la clé Linux n'ont pas été modifiés.

## Mesure après essai

Depuis le projet : python tools/run_ga104_benchmark.py --label after-2026-10-23.
Comparer avec la baseline en vérifiant l'empreinte du même exécutable, les versions
CUDA/cuBLAS et des conditions de puissance/température comparables. Le benchmark
utilise cuBLAS GemmEx, FP16 en entrée, accumulation FP32 et contrôle FP16, sans
réduction à précision inférieure pour l'accumulation FP32. Première et dernière
sortie vérifiées. Documentation : https://docs.nvidia.com/cuda/cublas/index.html.

Sources : https://github.com/Zagitta/gpu-unlock et
https://github.com/rough2515-hub/rtx3080-unlock-public.
