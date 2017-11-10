#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/serial_core.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>

#define SIMPLE_DEV_IRQ 53

static struct uart_port *uap;

static struct debugfs_data
{
  struct dentry *dir;
  struct dentry *file;
  u32   	counter;
  spinlock_t 	lock;
} *debugfs;

static irqreturn_t simple_irq_handler_top(int irq, void *dev_id)
{
	unsigned long flags;
	spin_lock_irqsave(&debugfs->lock, flags);
	++debugfs->counter;
	spin_unlock_irqrestore(&debugfs->lock, flags);
	return IRQ_HANDLED;
}


static int __init mk_life_init(void)
{
	int ret;
	
	debugfs = kmalloc(sizeof(struct debugfs_data), GFP_KERNEL);
	uap = kmalloc(sizeof(struct uart_port), GFP_KERNEL);
	if (!debugfs || !uap) {
		ret = -ENOMEM;
		goto ret_fail_alloc_mem;
	}
	
	spin_lock_init(&debugfs->lock);
	debugfs->counter = 0;

	debugfs->dir = debugfs_create_dir("simple-dev", NULL);
	if (!debugfs->dir) {
		ret = -ENOENT;
		goto ret_no_such_dir;
	}

	if ( (int)debugfs->dir == -ENODEV) {
        	ret = -ENODEV;
        	goto ret_no_such_dir;
    	}
	
	debugfs->file = debugfs_create_u32("uart-pl011-count", S_IRUSR | S_IRWXU, debugfs->dir, &debugfs->counter);
	if (!debugfs->file) {
		ret = -ENOENT;
		goto ret_no_such_file;
	}

	if ( (int)debugfs->file == -ENODEV) {
        	ret = -ENODEV;
        	goto ret_no_such_file;
    	}


	ret = request_irq(SIMPLE_DEV_IRQ, simple_irq_handler_top, IRQF_SHARED, "simple-dev", uap);
	if (ret < 0) {
		ret = -ENXIO;
		goto ret_no_such_dev;
	}
	
	printk("Making your life simplier...\n");
	return 0;

ret_fail_alloc_mem:
	return ret;

ret_no_such_dir:
	kfree(debugfs);
	return ret;

ret_no_such_file:
	debugfs_remove_recursive(debugfs->dir);
	kfree(debugfs);
	return ret;

ret_no_such_dev:
	debugfs_remove_recursive(debugfs->dir);
	kfree(debugfs);
	return ret;
}


static void __exit mk_life_exit(void)
{	
	free_irq(SIMPLE_DEV_IRQ, uap);
	debugfs_remove_recursive(debugfs->dir);
	kzfree(debugfs);
	printk("Congrats!!! Your life is simple.\n");
}


module_init(mk_life_init);
module_exit(mk_life_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple device module");
MODULE_AUTHOR("Vladyslav Podilnyk");

