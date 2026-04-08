#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/ultrason"
#define SSH_PATH "~/.ssh/id_rsa_ultrason"
#define USER "root"
#define IP4 "10.42.0.15"
#define MAJOR_NUM 245
#define MINOR_NUM 0

int file_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

int main(int argc, char *argv[]) {
    char command[512];
    int ret;

    if (argc > 1) {
        // Cas avec argument
        if (strcmp(argv[1], "insmod") == 0) {
            printf("Chargement du module ultrason.ko...\n");
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'insmod /lib/modules/6.12.63/updates/ultrason.ko'", SSH_PATH, USER, IP4);
            ret = system(command);
            if (ret == 0)
                printf("Module chargé avec succès.\n");
            else
                printf("Erreur lors du chargement du module.\n");
            return ret;
        } else if (strcmp(argv[1], "mknod") == 0) {
            printf("Création du device %s avec mknod...\n", DEVICE_PATH);
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'mknod %s c %d %d'", SSH_PATH, USER, IP4, DEVICE_PATH, MAJOR_NUM, MINOR_NUM);
            ret = system(command);
            if (ret == 0)
                printf("Device créé avec succès.\n");
            else
                printf("Erreur lors de la création du device.\n");
            return ret;
        } else if (strcmp(argv[1], "infinit") == 0) {
  
          while (1) {
            snprintf(command, sizeof(command), 
                 //"ssh -i %s %s@%s 'cat %s' > distance.txt", SSH_PATH, USER, IP4, DEVICE_PATH);    // Pour enregistrer la mesure dans le fichier
                 "ssh -i %s %s@%s 'cat %s' | tee distance.txt", SSH_PATH, USER, IP4, DEVICE_PATH);  // Pour enregistrer la mesure ET l'afficher sur le terminal
            ret = system(command);
            if (ret != 0) {
                printf("Erreur lors de la récupération de la distance via SSH\n");
                return ret;
            }
            usleep(10000); // 10 ms
          }
        } else if (strcmp(argv[1], "remove") == 0) {
            printf("Suppression du device et déchargement du module...\n");
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'rm -f %s'", SSH_PATH, USER, IP4, DEVICE_PATH);
            system(command);
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'rmmod ultrason'", SSH_PATH, USER, IP4);
            ret = system(command);
            if (ret == 0)
                printf("Module déchargé et device supprimé.\n");
            else
                printf("Erreur lors du déchargement du module.\n");
            return ret;
        } else {
            printf("Argument inconnu : %s\n", argv[1]);
            printf("Usage : %s [probe|remove]\n", argv[0]);
            return 1;
        }
    } else {
        // Cas sans argument : mesure classique
        // Vérifie que le device existe SUR LA CARTE via SSH
        snprintf(command, sizeof(command),
                 "ssh -i %s %s@%s 'test -e %s'",
                 SSH_PATH, USER, IP4, DEVICE_PATH);
        ret = system(command);
        if (ret != 0) {
            // Si test -e renvoie !=0, le device n'existe pas
            printf("%s n'existe pas sur la carte, vérification de la présence du module...\n", DEVICE_PATH);
            snprintf(command, sizeof(command), "ssh -i %s %s@%s 'insmod /lib/modules/6.12.63/updates/ultrason.ko'", SSH_PATH, USER, IP4);
            ret = system(command);
            if (ret == 0)
                printf("Module chargé avec succès.\n");
            else
                printf("Module probablement déjà chargé. Dernière tentative de création du device %s...\n", DEVICE_PATH);
            
            snprintf(command, sizeof(command),
                     "ssh -i %s %s@%s 'mknod %s c %d %d'",
                     SSH_PATH, USER, IP4, DEVICE_PATH, MAJOR_NUM, MINOR_NUM);
            ret = system(command);
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
        ret = system(command);
        if (ret != 0) {
            printf("Erreur lors de la récupération de la distance via SSH\n");
        }
        return ret;
    }
}
