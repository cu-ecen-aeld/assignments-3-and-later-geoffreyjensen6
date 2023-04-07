/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/slab.h>
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Geoffrey Jensen"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    //begin edits
    struct aesd_dev *new_device;
    new_device = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = new_device;
    //end edits
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    //begin edits
    struct aesd_dev *dev = filp->private_data;
    //dev->buffer_entry = kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    //memset(dev->buffer_entry, 0, sizeof(struct aesd_buffer_entry));
    char *read_buffer = (char *)kmalloc(count * sizeof(char *),GFP_KERNEL);
    size_t start_point;
    loff_t end_pos;
    //PDEBUG("dev buffer entry[0] is: %s", dev->buffer->entry[0].buffptr);
    //dev->buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer, 0, &start_point);
    PDEBUG("Start Point is %li in Entry %i", start_point, dev->buffer->out_offs);
    //read_buffer = dev->buffer_entry->buffptr;
    //PDEBUG("Read Buffer is: %s", read_buffer);
    end_pos = f_pos + count;
    PDEBUG("End Pos is %lld", end_pos);
    //copy_to_user(buf, read_buffer, strlen(read_buffer));
    kfree(read_buffer);
    //kfree(dev->buffer_entry);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    //begin edits
    struct aesd_dev *dev = filp->private_data;
    char newline = '\n';
    
    //Allocate write buffer space
    char *write_buffer = (char *)kmalloc(count * sizeof(char *),GFP_KERNEL);
    PDEBUG("Write Buffer Address is %p", write_buffer);
    if(!write_buffer){
	    goto exit;
    }
    memset(write_buffer, 0, count * sizeof(char *));

    //Copy contents from user space to kernel space
    if(copy_from_user(write_buffer, buf, count)){
	retval = -EFAULT;
	goto exit;
    }
    PDEBUG("Write Buffer is %s", write_buffer); 

    //Lock here. Locking all contents of dev by keeping all accesses to dev within the lock section
    //Makes it so other threads wouldn't make it past this unless they obtained lock. 
    //Release lock after updating first_packet in writing piece.
    mutex_lock_interruptible(&dev->lock);
    //Allocate new entry if it doesn't exist currently
    //if(dev->first_packet == 0){
    PDEBUG("First packet of write operation. Allocating memory");
    struct aesd_buffer_entry *buffer_entry = (struct aesd_buffer_entry *)kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    buffer_entry->buffptr = (char *)kmalloc(count * sizeof(char),GFP_KERNEL);

    PDEBUG("Initial INSTANCE OF PRINTING BUFFER");
    //aesd_print_circular_buffer(dev->buffer);

    PDEBUG("Setting memory to 0");
    //memset(buffer_entry, 0, sizeof(struct aesd_buffer_entry));
    memset(buffer_entry->buffptr, 0, (count* sizeof(sizeof(char))));
    PDEBUG("Memory is set to 0 now");
    buffer_entry->size = 0;
    dev->first_packet = 1;
    //}

    //Check for newline to indicate partial or complete write
    if(strchr(write_buffer, newline)){
	dev->end_of_packet = 1;
    }    
    
    //Add contents to entry
    //strcat(dev->buffer_entry->buffptr,write_buffer);
    PDEBUG("FIRST INSTANCE OF PRINTING BUFFER");
    //aesd_print_circular_buffer(dev->buffer);

    if(copy_from_user(buffer_entry->buffptr, buf, count)){
	retval = -EFAULT;
	goto exit;
    }
    //buffer_entry->buffptr = write_buffer;
    PDEBUG("Writing Entry: %s", buffer_entry->buffptr);
    //dev->buffer_entry->size = dev->buffer_entry->size + count;
    buffer_entry->size = count;
    
    //Check for newline to indicate partial or complete write
    if(dev->end_of_packet == 1){
	PDEBUG("SECOND INSTANCE OF PRINTING BUFFER");
	aesd_print_circular_buffer(dev->buffer);
    	//Since it is full packet, go ahead and write entry to circular buffer
	PDEBUG("Writing entry to buffer and freeing");
        aesd_circular_buffer_add_entry(dev->buffer, buffer_entry);
	PDEBUG("dev buffer entry[0] is: %s", dev->buffer->entry[0].buffptr);
	PDEBUG("dev buffer entry[1] is: %s", dev->buffer->entry[1].buffptr);
	kfree(dev->buffer_entry);
	dev->first_packet = 0;
    }
    mutex_unlock(&dev->lock);
    PDEBUG("Freeing write buffer");
    kfree(write_buffer);
    retval = count;

    exit:
	//end edits
	return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
    */
    //beginning of edits
    aesd_device.buffer = (struct aesd_circular_buffer *)kmalloc(sizeof(struct aesd_circular_buffer),GFP_KERNEL);
    //aesd_device.buffer_entry = kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    aesd_device.end_of_packet = 0;
    aesd_device.first_packet = 0;
    aesd_device.buffer->in_offs = 0;
    aesd_device.buffer->out_offs = 0;
    mutex_init(&aesd_device.lock);
    //aesd_device.buffer_lock = kmalloc(sizeof(struct semaphore),GFP_KERNEL);
    memset(aesd_device.buffer,0,sizeof(struct aesd_circular_buffer));
    //memset(aesd_device.buffer_entry,0,sizeof(struct aesd_buffer_entry));
    //end of edits

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
