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
    struct aesd_dev *aesd_device;
    aesd_device = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = aesd_device;
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
    //aesd_device = filp->private_data;
    char newline = '\n';
    
    //Allocate write buffer space
    aesd_device.write_buffer = (char *)kmalloc(count * sizeof(char *),GFP_KERNEL);
    PDEBUG("Size of write_buffer is %li", sizeof(aesd_device.write_buffer));
    if(!aesd_device.write_buffer){
	    goto out;
    }
    memset(aesd_device.write_buffer, 0, count * sizeof(char *));

    //Allocate new entry
    if(aesd_device.end_of_packet == 0){
	aesd_device.buffer_entry = kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
	memset(aesd_device.buffer_entry, 0, sizeof(struct aesd_buffer_entry));
    }
    
    //Copy contents to kernel space
    if(copy_from_user(aesd_device.write_buffer, buf, count)){
	retval = -EFAULT;
	goto out;
    }
    PDEBUG("Write Buffer is %s", aesd_device.write_buffer);
    aesd_device.buffer_entry->buffptr = aesd_device.write_buffer;
    aesd_device.buffer_entry->size = count;

    //Check for newline to indicate partial or complete write
    if(strchr(aesd_device.write_buffer, newline)){
	aesd_device.end_of_packet = 1;
    }
    PDEBUG("End of Packet is: %i", aesd_device.end_of_packet);
    
    //Since it is full packet, go ahead and write entry to circular buffer
    if(aesd_device.end_of_packet == 1){
        aesd_circular_buffer_add_entry(aesd_device.buffer, aesd_device.buffer_entry);
	kfree(aesd_device.buffer_entry);
	aesd_device.end_of_packet = 0;
    }

    kfree(aesd_device.write_buffer);
    retval = count;

    out:
    
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
    aesd_device.buffer = kmalloc(sizeof(struct aesd_circular_buffer),GFP_KERNEL);
    //aesd_device.buffer_entry = kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    aesd_device.end_of_packet = 0;
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