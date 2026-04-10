#include <linux/fs.h>              // file_operations, register_chrdev
#include <linux/uaccess.h>         // copy_to_user
#include <linux/module.h>          // MODULE_LICENSE, MODULE_AUTHOR, module_platform_driver
#include <linux/of_device.h>       // of_device_id, MODULE_DEVICE_TABLE : correspondance DTS
#include <linux/kernel.h>          // printk, KERN_INFO
#include <linux/delay.h>           // udelay, msecs_to_jiffies
#include <linux/gpio/consumer.h>   // gpio_desc, devm_gpiod_get, gpiod_set_value
#include <linux/platform_device.h> // platform_device, platform_driver
#include <linux/mod_devicetable.h> // of_device_id
#include <linux/ktime.h>           // ktime_t, ktime_get, ktime_us_delta
#include <linux/interrupt.h>       // request_irq, IRQ_HANDLED, IRQF_TRIGGER_*
#include <linux/sysfs.h>           // Utile ?
#include <linux/completion.h>      // completion, init_completion, wait_for_completion

MODULE_LICENSE("GPL");                         
MODULE_DESCRIPTION("Driver ultrason HC-SR04"); 
MODULE_AUTHOR("Ziad et Julien");             
MODULE_VERSION("1.0");                        

// structure du driver
struct hcsr04_id {
    struct gpio_desc *gpio_trig;   // déclaration du pointeur sur TRIG (sortie)
    struct gpio_desc *gpio_echo;   // déclaration du pointeur sur ECHO (entrée)
    int irq;                       // numéro d'IRQ associé au GPIO ECHO
    ktime_t start;                 // capture du temps sur front montant du signal ECHO
    ktime_t end;                   // capture du temps sur front descendant du signal ECHO
    struct completion measurement; // objet de synchronisation
};

static struct hcsr04_id *hcsr04_driver;     // notre driver global

static int major;      // numéro majeur 
static char buf[32];   // buffer noyau 


static irqreturn_t hcsr04_irq_handler(int irq, void *dev_id)        // Appelé automatiquement par le noyau à chaque front de ECHO
{
    struct hcsr04_id *driver = dev_id;  // Convertit en une structure driver

    int val = gpiod_get_value(driver->gpio_echo); // lit l'état d'ECHO

    if (val == 1) {                        // lorsque ECHO reçoit un signal
        driver->start = ktime_get();       // début du chronomètre
    } else if (val == 0) {                 // lorsque ECHO ne reçoit plus de signal
        driver->end = ktime_get();         // fin du chronomètre
        complete(&(driver->measurement));  // réveil dev_read() pour continuer
    }

    return IRQ_HANDLED; // indique au noyau que l'interruption a bien été traitée
}


static ssize_t dev_read(struct file *fp, char __user *ubuf, size_t len, loff_t *off)    // fonction exécutée lorsque 'cat /dev/ultrason' par exemple
{
    printk(KERN_INFO "ultrason : Fonction Read commencé\n");

    struct hcsr04_id *driver = hcsr04_driver; // création d'un driver local sur la base du global

    u32 delta_us;         // durée en µs du signal ECHO (type u32 : entier 32 bits non signé)
    int distancex10;      // distance *10 pour conserver un chiffre décimal sans utiliser de float
    int distance_cm;      // partie entière de la distance en centimètres
    int distance_decimal; // chiffre après la virgule (dixième de centimètre)


    if (*off > 0)  // permet de ne pas lire à l'infini 
        return 0;  // retourner 0 pour que 'cat' arrête de lire

    reinit_completion(&(driver->measurement)); // remet le compteur de completion à 0 avant nouvelle mesure

    gpiod_set_value(driver->gpio_trig, 0); // force TRIG à l'état bas pour partir d'un état connu
    udelay(2);                             // attente de 2µs pour stabiliser le signal

    printk(KERN_INFO "ultrason : Envoie de TRIG\n"); // log dans dmesg

    gpiod_set_value(driver->gpio_trig, 1); // TRIG à l'état haut : début de l'impulsion ultrasonique
    udelay(10);                            // maintien de l'état haut pendant 10µs 
    gpiod_set_value(driver->gpio_trig, 0); // TRIG à l'état bas : fin de l'impulsion

    
    if (wait_for_completion_interruptible_timeout(&(driver->measurement), msecs_to_jiffies(1000)) <= 0) {    // Se met en sommeil jusqu'à être réveillé par une interruption   
        return -ETIMEDOUT; // retourne l'erreur timeout si sommeil plus long que 1000ms
    }

    printk(KERN_INFO "ultrason : Calcul du delta et distance\n"); // log dans dmesg

    delta_us = (u32)ktime_us_delta(driver->end, driver->start); // calcule la durée en µs
    
    distancex10 = (int)(delta_us * 10 / 58) + 7;            // convertit la durée en une distance en cm et ajoute un offset de 7 (0.7cm) pour corriger le décalage réel
    distance_cm      = (int)distancex10 / 10;          // permet de récupérer la partie entière
    distance_decimal = distancex10 - distance_cm * 10; // permet de récupérer la partie décimale

    snprintf(buf, sizeof(buf), "%d.%d cm\n", distance_cm, distance_decimal);    // écrit la distance dans le buffer du noyau sous le format "12.4 cm"

    if (copy_to_user(ubuf, buf, strlen(buf)))   // copie le buffer noyau vers le buffer utilisateur ubuf
        return -EFAULT; // EFAULT : erreur d'accès mémoire si la copie vers userspace échoue

    *off = strlen(buf); // met à jour l'offset pour signaler que la donnée a été lue et qu'il faut s'arrêter 
    return strlen(buf); // retourne le nombre d'octets transmis à l'utilisateur
}

