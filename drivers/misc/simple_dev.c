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
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>

#define TRUE			1
#define FALSE			0
#define SIMPLE_DEV_IRQ	53
#define CBUFFER_SIZE	16
#define VIEW_SIZE		30
#define START_COMM	"start"
#define STOP_COMM	"stop"
#define CLEAR_COMM	"clear"
#define COMM_LEN		6
#define MEM_CHUNK		4000


//
// Memory allocator part
//
typedef struct {
	unsigned int prev_block_size:31;
	unsigned int is_free:1;
	unsigned int block_size:32;
} header_t;

void *gp_to_buffer;
size_t g_buffer_size;

int init_buffer(int order)
{
	header_t *buffer;
	gp_to_buffer = (void*) __get_free_pages(GFP_KERNEL, order);
	g_buffer_size = (order + 1) * MEM_CHUNK;
	if (!gp_to_buffer) {
		g_buffer_size = 0;
		return -1;
	}
	gp_to_buffer = (void*)((size_t)gp_to_buffer + (4 - ((size_t)gp_to_buffer % 4)));
	buffer = (header_t*)gp_to_buffer;
	buffer->prev_block_size = 0;
	buffer->is_free = TRUE;
	buffer->block_size = g_buffer_size;
	return 0;
}


void mem_free(void *mem_block)
{
	header_t *current_block, *prev_block, *next_block;

	current_block = (header_t*)mem_block - 1;
	prev_block = (header_t*)((size_t)current_block - current_block->prev_block_size);
	next_block = (header_t*)((size_t)current_block + current_block->block_size);

	current_block->is_free = TRUE;
	if ((current_block != prev_block) && ((size_t)prev_block >= (size_t)gp_to_buffer)
	&& (prev_block->is_free)) {
		prev_block->block_size += current_block->block_size;
		if ((size_t)next_block < (size_t)gp_to_buffer + g_buffer_size)
			next_block->prev_block_size = prev_block->block_size;
		current_block = prev_block;
	}

	if (((size_t)next_block < (size_t)gp_to_buffer + g_buffer_size) && (next_block->is_free)) {
		current_block->block_size += next_block->block_size;
		if ((size_t)next_block + next_block->block_size < (size_t)gp_to_buffer + g_buffer_size) {
			next_block = (header_t*)((size_t)next_block + next_block->block_size);
			next_block->prev_block_size = current_block->block_size;
		}
	}

}


void *mem_alloc(size_t size)
{
	header_t *current_block = (header_t*)gp_to_buffer;
	header_t *next_block;
	size_t header_size = sizeof(header_t);
	size_t new_size = size + header_size;

	new_size = new_size + (4 - (new_size % 4));

	while ((size_t)current_block < ((size_t)gp_to_buffer + g_buffer_size)) {
		if ((current_block->is_free) && (current_block->block_size >= new_size)) {
			if (current_block->block_size - new_size <= header_size)
				new_size = current_block->block_size;

			else {
				next_block = (header_t*)((size_t)current_block + new_size);
				next_block->is_free = TRUE;
				next_block->prev_block_size = new_size;
				next_block->block_size = current_block->block_size - new_size;
				if (((size_t)next_block + next_block->block_size) < ((size_t)gp_to_buffer + g_buffer_size)) {
					next_block = (header_t*)((size_t)next_block + next_block->block_size);
					next_block->prev_block_size = current_block->block_size - new_size;
				}
			}
			current_block->is_free = FALSE;
			current_block->block_size = new_size;
			return (void*)(current_block + 1);
		}
		current_block = (header_t*)((size_t)current_block + current_block->block_size);
	}

	return NULL;
}

#undef TRUE
#undef FALSE


static struct uart_port *uap;

static struct device_meta_data
{
	spinlock_t    lock;
	unsigned long irq_flags;

	struct debugfs_data
	{
		struct dentry *dir;
		struct dentry *cfile; // counter
		struct dentry *tfile; // timestamp
	} debugfs;

	struct dev_counter
	{
		int value;
		int prev_value;
		char *str_val;
	} counter;

	struct dev_timestamp
	{
		struct record
		{
			int counter;
			struct timeval time;
		} *rec;
		int index;      // index of the current element
		int prev_value; // reported counter value
		char status;    // 0 - Start; 1 - Stop;
		char *str_val;
	} timestamp;

} *data;

//
// File operaions for uart-pl011-count
//
static ssize_t dev_counter_read(struct file *dst_file, char *buf, size_t len, loff_t *offset)
{
	int cnt_value;
	int error = 1;
	int bytes_to_write, bytes_written = 0;

	spin_lock_irqsave(&data->lock, data->irq_flags);
	cnt_value = data->counter.value;
	spin_unlock_irqrestore(&data->lock, data->irq_flags);

	if (data->counter.prev_value != cnt_value) {
		bytes_written = snprintf(data->counter.str_val, VIEW_SIZE, "%i\n", cnt_value);
		bytes_to_write = bytes_written > len ? len : bytes_written;
		error = copy_to_user(buf, data->counter.str_val, bytes_to_write);
		data->counter.prev_value = cnt_value;
	}

	if (!error)
		return bytes_to_write;

	return 0;
}


static ssize_t dev_counter_write(struct file *dst_file, const char *buf, size_t len, loff_t *offset)
{
	unsigned int value;
	int error = kstrtouint_from_user(buf, len, 10, &value);
	if (error < 0)
		return 0;
	spin_lock_irqsave(&data->lock, data->irq_flags);
	data->counter.value = value;
	spin_unlock_irqrestore(&data->lock, data->irq_flags);
	return value;
}


