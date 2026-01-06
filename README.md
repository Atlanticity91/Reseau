Projet réseau ( Alves Quentin )

Le Projet est découpé en 4 fichiers pour les clients et serveurs + "net_globals.h" 
pour les paramètres de base, "net_utils.h" pour les helpers autours de la std C(11)
"net_utils.c" les implémentations et "net_socket.c" le fichier d'implémentation.
pour la couche sur l'API socket.
Dans "net_utils.h/.c", on peut retrouver la partie RSA avec le commentaire 
"CRYPTO ( RSA-TOY )"

Pour générer le Projet simplement lancé le makefile fournis, il compile en C(11)
et inclut la librairie pthread, de plus certain header un peu exotique sont 
présent comme "endian.h"

Note : Je n'ai pas eu le temps de FIX ma gestion de l'exit du serveur comme je
voulais alors au lancement, il faut appuyer sur entrer pour valider l'input du
serveur ( il attend l'entrée de l'utilisateur pour exit )
