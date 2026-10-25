# Premier diagnostic matériel Lenovo GA104

Rapport et deux copies ROM récupérés depuis F:/RTX3080-PROBE/RUN00.
La collecte, la lecture PROM et la sauvegarde ROMIMAGE ont renvoyé EFI_SUCCESS.

## Observations

- GPU unique correspondant : 0000:01:00.0, 24dc10de, subsystem 3a5817aa.
- PMC_BOOT_0 : b74000a1, GA104, cohérent avec le matériel Windows.
- BAR0 : d0000000, longueur 01000000 (16 MiB).
- Mémoire framebuffer BAR1 : f800000000.
- GOP physique : chemin PCI 01.1 / 00.0, même chemin que la RTX,
  framebuffer f800000000. Le GOP sans chemin utilise le même framebuffer.
  L'affichage UEFI utilise donc la RTX sur ce démarrage. Le GOP sans chemin
  est compatible avec une console agrégée ; son rôle exact n'est pas démontré.
- Commande PCI : 0003 (mémoire activée, bus mastering désactivé).
- PCIe FLR advertised : bit 28 de Device Capabilities (112c8de1) positionné.
  La présence de ce bit ne valide pas un reset réel sur ce portable.
- WPR2_HI : 00000000.
- FEAT_PLM : ffffff8f.
- SS0 : 61014100, SS1 : 00000006. Ces valeurs diffèrent des sélecteurs
  d'unlock 88888888 / 8, ainsi que du profil froid MSI du port public.
- Taille FB exposée à 1183a4 : 4000, cohérente avec 16384 MiB.
- Copie ROM firmware : 157696 octets ; fenêtre PROM : 1048576 octets.

## Conséquence pour le portage

Ne pas supprimer le contrôle d'affichage Intel du port MSI pour faire accepter
ce rapport : la transaction et son reset pourraient couper le seul affichage UEFI.
Il faut déterminer si Hybrid/AMD iGPU peut fournir un chemin GOP distinct sur
cette machine. Le code Linux prend en charge un GPU d'affichage avec reprise par
nvidia-drm ; ce comportement ne constitue pas une preuve de reprise par Windows
après une transaction UEFI. Aucun unlock actif n'a été tenté ici.

La sonde fonctionne ; aucun changement de son binaire n'est nécessaire pour un
éventuel relevé en mode hybride. Les captures existantes doivent être conservées.
