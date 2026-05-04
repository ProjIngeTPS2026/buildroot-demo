# Présentation de l’application de commande

Dans le projet, j’ai fait une application de commande qui tourne sur le module portable sous Linux. En gros ce module sert à piloter les poteaux de sonorisation qui sont installés sur les points de rassemblement. Il n’y a pas de serveur central : les bases fixes sont juste sur le même réseau, et le module portable discute directement avec elles.

L’interface est faite en C++ avec Qt. Elle est prévue pour un petit écran horizontal, environ 800 x 480, donc les menus sont séparés pour ne pas tout mettre au même endroit.

Voir annexes : les captures des menus principaux sont dans `docs/rapport_application/screenshots`.

## Fonctionnement général

Le menu Bases sert à voir les poteaux disponibles et à choisir ceux qui vont diffuser. Le menu Carte affiche les points autour de Télécom Physique Strasbourg et du pôle API. Le menu Diffusion permet de lancer un message déjà présent dans la bibliothèque audio. Le menu Direct sert au mode mégaphone en temps réel. Le menu Enregistrement permet d’enregistrer un message, de le réécouter, puis de le diffuser ou de le sauvegarder.

Pour éviter les erreurs, dès qu’une action peut faire diffuser du son sur les poteaux, l’application demande une validation avec la liste des poteaux concernés.

## Communication entre les modules

Pour les commandes qui ne sont pas de l’audio, l’application envoie des requêtes de contrôle aux bases fixes. Pour le son en direct, le flux audio utilise Roc Toolkit avec Opus pour économiser le débit. Le son est envoyé en multicast dans la plage 239.0.0.0/8, ici avec l’adresse 239.42.0.10, parce que c’est plus adapté au réseau mesh HaLow avec batman-adv.

## Structure du programme

- `portable-console` : application avec interface Qt qui tourne sur le module portable.
- `fixed-base-service` : service lancé sur chaque base fixe, qui reçoit les commandes et déclenche la sortie son.
- fichiers de configuration : identifiant, nom, position GPS, ports réseau et dossiers audio de chaque base.
- simulation locale : permet de tester plusieurs bases sur le PC.