static struct file_operations fops = {      // table des opérations fichier
    .owner = THIS_MODULE, // empêche le déchargement pendant une utilisation
    .read  = dev_read,    // permet l'appel de dev_read
};


static int ultrason_probe(struct platform_device *pdev)     // Fonction d'initialisation directement appelée lorsque "hc-sr04" est trouvé dans le DTS
{
    int ret;                  // variable pour stocker les retours des fonctions
    struct hcsr04_id *driver; // pointeur local vers notre driver global

    //dev_info(&pdev->dev, "Probing HC-SR04 driver\n"); // log lié au driver qui fait apparâitre son nom (hc-sr04)

 
    driver = devm_kzalloc(&pdev->dev, sizeof(struct hcsr04_id), GFP_KERNEL);    // Alloue la mémoire avec que des zéros et rélié au driver pour être désalloué lors du remove
    if (!driver) return -ENOMEM;    // erreur si pas assez de mémoire disponible dans le noyau

    hcsr04_driver = driver;               // sauvegarde le pointeur dans la variable globale pour dev_read
    init_completion(&(driver->measurement)); // initialise à l'état "non-complété" pour permettre le sommeil

    driver->gpio_trig = devm_gpiod_get(&pdev->dev, "trig", GPIOD_OUT_LOW);      // pointe sur le gpio "trig-gpios" et la configure en sortie à l'état bas
    driver->gpio_echo = devm_gpiod_get(&pdev->dev, "echo", GPIOD_IN);       // pointe sur le gpio "echo-gpios" et la configure en entrée

    
    driver->irq = gpiod_to_irq(driver->gpio_echo);  // convertit le descripteur GPIO ECHO en numéro d'IRQ utilisable par le noyau

    
    ret = devm_request_irq(&pdev->dev, driver->irq, hcsr04_irq_handler,     // Créer le lien d'interruption sur front montant et descendant 
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           "hcsr04", driver);
    if (ret) return ret; // retourne l'erreur si l'enregistrement de l'IRQ a échoué


    major = register_chrdev(0, "ultrason", &fops);      // Enregistrement en tant que character device avec un numéro choisi par le noyau

    printk(KERN_INFO "ultrason: chargé, majeur = %d\n", major);    
    printk(KERN_INFO "ultrason: mknod /dev/ultrason c %d 0\n", major);

    return 0;
}

// fonction remove : appelée automatiquement par le noyau lors du rmmod ou détachement du device
// retourne void imposé par l'API platform_driver depuis Linux 6.x (était int avant)
static void ultrason_remove(struct platform_device *pdev)
{
    unregister_chrdev(major, "ultrason");     // désenregistre le character device et libère le numéro majeur
    printk(KERN_INFO "ultrason: déchargé\n"); // confirmation du déchargement dans dmesg
    // note : les ressources devm_* (GPIO, IRQ, mémoire) sont libérées automatiquement par le noyau
}

static const struct of_device_id ultrason_of_match[] = {
    { .compatible = "hc-sr04" }, // doit correspondre exactement aux infos du DTS
    { }                        
};
MODULE_DEVICE_TABLE(of, ultrason_of_match); // exporte la table pour que udev et le noyau fassent la correspondance et charge ce module

static struct platform_driver ultrason_driver = {       // structure qui permet un bus virtuel 
    .probe  = ultrason_probe,  // fonction appelée quand le noyau trouve le device compatible dans le DTS
    .remove = ultrason_remove, // fonction appelée lors du déchargement du module
    .driver = {
        .name           = "ultrason",        // nom du driver visible dans /sys/bus/platform/drivers/
        .of_match_table = ultrason_of_match, // table de correspondance DTS utilisée par le noyau
    },
};

module_platform_driver(ultrason_driver);    // Macro qui remplace les fonctions init et exit
