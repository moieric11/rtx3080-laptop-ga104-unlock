# Diagnostic v0.2 du mode Discrete

RUN01 récupéré depuis la clé F:, avec REPORT.TXT, PROM.BIN et ROMIMAGE.BIN.
Les 64 dwords PCI, les 16 registres, le PROM et ROMIMAGE sont inchangés par rapport
à RUN00. Les adresses de handles ci-dessous ne sont valables que pour ce boot.

## Propriétaires

- Contrôleur RTX 3080 : b568ff98.
- Pilote NVIDIA : b5685f98, image du même handle, version 0006000a,
  ImageSize 0002be40, EfiBootServicesCode.
- Ce pilote ouvre PciIo en BY_DRIVER et DevicePath en BY_CHILD_CONTROLLER pour
  deux enfants : b5638d98 (GOP physique observé) et b5639c98 (non inspecté dans
  RUN01, à examiner dynamiquement par le test avant toute déconnexion).
- ConsoleOutHandle : b57af098, GOP virtuel sans chemin.
- Sur le GOP physique, les propriétaires BY_DRIVER exposent une pile de
  services console ; un lien BY_CHILD_CONTROLLER rejoint la console virtuelle.
  Le pilote de version 0xa et image 0x8a20 correspond à la forme attendue d'un
  agrégateur de console, mais son nom n'a pas été extrait : c'est une inférence.

## Démarrage Windows

Boot0001 actif, description Windows Boot Manager, chemin
\EFI\Microsoft\Boot\bootmgfw.efi sur partition GPT
ae4b382b-0cf9-4f27-ae9a-3f701d8055f2. Le rapport contient une option de démarrage,
pas une preuve que l'image a été chargée ou démarrée par notre code.

## Test préparé

GA104-HANDOFF.EFI réalise d'abord la collecte normale, puis propose T pour tester
une seule transition du pilote. Il vérifie les deux chemins enfants sous le GPU,
le propriétaire, le binding, le BME initialement désactivé, le GOP et la console.
Le propriétaire et la configuration sont revérifiés juste avant DisconnectController.
Après déconnexion, il teste l'ouverture exclusive PciIo et la referme, puis appelle
ConnectController une seule fois avec le pilote d'origine. Il vérifie ensuite le
GOP physique, la console, les BAR et le propriétaire revenu.

Aucune transaction unlock, aucun DMA de notre code, aucune écriture MMIO/PCI
explicite, aucun FLR explicite et aucune écriture NVRAM. Les fonctions Stop/Start
du pilote de firmware peuvent elles-mêmes modifier l'état du GPU : ce test n'est
donc plus une simple lecture. En cas de perte du journal après la déconnexion,
la reconnexion reste tentée. En cas de retour d'affichage non vérifié, le code
attend l'arrêt manuel au lieu de rendre la main au chargeur Windows.

Le journal de transition est HANDOFF.TXT, distinct du REPORT.TXT collecté avant
la transition. 21 scénarios de transition et les contrôles clavier passent en
simulation. Le test matériel de la transition reste à réaliser.
