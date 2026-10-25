# ECC : première observation sur la GA104 Lenovo

Cette variante observe un seul registre supplémentaire, NV_FUSE_FEATURE_READOUT
à 0x00823814, avant l'unlock de calcul et après le retour du GOP. Elle ne tente
pas d'activer l'ECC. Le bit 16 est enregistré comme enabled_readout ; cette valeur
ne prouve ni le support complet, ni la correction effective d'erreurs mémoire.

## Correspondance dans les sources NVIDIA

Source figée : open-gpu-kernel-modules 615.78.08, commit
ca2d03e6fafd4d4b56b383497d931c25b283bfcf.

1. src/nvidia/generated/g_kern_mem_sys_nvoc.c, lignes 881–888, sélectionne
   kmemsysCheckReadoutEccEnablement_GA100 pour la famille comprenant explicitement GA104.
2. src/nvidia/src/kernel/gpu/mem_sys/arch/ampere/kern_mem_sys_ga100.c, lignes 479–486,
   lit NV_FUSE_FEATURE_READOUT et teste le champ ECC_DRAM.
3. src/common/inc/swref/published/ampere/ga100/dev_fuse.h, lignes 26–28, définit
   l'adresse 0x00823814, le champ 16:16 et la valeur ENABLED=1.
4. kernel_gsp.c utilise cette lecture dans _kgspBootGspRm avant le bootstrap GSP.

Le nom GA100 de la fonction ne limitait donc pas son utilisation à GA100. Cette
chaîne de sélection confirme la correspondance GA104 pour la lecture retenue.
Elle ne valide pas les écritures ECC du CMP170HX, ni leurs prérequis sur ce portable.
Le pilote Windows installé est 617.14, distinct de cette source Linux publiée.

## Ce qui change

- ecc_readout.c vérifie l'identité 10de:24dc / 17aa:3a58, la position PCI,
  l'activation mémoire et BAR0 avant une lecture 32 bits bornée.
- Les sentinelles ffffffff / badfxxxx et les erreurs restent non interprétées.
- Deux lignes ECC_READOUT sont ajoutées au rapport de l'application existante.
- Aucun override ECC, réglage de géométrie, mode ECC ou fuse n'est écrit.
- La touche U effectue toujours l'unlock de calcul déjà utilisé, puis reconnecte
  l'affichage. L'ECC est seulement observé ; U n'est pas une activation ECC.
- L'EFI original validé demeure dans build/ga104-unlock et est conservé sur la clé
  sous EFI/TOOLS/GA104-UNLOCK-VALIDE.EFI. Son SHA256 est
  5e5b99107f6ddf38fdda50a7995b60cb3c335742cfa6f7d8df4f519fd3c353fa.

## Essai

La clé FAT32 contient la variante ECC comme démarrage par défaut. Démarrer dessus
en Discrete, puis appuyer sur U pour conserver aussi l'unlock de calcul sur ce boot.
Attendre le retour de l'affichage, puis choisir Windows Boot Manager sans redémarrer.
Le journal reste RTX3080-UNLOCK/RUNxx/REPORT.TXT ; RUN02 est le prochain numéro
attendu, mais il faut lire le dernier rapport si une annulation crée un autre RUN.

Les consignes de l'essai de calcul restent applicables : ne pas retirer la clé ;
si STOP apparaît ou si l'écran reste noir plus de trois minutes, arrêter avec le
bouton physique. Aucun essai d'activation ECC ne sera déduit automatiquement du bit.

Pour remettre l'EFI de calcul validé : depuis Windows, exécuter
RESTAURER-UNLOCK-VALIDE.cmd à la racine de la clé. Les anciens rapports sont conservés.

## Vérification locale

test-ecc-readout.cmd : 16 cas, incluant identités erronées, limites BAR0, sentinelles,
erreurs de lecture et absence d'écriture dans le collecteur. L'EFI est compilé
séparément par python tools/build_ga104_unlock.py --ecc-readout, puis vérifié en PE.
Les essais simulés ne constituent pas encore une observation matérielle du bit ECC.

[Sélection GA104 dans les sources NVIDIA](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/ca2d03e6fafd4d4b56b383497d931c25b283bfcf/src/nvidia/generated/g_kern_mem_sys_nvoc.c#L881).
