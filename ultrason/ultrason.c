#include <linux/fs.h>
#include <linux/uaccess.h>
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

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver ultrason HC-SR04");
MODULE_AUTHOR("Ziad et Julien");
MODULE_VERSION("1.0");

// structure du driver
struct hcsr04_id {
    struct gpio_desc *gpio_trig;
    struct gpio_desc *gpio_echo;
    int irq;
    ktime_t start;
    ktime_t end;
    struct completion measurement;
};

static struct hcsr04_id *hcsr04_driver;

static int major;
static char buf[32];

static irqreturn_t hcsr04_irq_handler(int irq, void *dev_id)
{
    struct hcsr04_id *driver = dev_id;

    int val = gpiod_get_value(driver->gpio_echo);

    if (val == 1) {
        driver->start = ktime_get();
    } else if (val == 0) {
        driver->end = ktime_get();
        complete(&(driver->measurement));   // réveil dev_read() pour continuer
    }

    return IRQ_HANDLED;
}

static ssize_t dev_read(struct file *fp, char __user *ubuf, size_t len, loff_t *off)
{
    printk(KERN_INFO "ultrason : Fonction Read commencé\n");

    struct hcsr04_id *driver = hcsr04_driver;

    u32 delta_us;
    int distancex10;        // distance *10 pour conserver un chiffre décimal
    int distance_cm;        // partie entière de la distance en centimètres
    int distance_decimal;   // chiffre après la virgule (dixième de centimètre)

    if (*off > 0)      // permet de ne pas lire à l'infini
        return 0;

    reinit_completion(&(driver->measurement));

    gpiod_set_value(driver->gpio_trig, 0);
    udelay(2);

    printk(KERN_INFO "ultrason : Envoie de TRIG\n");

    gpiod_set_value(driver->gpio_trig, 1);
    udelay(10);
    gpiod_set_value(driver->gpio_trig, 0);

    if (wait_for_completion_interruptible_timeout(&(driver->measurement), msecs_to_jiffies(1000)) <= 0) {
        return -ETIMEDOUT;
    }

    printk(KERN_INFO "ultrason : Calcul du delta et distance\n");

    delta_us = (u32)ktime_us_delta(driver->end, driver->start);

    distancex10 = (int)(delta_us * 10 / 58) + 5;        // durée multipliée par 10 pour garder une décimale
    distance_cm = (int)distancex10 / 10;                // cast pour ne garder que la partie entière
    distance_decimal = distancex10 - distance_cm * 10;  // partie décimale

    snprintf(buf, sizeof(buf), "%d.%d cm\n", distance_cm, distance_decimal);

    if (copy_to_user(ubuf, buf, strlen(buf)))
        return -EFAULT;

    *off = strlen(buf);
    return strlen(buf);
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
};

static int ultrason_probe(struct platform_device *pdev)
{
    int ret;
    struct hcsr04_id *driver;

    driver = devm_kzalloc(&pdev->dev, sizeof(struct hcsr04_id), GFP_KERNEL);
    if (!driver) return -ENOMEM;

    hcsr04_driver = driver;
    init_completion(&(driver->measurement));

    driver->gpio_trig = devm_gpiod_get(&pdev->dev, "trig", GPIOD_OUT_LOW);
    driver->gpio_echo = devm_gpiod_get(&pdev->dev, "echo", GPIOD_IN);

    driver->irq = gpiod_to_irq(driver->gpio_echo);

    ret = devm_request_irq(&pdev->dev, driver->irq, hcsr04_irq_handler,
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           "hcsr04", driver);
    if (ret) return ret;

    major = register_chrdev(0, "ultrason", &fops);

    printk(KERN_INFO "ultrason: chargé, majeur = %d\n", major);
    printk(KERN_INFO "ultrason: Faire mknod /dev/ultrason c %d 0\n", major);

    return 0;
}

static void ultrason_remove(struct platform_device *pdev)
{
    unregister_chrdev(major, "ultrason");
    printk(KERN_INFO "ultrason: déchargé\n");
}

static const struct of_device_id ultrason_of_match[] = {
    { .compatible = "hc-sr04" },
    { }
};
MODULE_DEVICE_TABLE(of, ultrason_of_match);

static struct platform_driver ultrason_driver = {
    .probe  = ultrason_probe,
    .remove = ultrason_remove,
    .driver = {
        .name           = "ultrason",
        .of_match_table = ultrason_of_match,
    },
};

module_platform_driver(ultrason_driver);
