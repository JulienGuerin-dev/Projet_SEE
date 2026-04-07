#include <linux/fs.h>              /* file_operations, register_chrdev, copy_to_user */
#include <linux/uaccess.h>         /* copy_to_user : transfert noyau -> espace utilisateur */
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/completion.h>

MODULE_LICENSE("GPL");                        /* licence obligatoire pour accéder aux fonctions noyau */
MODULE_DESCRIPTION("Driver ultrason HC-SR04"); /* description visible avec modinfo */
MODULE_AUTHOR("Julien Guerin");                    /* auteur visible avec modinfo */
MODULE_VERSION("1.0");                        /* version visible avec modinfo */

struct hcsr04_id {
    struct gpio_desc *gpio_trig;    /* descripteur GPIO pour la broche TRIG (sortie) */
    struct gpio_desc *gpio_echo;    /* descripteur GPIO pour la broche ECHO (entrée) */
    int irq;                        // Numéro de l'interruption
    ktime_t start;             // Timestamp au front montant
    ktime_t end;              // Timestamp au front descendant
    struct completion measurement;
};

static struct hcsr04_id *hcsr04_driver;

static int major;                   /* numéro majeur alloué dynamiquement par register_chrdev */
static char buf[32];                /* buffer noyau contenant la distance formatée en chaîne */


static irqreturn_t hcsr04_irq_handler(int irq, void *dev_id) {
    struct hcsr04_id *driver = dev_id;

    int val = gpiod_get_value(driver->gpio_echo);

    if(val == 1){
        driver->start = ktime_get();
    }else if (val == 0){
        driver->end = ktime_get();
        complete(&(driver->measurement));
    }

    return IRQ_HANDLED;
}

static ssize_t dev_read(struct file *fp, char __user *ubuf, size_t len, loff_t *off)
/* fonction appelée automatiquement par le noyau lors d'un read() ou cat /dev/ultrason */
{
    printk(KERN_INFO "ultrason : Fonction Read commencé\n");
    struct hcsr04_id *driver = hcsr04_driver;
    u32 delta_us;       /* delta de temps en µs entre début et fin du signal ECHO */
    int distancex10; 
    int distance_cm;       /* distance calculée en centimètres */
    int distance_decimal;   
    
    
    //Debug
    ktime_t startDEBUG, endDEBUG;  
    u32 delta_DEBUG;       

    if (*off > 0)        /* si déjà lu, retourner 0 = fin de fichier pour éviter une boucle infinie */
        return 0;
        
    reinit_completion(&(driver->measurement));

    gpiod_set_value(driver->gpio_trig, 0); /* assure que TRIG est à l'état bas avant de commencer */
    udelay(2);                     /* attente 2µs pour stabiliser le signal */
    printk(KERN_INFO "ultrason : Envoie de TRIG\n");
    gpiod_set_value(driver->gpio_trig, 1); /* TRIG à l'état haut : début de l'impulsion */
    udelay(10);                    /* maintien de l'impulsion pendant 10µs comme exigé par le HC-SR04 */
    gpiod_set_value(driver->gpio_trig, 0); /* TRIG à l'état bas : fin de l'impulsion de déclenchement */

    // Attente de l'interruption (timeout de 1 sec)
    if (wait_for_completion_interruptible_timeout(&(driver->measurement), msecs_to_jiffies(1000)) <= 0) {
        return -ETIMEDOUT;
    }

    printk(KERN_INFO "ultrason : Calcul du delta et distance\n");
    delta_us = (u32)ktime_us_delta(driver->end, driver->start); /* calcul du delta en µs entre end et start */
    distancex10 = (int)(delta_us*10/ 58)+7;           /* conversion en cm : distance = delta_us / 58 */
    distance_cm = (int)distancex10/10;
    distance_decimal = distancex10-distance_cm*10;
    printk(KERN_INFO "ultrason: %u µs -> %d.%d cm\n", delta_us, distance_cm, distance_decimal); /* affichage dans dmesg */

    snprintf(buf, sizeof(buf), "%d.%d cm\n", distance_cm, distance_decimal); /* formatage de la distance en chaîne dans le buffer noyau */

    if (copy_to_user(ubuf, buf, strlen(buf))) /* transfert du buffer noyau vers l'espace utilisateur */
        return -EFAULT;                       /* retourne une erreur si le transfert échoue */

    *off = strlen(buf);              /* indique que la lecture a eu lieu pour éviter une relecture infinie */
    return strlen(buf);    /* retourne le nombre d'octets écrits pour signaler la fin de la lecture */
}

