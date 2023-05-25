#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/fbcon.h>
#include <asm/fb.h>
#include <linux/leds.h>

#define FB_FIREFLY_MAJOR 35  
#define DEVICE_NAME   "fb_firefly"
static struct class *fb_firefly_class;
struct device *fb_firefly_dev; 
struct led_trigger *firefly_power_click;
EXPORT_SYMBOL(firefly_power_click);
void
fb_ctl_led(int blank)
{	

	if(blank== 4){
    	led_trigger_event(firefly_power_click,LED_OFF);
	}else if(blank == 0){
		led_trigger_event(firefly_power_click,LED_FULL);
	}
}
EXPORT_SYMBOL(fb_ctl_led);

static long fb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case FBIOBLANK:
		fb_ctl_led(arg);
		break;
	default:
        printk("fb_firefly do nothing\n");
	}
	return ret;
}


static const struct file_operations fb_firefly_fops = {
	.owner =	THIS_MODULE,
	.unlocked_ioctl = fb_ioctl,
};


static int __init
fb_firefly_init(void)
{
	int ret;
 
    ret = register_chrdev(FB_FIREFLY_MAJOR, DEVICE_NAME, &fb_firefly_fops);
	if (ret) {
		printk("unable to get major %d for fb firefly devs\n", FB_FIREFLY_MAJOR);
		return ret;
	}

	fb_firefly_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(fb_firefly_class)) {
		ret = PTR_ERR(fb_firefly_class);
		pr_warn("Unable to create fb firefly class; errno = %d\n", ret);
		fb_firefly_class = NULL;
		goto err_class;
	}

    fb_firefly_dev = device_create(fb_firefly_class, NULL, MKDEV(FB_FIREFLY_MAJOR, 0), 0, DEVICE_NAME);
	if (IS_ERR(fb_firefly_dev)) {
		/* Not fatal */
		printk("Unable to create device for fb firefly,error = %ld \n ",PTR_ERR(fb_firefly_dev));
		fb_firefly_dev = NULL;
        goto err_device;
	}

	led_trigger_register_simple("ir-power-click", &firefly_power_click);
    return 0;

err_class:
	unregister_chrdev(FB_FIREFLY_MAJOR, "fb_firefly");
err_device:
    class_destroy(fb_firefly_class);
	return ret;

}

#ifdef MODULE
module_init(fb_firefly_init);
static void __exit
fb_firefly_exit(void)
{
	class_destroy(fb_firefly_class);
	unregister_chrdev(FB_FIREFLY_MAJOR, "fb_firefly");
	led_trigger_unregister_simple(firefly_power_click);
}

module_exit(fb_firefly_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FB NOTIFY FIREFLY");
#else
subsys_initcall(fb_firefly_init);
#endif
MODULE_LICENSE("GPL");
