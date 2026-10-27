# Correction du refus avant U

## Version 2 : masque des overrides (27 octobre 2026)

RUN14 avait SS0=0x61014101 ; RUN15 et RUN16 avaient SS0=0x60004100.
Ces deux valeurs etaient refusees par la liste de trois captures v1 alors que
les autres controles initiaux passaient. Les rapports sont conserves dans
evidence/usb-boot-2026-10-27.

La nouvelle variante GU_COLD_SELECTOR_MASK accepte un SS0 non nul si
(SS0 & 0x88888888) == 0. Chaque chiffre hexadecimal contient un bit
d'activation d'override et trois bits de selection ; la variation de ces
selecteurs inactifs ne suffit pas a etablir un etat incompatible. La lecture
0xffffffff et tout override actif restent refuses ; zero reste refuse comme
etat non valide ici. SS1 doit toujours valoir 6. Les controles PCI, BAR, chip,
VRAM, PLM, WPR2, GFW, PROM et firmware ainsi que la confirmation manuelle U
restent obligatoires. La transaction d'unlock et le retour du GOP ne changent pas.

Build : python tools/build_ga104_unlock.py --cold-selector-mask
Sortie : build/ga104-cold-selector-mask/GA104-UNLOCK.EFI
Verification : python tools/test_ga104_port.py

Les tests executent le callback reel avec les cinq valeurs SS0 observees,
verifient le refus de chacun des huit bits d'override, les lectures invalides
et le maintien des autres controles avec le nouveau SS0 de RUN15.
Le materiel et l'execution des firmwares sont simules dans ces tests : le
prochain boot reste necessaire pour valider cette variante sur la machine.

La variante v1 et le paquet publie v0.4.1 restent reproductibles avec
--cold-selector-variants ; leurs empreintes historiques ne designent pas la v2.
La mise a jour locale de la cle sauvegarde le boot v1 et son ACTIVE-BOOT.json
dans EFI/TOOLS/COLD-SELECTORS-V1-BACKUP et ajoute RESTAURER-SELECTEURS-V1.cmd.
Les rapports RUNxx et les firmwares ASSETS sont conserves.

## Historique : version 1

Le boot reussi RUN01 avait SS0=0x61014100 et SS1=6. Les boots refuses
RUN03 et RUN04 ont respectivement SS0=0x60014100 et SS0=0x61004100,
avec le meme chip, la meme taille de VRAM, PLM=0xffffff8f et WPR2_HI=0.
RUN02 avait SS0=0 et SS1=0 et demeure refuse.

La comparaison exacte avec la premiere capture rejette donc aussi les
demarrages avec la version originale. Restaurer cette version ne suffisait pas.
La raison de la variation du registre n'est pas encore etablie.

Cette variante accepte uniquement ces trois SS0 observes, toujours avec SS1=6.
Le modele documente par Zagitta indique que les bits d'activation d'override
(masque 0x88888888) sont tous inactifs dans ces trois valeurs. Cela motive un
essai limite, sans etablir a lui seul l'equivalence de tous les etats internes.
Les controles PCI, BAR, chip, VRAM, PLM, WPR2, GFW, PROM et firmware restent
en place. L'operation d'unlock et le retour du GOP ne changent pas.
Un journal supplementaire detaille chaque condition froide et les valeurs GFW.
Aucune lecture ou activation ECC n'est incluse dans cette variante.

Build : python tools/build_ga104_unlock.py --cold-selector-variants
Sortie separee : build/ga104-cold-selectors/GA104-UNLOCK.EFI
SHA256 : 4b58554e18b0a2e987040cf3c006aa122bfd84ccd0f2dbee11cb225da7340000

Verification : python tools/test_ga104_port.py. Les essais simules couvrent
les trois etats dans le callback reel, six selecteurs refuses et six autres
conditions froides invalides, en plus des scenarios existants de transaction
et de nettoyage. Verification PE AMD64 EFI sans imports reussie.
La variante n'est pas encore validee sur le materiel.

L'EFI original valide 5e5b99107f6ddf38fdda50a7995b60cb3c335742cfa6f7d8df4f519fd3c353fa
reste intact dans build/ga104-unlock et F:/EFI/TOOLS/GA104-UNLOCK-VALIDE.EFI.
F:/RESTAURER-UNLOCK-VALIDE.cmd permet de le restaurer.

Essai : arret complet, boot de la cle en Discrete, U lorsque l'invite apparait,
puis Windows Boot Manager sans redemarrer. Si le programme refuse encore,
le nouveau rapport indiquera les autres conditions. En cas de STOP, suivre
l'instruction d'arret physique affichee. Conserver la cle pendant l'operation.
