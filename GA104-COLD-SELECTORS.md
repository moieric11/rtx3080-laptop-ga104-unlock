# Correction du refus avant U

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
