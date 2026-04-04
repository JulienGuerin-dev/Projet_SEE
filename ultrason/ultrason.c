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
#include <linux/fs.h>         // Pour file_operations et register_chrdev
#include <linux/uaccess.h>    // Pour copy_to_user

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver ultrason pour HC-SR04²");
MODULE_VERSION("1.0");

struct hcsr04_data {
    struct gpio_desc *trig_gpio;
    struct gpio_desc *echo_gpio;
    int irq;                        // Numéro de l'interruption
    ktime_t echo_start;             // Timestamp au front montant
    ktime_t echo_stop;              // Timestamp au front descendant
    struct completion measurement_done;
};

static struct hcsr04_data *hcsr04_dev_data;
static int major_number; // Stocke le numéro majeur de notre Character Device

static irqreturn_t hcsr04_irq_handler(int irq, void *dev_id) {
    struct hcsr04_data *data = dev_id;

    int val = gpiod_get_value(data->echo_gpio);

    if(val == 1){
        data->echo_start = ktime_get();
    }else if (val == 0){
        data->echo_stop = ktime_get();
        complete(&data->measurement_done);
    }

    return IRQ_HANDLED;
}

// --- 2. Fonction Read du Character Device ---
static ssize_t hcsr04_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    struct hcsr04_data *data = hcsr04_dev_data;
    char kbuf[32]; // Buffer côté noyau
    int len;
    s64 time_us;
    int distance_cm;

    // Si on a déjà lu (EOF), on s'arrête
    if (*ppos > 0)
        return 0;

    reinit_completion(&data->measurement_done);

    // Déclenchement
    gpiod_set_value(data->trig_gpio, 1);
    udelay(10);
    gpiod_set_value(data->trig_gpio, 0);

    // Attente de l'interruption (timeout de 1 sec)
    if (wait_for_completion_interruptible_timeout(&data->measurement_done, msecs_to_jiffies(1000)) <= 0) {
        return -ETIMEDOUT;
    }

    // Calcul avec ktime_us_delta (CONSIGNE RESPECTÉE)
    time_us = ktime_us_delta(data->echo_stop, data->echo_start);
    distance_cm = time_us / 58;

    // Formatage de la chaîne dans notre buffer noyau
    len = snprintf(kbuf, sizeof(kbuf), "%d\n", distance_cm);

    // Copie vers l'espace utilisateur (CONSIGNE RESPECTÉE)
    if (copy_to_user(user_buf, kbuf, len)) {
        return -EFAULT;
    }

    *ppos += len;
    return len;
}

// Structure associant nos fonctions aux opérations de fichier
static struct file_operations hcsr04_fops = {
    .owner = THIS_MODULE,
    .read = hcsr04_read,
};


// --- 3. Probe & Remove ---
static int hcsr04_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    struct hcsr04_data *data;
    int ret;

    dev_info(dev, "Probing HC-SR04 driver\n");

    data = devm_kzalloc(dev, sizeof(struct hcsr04_data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    // Sauvegarde dans le pointeur global pour le read()
    hcsr04_dev_data = data;
    init_completion(&data->measurement_done);

    data->trig_gpio = devm_gpiod_get(dev, "trig", GPIOD_OUT_LOW);
    if (IS_ERR(data->trig_gpio)) return PTR_ERR(data->trig_gpio);

    data->echo_gpio = devm_gpiod_get(dev, "echo", GPIOD_IN);
    if (IS_ERR(data->echo_gpio)) return PTR_ERR(data->echo_gpio);

    data->irq = gpiod_to_irq(data->echo_gpio);
    if (data->irq < 0) return data->irq;

    ret = devm_request_irq(dev, data->irq, hcsr04_irq_handler, 
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           pdev->name, data);
    if (ret) return ret;

    platform_set_drvdata(pdev, data);

    // Enregistrement du Character Device (CONSIGNE RESPECTÉE)
    // Le premier 0 demande au noyau de choisir un Major Number dynamiquement
    major_number = register_chrdev(0, "hcsr04", &hcsr04_fops);
    if (major_number < 0) {
        dev_err(dev, "Erreur lors de l'enregistrement du chrdev\n");
        return major_number;
    }

    dev_info(dev, "HC-SR04 enregistré avec le Major Number %d\n", major_number);
    return 0;
}

static void hcsr04_remove(struct platform_device *pdev) {
    unregister_chrdev(major_number, "hcsr04");
    dev_info(&pdev->dev, "HC-SR04 driver removed\n");
}

static const struct of_device_id hcsr04_ids[] = {
    {.compatible = "hc-sr04"},
    {/* end node*/}
};
MODULE_DEVICE_TABLE(of, hcsr04_ids);

static struct platform_driver hcsr04_driver = {
    .probe = hcsr04_probe,
    .remove = hcsr04_remove,
    .driver = {
        .name = "hcsr04-driver",
        .of_match_table = hcsr04_ids,
    },
};

/*
static int __init kinit(void) {
    printk(KERN_INFO "Init hc-sr04 driver\n");
    return 0;
}

static void __exit kexit(void) {
    printk(KERN_INFO "Exit hc-sr04 drive\n");
}

module_init(kinit);
module_exit(kexit);
*/
module_platform_driver(hcsr04_driver); // Remplace tout ce qu'il y a au dessus

