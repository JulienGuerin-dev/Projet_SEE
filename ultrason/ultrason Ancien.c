#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/of.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver ultrason pour HC-SR04²");
MODULE_VERSION("1.0");

static struct gpio_desc *gpio_trig;
static struct gpio_desc *gpio_echo;
static int major;
static char buf[32];

static ssize_t dev_read(struct file *fp, char __user *ubuf, size_t len, loff_t *off)
{
    ktime_t start, end;
    u32 delta;
    long distance;

    if (*off > 0)
        return 0;

    gpiod_set_value(gpio_trig, 0);
    udelay(2);
    gpiod_set_value(gpio_trig, 1);
    udelay(10);
    gpiod_set_value(gpio_trig, 0);

    while (gpiod_get_value(gpio_echo) == 0);
    start = ktime_get();
    while (gpiod_get_value(gpio_echo) == 1);
    end = ktime_get();

    delta    = (u32)ktime_us_delta(end, start);
    distance = (long)(delta / 58);

    printk(KERN_INFO "ultrason: %lld µs -> %ld cm\n", delta, distance);

    snprintf(buf, sizeof(buf), "%ld cm\n", distance);
    copy_to_user(ubuf, buf, strlen(buf));

    *off = 1;
    return strlen(buf);
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
};

static int ultrason_probe(struct platform_device *pdev)
{
    printk(KERN_INFO "ultrason : Chargement\n");
    gpio_trig = devm_gpiod_get(&pdev->dev, "trig", GPIOD_OUT_LOW);
    gpio_echo = devm_gpiod_get(&pdev->dev, "echo", GPIOD_IN);
    major     = register_chrdev(0, "ultrason", &fops);

    printk(KERN_INFO "ultrason: chargé, majeur = %d\n", major);
    printk(KERN_INFO "ultrason: mknod /dev/ultrason c %d 0\n", major);
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
