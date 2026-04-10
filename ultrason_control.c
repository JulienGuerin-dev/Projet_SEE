#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/ultrason"   // chemin du noeud
#define SSH_PATH "~/.ssh/id_rsa_ultrason"   // chemin de la clé ssh
#define USER "root"   // username de la carte
#define IP4 "10.42.0.15"  // IPv4 de la carte
#define MAJOR_NUM 245
#define MINOR_NUM 0
#define INFINIT_SAMPLE 10000  // temps d'échantillonnage en ms pour le mode infinit

int main(int argc, char *argv[]) {
    char command[512];
    int ret;

    if (argc > 1) {
        // Cas avec argument
        if (strcmp(argv[1], "insmod") == 0) {   // Pour charger le module sur la carte
            printf("Chargement du module ultrason.ko...\n");
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'insmod /lib/modules/6.12.63/updates/ultrason.ko'", SSH_PATH, USER, IP4); // Charge le module sur la carte
            ret = system(command);    // exécute la commande shell
            if (ret == 0)
                printf("Module chargé avec succès.\n");
            else
                printf("Erreur lors du chargement du module.\n");
            return ret;
            
        } else if (strcmp(argv[1], "mknod") == 0) {   // Pour créer le noeud relié au driver
            printf("Création du device %s avec mknod...\n", DEVICE_PATH);
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'mknod %s c %d %d'", SSH_PATH, USER, IP4, DEVICE_PATH, MAJOR_NUM, MINOR_NUM); // créer le noeud
            ret = system(command);    // exécute la commande shell
            if (ret == 0)
                printf("Device créé avec succès.\n");
            else
                printf("Erreur lors de la création du device.\n");
            return ret;
            
        } else if (strcmp(argv[1], "infinit") == 0) {   // Pour afficher les mesures à l'infini tout les x ms
  
          while (1) {
            snprintf(command, sizeof(command), 
                 //"ssh -i %s %s@%s 'cat %s' > distance.txt", SSH_PATH, USER, IP4, DEVICE_PATH);    // Pour enregistrer la mesure dans le fichier
                 "ssh -i %s %s@%s 'cat %s' | tee distance.txt", SSH_PATH, USER, IP4, DEVICE_PATH);  // Pour enregistrer la mesure ET l'afficher sur le terminal
            ret = system(command);    // exécute la commande shell
            if (ret != 0) {
                printf("Erreur lors de la récupération de la distance via SSH\n");
                return ret;
            }
            usleep(INFINIT_SAMPLE); // reboucle tout les x ms
          }
          
        } else if (strcmp(argv[1], "remove") == 0) {
            printf("Suppression du device et déchargement du module...\n");
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'rm -f %s'", SSH_PATH, USER, IP4, DEVICE_PATH); // supprime le noeud
            system(command);    // exécute la commande shell
            
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'rmmod ultrason'", SSH_PATH, USER, IP4);  // décharge le module
            ret = system(command);    // exécute la commande shell
            if (ret == 0)
                printf("Module déchargé et device supprimé.\n");
            else
                printf("Erreur lors du déchargement du module.\n");
            return ret;
            
        } else {
            printf("Argument inconnu : %s\n", argv[1]);
            return 1;
        }
    } else {
        // Cas sans argument : mesure classique
        // Vérifie que le device existe SUR LA CARTE via SSH
        snprintf(command, sizeof(command),"ssh -i %s %s@%s 'test -e %s'",SSH_PATH, USER, IP4, DEVICE_PATH); // test si le noeud existe
        ret = system(command);    // exécute la commande shell
        if (ret != 0) {
            // Si test -e renvoie !=0, le noeud n'existe pas
            printf("%s n'existe pas sur la carte, vérification de la présence du module...\n", DEVICE_PATH);
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'insmod /lib/modules/6.12.63/updates/ultrason.ko'", SSH_PATH, USER, IP4); // Essaye de chargé le module
            ret = system(command);    // exécute la commande shell
            if (ret == 0)
                printf("Module chargé avec succès.\n");
            else
                printf("Module probablement déjà chargé. Dernière tentative de création du device %s...\n", DEVICE_PATH);
            
            snprintf(command, sizeof(command),
                     "ssh -i %s %s@%s 'mknod %s c %d %d'",
                     SSH_PATH, USER, IP4, DEVICE_PATH, MAJOR_NUM, MINOR_NUM); // Créer le noeud
            ret = system(command);    // exécute la commande shell
            if (ret != 0) {
                printf("Erreur lors de la création du device, impossible de continuer...\n");
                return ret;
            }
            printf("Device créé avec succès.\n");
        }

        // Mesure via SSH
        snprintf(command, sizeof(command), 
                 //"ssh -i %s %s@%s 'cat %s' > distance.txt", SSH_PATH, USER, IP4, DEVICE_PATH);    // Pour enregistrer la mesure dans le fichier
                 "ssh -i %s %s@%s 'cat %s' | tee distance.txt", SSH_PATH, USER, IP4, DEVICE_PATH);  // Pour enregistrer la mesure ET l'afficher sur le terminal
        ret = system(command);    // exécute la commande shell
        if (ret != 0) {
            printf("Erreur lors de la récupération de la distance via SSH\n");
        }
        return ret;
    }
}