static struct file_operations fops = {
    .owner = THIS_MODULE, /* pointeur vers le module courant */
    .read  = dev_read,    /* associe l'opération read() à notre fonction dev_read */
};

static int ultrason_probe(struct platform_device *pdev)
/* fonction appelée automatiquement par le noyau quand il trouve compatible = "hc-sr04" dans le DTS */
{
    int ret;
    struct hcsr04_id *driver;
    dev_info(&pdev->dev, "Probing HC-SR04 driver\n");
    
    driver = devm_kzalloc(&pdev->dev, sizeof(struct hcsr04_id), GFP_KERNEL);
    if (!driver) return -ENOMEM;

    // Sauvegarde dans le pointeur global pour le read()
    hcsr04_driver = driver;
    init_completion(&(driver->measurement));
    
    /* récupère GPIO TRIG depuis le DTS via "trig-gpios", configuré en sortie à l'état bas */
    driver->gpio_trig = devm_gpiod_get(&pdev->dev, "trig", GPIOD_OUT_LOW);

    /* récupère GPIO ECHO depuis le DTS via "echo-gpios", configuré en entrée */
    driver->gpio_echo = devm_gpiod_get(&pdev->dev, "echo", GPIOD_IN);
    
    driver->irq = gpiod_to_irq(driver->gpio_echo);
    if (driver->irq < 0) return driver->irq;

    ret = devm_request_irq(&pdev->dev, driver->irq, hcsr04_irq_handler, 
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           "hcsr04", driver);
     if (ret) return ret;

    /* enregistre le driver comme character device, 0 = numéro majeur alloué automatiquement */
    major = register_chrdev(0, "ultrason", &fops);

    /* affiche le numéro majeur dans dmesg pour savoir quelle commande mknod utiliser */
    printk(KERN_INFO "ultrason: chargé, majeur = %d\n", major);
    printk(KERN_INFO "ultrason: mknod /dev/ultrason c %d 0\n", major);

    return 0; /* retourne 0 = succès */
}

static void ultrason_remove(struct platform_device *pdev)
/* fonction appelée lors du rmmod, retourne void imposé par le noyau Linux 6.x */
{
    unregister_chrdev(major, "ultrason"); /* désenregistre le character device et libère le numéro majeur */
    printk(KERN_INFO "ultrason: déchargé\n"); /* confirmation du déchargement dans dmesg */
}

/* table de correspondance entre le DTS et ce driver */
static const struct of_device_id ultrason_of_match[] = {
    { .compatible = "hc-sr04" }, /* doit correspondre exactement au compatible dans le DTS */
    { }                          /* entrée vide obligatoire pour terminer le tableau */
};
MODULE_DEVICE_TABLE(of, ultrason_of_match); /* exporte la table pour que le noyau fasse la correspondance */

/* structure décrivant le driver auprès du noyau */
static struct platform_driver ultrason_driver = {
    .probe  = ultrason_probe,  /* fonction appelée au chargement quand le DTS correspond */
    .remove = ultrason_remove, /* fonction appelée au déchargement */
    .driver = {
        .name           = "ultrason",        /* nom du driver */
        .of_match_table = ultrason_of_match, /* table de correspondance DTS */
    },
};

/* remplace module_init et module_exit pour un platform driver */
/* enregistre et désenregistre automatiquement le driver au chargement et déchargement */
module_platform_driver(ultrason_driver);
