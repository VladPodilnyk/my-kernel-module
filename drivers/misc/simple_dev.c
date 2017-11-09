#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/serial_core.h>

#define SIMPLE_DEV_IRQ 53

static struct uart_port *uap;

static irqreturn_t simple_irq_handler(int irq, void *dev_id)
{
	printk("IRQ: %i\n", irq);
	return IRQ_HANDLED;
}


static int __init mk_life_init(void)
{
	int irq_ret;

	uap = kmalloc(sizeof(struct uart_port), GFP_KERNEL);
	if (!uap)
		return -ENOMEM;
	irq_ret = request_irq(SIMPLE_DEV_IRQ, simple_irq_handler, IRQF_SHARED, "simple-dev", uap);
	if (irq_ret < 0)
		return -ENXIO;
	printk("Making your life simplier...\n");
	return 0;
}


static void __exit mk_life_exit(void)
{	
	free_irq(SIMPLE_DEV_IRQ, uap);
	kfree(uap);
	printk("Congrats!!! Your life is simple.\n");
}


module_init(mk_life_init);
module_exit(mk_life_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple device module");
MODULE_AUTHOR("Vladyslav Podilnyk");

