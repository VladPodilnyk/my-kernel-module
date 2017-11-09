#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>

char* wait_msg;
struct
{
  struct timeval start;
  struct timeval finish;
} timer;


static int __init mk_life_init(void)
{
	do_gettimeofday(& timer.start);
	wait_msg = (char*)kmalloc(7 * sizeof(char), GFP_KERNEL);
	if (!wait_msg)
		return -ENOMEM;
	memcpy(wait_msg, "Wait...", 7);
        printk("Making your life simplier\n");
	printk("%s\n", wait_msg);
	return 0;
}


static void __exit mk_life_exit(void)
{	
	kfree(wait_msg);;
	do_gettimeofday(& timer.finish);
	printk("Work time: %ld sec.\n" timer.finish.tv_sec - timer.start.tv_sec);
        printk("Congrats!!! Your life is simple\n");
	
}


module_init(mk_life_init);
module_exit(mk_life_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple device module");
MODULE_AUTHOR("Vladyslav Podilnyk");

