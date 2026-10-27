# Refus des derniers boots USB

Rapports copies depuis F:\RTX3080-UNLOCK, RUN10 a RUN16, le 27 octobre 2026.
Chaque copie de REPORT.TXT a ete comparee par SHA256 avec l'original.
Les heures ci-dessous sont celles des fichiers FAT32, sans validation de l'horloge UEFI.

| Essai | Date/heure du rapport | SS0 | Resultat |
| --- | --- | --- | --- |
| RUN12 | 26/10 01:48:22 | 0x61014100 | transaction reussie, affichage restaure |
| RUN13 | 26/10 20:43:20 | 0x61014100 | transaction reussie, affichage restaure |
| RUN14 | 27/10 21:56:10 | 0x61014101 | refus cold-state |
| RUN15 | 27/10 22:02:02 | 0x60004100 | refus cold-state |
| RUN16 | 27/10 22:03:24 | 0x60004100 | refus cold-state |

Les trois derniers essais renvoient EFI_NOT_READY (0x8000000000000006),
attempted=0 et operation=0. Le programme s'arrete au controle initial CheckCold,
avant la confirmation U et avant de detacher le pilote d'affichage.
display_restored=0 ne signifie donc pas qu'une restauration d'affichage a echoue.

Le seul controle negatif est ss0=0. Chip, VRAM, PLM, WPR2 et SS1 passent,
aucune faute de lecture n'est signalee, et gfw=0 est le resultat accepte
par CheckCold. Les valeurs GFW sont identiques a celles de RUN13.
Les controles PROM et fichiers firmware ne sont pas atteints dans RUN14-16.

Le binaire actif F:\EFI\BOOT\BOOTX64.EFI a le SHA256
4b58554e18b0a2e987040cf3c006aa122bfd84ccd0f2dbee11cb225da7340000,
conforme a la variante cold-selectors-v1 documentee dans ACTIVE-BOOT.json.
Cette variante autorise exactement 0x61014100, 0x60014100 et 0x61004100
(unlock/board_profile.h). Les deux nouvelles valeurs sont donc refusees.
0x61014101 figure deja dans les cas explicitement rejetes du test callback.

RUN14 differe de la valeur reussie 0x61014100 par le bit 0.
RUN15-16 combinent deux variations deja observees separement dans les
valeurs autorisees, mais cette combinaison n'est pas dans la liste.
Ces comparaisons ne prouvent pas l'equivalence des etats internes du GPU.

L'extinction rapportee par l'utilisateur n'a donc pas suffi a retrouver une
valeur autorisee. Les journaux UEFI ne permettent pas de determiner pourquoi
SS0 varie ni de distinguer arret hybride, coupure GPU complete ou
initialisation differente par le firmware. Aucun changement de ces mecanismes
ne peut etre affirme a partir des seuls rapports.

Conclusion : refus du filtre SS0 avant toute operation d'unlock, et non echec
du chargement des firmwares ou de l'execution de l'unlock. L'analyse ne modifie
ni la liste autorisee, ni le binaire, ni la cle USB. Toute extension de la liste
necessite une justification technique et une validation distinctes.