static struct file_operations counter_fops =
{
	.read = dev_counter_read,
	.write = dev_counter_write,
};

//
// File operations for uart-pl011-timestamp
//
static ssize_t dev_timestamp_read(struct file *dst_file, char *buf, size_t len, loff_t *offset)
{
	struct record curr_timestamp;
	int bytes_written, bytes_to_write;
	int error;
	spin_lock_irqsave(&data->lock, data->irq_flags);
	curr_timestamp = *(data->timestamp.rec + data->timestamp.index);
	spin_unlock_irqrestore(&data->lock, data->irq_flags);
	if (curr_timestamp.counter != data->timestamp.prev_value) {
		bytes_written = snprintf(data->timestamp.str_val, VIEW_SIZE,
								"%i %02u-%02u-%03u\n",
								curr_timestamp.counter,
								(unsigned int)((curr_timestamp.time.tv_sec / 60) % 60),
								(unsigned int)curr_timestamp.time.tv_sec % 60,
								(unsigned int)curr_timestamp.time.tv_usec / 1000
								);
		bytes_to_write = bytes_written > len ? len : bytes_written;
		error = copy_to_user(buf, data->timestamp.str_val, bytes_to_write);
		data->timestamp.prev_value = curr_timestamp.counter;
	}
	if (!error)
		return bytes_to_write;
	return 0;
}


static ssize_t dev_timestamp_write(struct file *dst_file, const char *buf, size_t len, loff_t *offset)
{
	char *command = mem_alloc(len);
	int str_len = strncpy_from_user(command, buf, len);
	int bytes_to_cmp = len - 1;
	if (str_len < 0)
		return 0;

	if (str_len <= COMM_LEN) {
		spin_lock_irqsave(&data->lock, data->irq_flags);
		if (strncmp(command, START_COMM, bytes_to_cmp) == 0) {
			data->timestamp.status = 0;
			printk("Start recording.\n");
		}
		else if (strncmp(command, STOP_COMM, bytes_to_cmp) == 0) {
			data->timestamp.status = 1;
			printk("Stop recording.\n");
		}
		else if (strncmp(command, CLEAR_COMM, bytes_to_cmp) == 0) {
			data->timestamp.index = 0;
			data->timestamp.prev_value = -1;
			memset(data->timestamp.rec, 0, CBUFFER_SIZE * sizeof(struct record));
			printk("Clear records.\n");
		}
		spin_unlock_irqrestore(&data->lock, data->irq_flags);
	}

	mem_free(command);
	return str_len;
}


static struct file_operations timestamp_fops =
{
	.read = dev_timestamp_read,
	.write = dev_timestamp_write,
};


static irqreturn_t simple_irq_handler_top(int irq, void *dev_id)
{
	spin_lock_irqsave(&data->lock, data->irq_flags);
	++data->counter.value;
	if (data->timestamp.status == 0) {
		(data->timestamp.rec + data->timestamp.index)->counter = data->counter.value;
		do_gettimeofday(&(data->timestamp.rec + data->timestamp.index)->time);
		data->timestamp.index = (data->timestamp.index + 1) & (CBUFFER_SIZE - 1);
	}
	spin_unlock_irqrestore(&data->lock, data->irq_flags);
	return IRQ_HANDLED;
}


static int __init mk_life_init(void)
{
	int ret;
	init_buffer(0);

	// init data
	data = mem_alloc(sizeof(struct device_meta_data));
	data->timestamp.rec = mem_alloc(sizeof(struct record) * CBUFFER_SIZE);
	data->counter.str_val = mem_alloc(VIEW_SIZE);
	data->timestamp.str_val = mem_alloc(VIEW_SIZE);
	data->timestamp.index = 0;
	data->timestamp.prev_value = -1;
	data->counter.value = 0;
	data->counter.prev_value = -1;
	data->timestamp.status = 0;
	uap = mem_alloc(sizeof(struct uart_port));

	if (!data
		|| !uap
		|| !data->timestamp.rec
		|| !data->counter.str_val) {
		ret = -ENOMEM;
		goto ret_fail_alloc_mem;
	}
	
	spin_lock_init(&data->lock);

	data->debugfs.dir = debugfs_create_dir("simple-dev", NULL);
	if (!data->debugfs.dir) {
		ret = -ENOENT;
		goto ret_no_such_dir;
	}

	if ( (int)data->debugfs.dir == -ENODEV) {
        ret = -ENODEV;
        goto ret_no_such_dir;
    }
	
	data->debugfs.cfile = debugfs_create_file("uart-pl011-count", S_IRUSR | S_IRWXU, data->debugfs.dir, NULL, &counter_fops);
	data->debugfs.cfile = debugfs_create_file("uart-pl011-timestamp", S_IRUSR | S_IRWXU, data->debugfs.dir, NULL, &timestamp_fops);
	if (!data->debugfs.cfile) {
		ret = -ENOENT;
		goto ret_no_such_file;
	}

	if ( (int)data->debugfs.cfile == -ENODEV) {
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
	kfree(data);
	return ret;

ret_no_such_file:
	debugfs_remove_recursive(data->debugfs.dir);
	kfree(data);
	return ret;

ret_no_such_dev:
	debugfs_remove_recursive(data->debugfs.dir);
	kfree(data);
	return ret;
}


static void __exit mk_life_exit(void)
{	
	free_irq(SIMPLE_DEV_IRQ, uap);
	free_pages((unsigned long)gp_to_buffer, 0);
	debugfs_remove_recursive(data->debugfs.dir);
	printk("Congrats!!! Your life is simple.\n");
}


module_init(mk_life_init);
module_exit(mk_life_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple device module");
MODULE_AUTHOR("Vladyslav Podilnyk");

