# Verification du switch unlock sous Windows

Double-cliquer sur `check-unlock.cmd` a la racine du projet. Python et les
bibliotheques CUDA 13.4 utilisees par le benchmark doivent etre disponibles.
Brancher le secteur et fermer les applications qui utilisent le GPU avant
de lancer le test. Trois passages prennent environ 30 a 60 secondes.
La fenetre reste ouverte pour lire le resultat.

- **UNLOCK PROBABLEMENT ACTIF** : profil conforme aux mesures avec unlock.
- **PROFIL SANS UNLOCK** : profil conforme aux mesures sans unlock.
- **INDETERMINE** : mesures instables, intermediaires ou environnement different.

Le verdict est une inference par performances sur cette RTX 3080 Laptop.
Il ne confirme pas directement la valeur d'un registre ni l'etat physique du
switch. Charge concurrente, alimentation et temperature peuvent influencer
le resultat. Le programme lance uniquement le benchmark CUDA existant ;
il n'active et ne desactive pas l'unlock.

Le test exige l'empreinte de l'executable des references locales du 23 octobre
2026 et les memes GPU, versions CUDA/cuBLAS/pilote et parametres. Il compare
le rapport FP32acc/FP16acc aux tailles 4096 et 8192. Chaque taille de chacun
des trois passages doit etre a moins de 15 % d'une seule reference et tous
les passages doivent donner le meme verdict. Ce seuil est une heuristique,
pas un detecteur materiel valide dans toutes les conditions.

Les sorties brutes et le verdict sont conserves dans
`evidence/windows-switch-check-<date>/` (`RESULTAT.txt` et `summary.json`).

Pour automatiser : `python -u tools/check_unlock.py`.
Codes de sortie : 0 = probablement actif, 1 = profil sans unlock,
2 = indetermine ou erreur. Pour utiliser le batch sans pause, definir
la variable d'environnement `CHECK_UNLOCK_NO_PAUSE=1`.
