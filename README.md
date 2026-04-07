# Projet-SEE---Ultrasons-et-STM32MP157C
Interface entre un système Linux et un capteur à ultrasons de mesure de distance. La carte utilisée est une STM32MP157C.


## ultrason_control
C'est un programme qui intéragit directment avec la carte pour obtenir une mesure et la stocker dans le fichier distance.txt. Ce programme peut être exécuté avec "./ultrason_control" et peux avoir les argument suivant : 

1) ./ultrason_control insmod : pour chargé le module/driver déjà présent sur la carte

2) ./ultrason_control mknod : pour créer le noeud /dev/ultrason sur la carte

3) ./ultrason_control remove : pour supprimer le noeud de la carte et décharger le module/driver

4) ./ultrason_control : pour effectuer une mesure qui sera stocker dans distance.txt. Attention, le module sera chargé et le noeud sera créé si ce n'est pas déjà le cas
