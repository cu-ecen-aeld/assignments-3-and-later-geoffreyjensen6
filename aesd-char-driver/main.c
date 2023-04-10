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
    struct aesd_dev *dev = filp->private_data;
    //struct aesd_buffer_entry *buffer_entry;
    char *read_buffer;
    mutex_lock_interruptible(&dev->lock);
    int i=0;
    //dev->buffer_entry = (struct aesd_buffer_entry*)kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    read_buffer = (char *)kmalloc(count * sizeof(char *),GFP_KERNEL); 
    //memset(dev->buffer_entry, 0, sizeof(struct aesd_buffer_entry));
    //buffer_entry = (struct aesd_buffer_entry *)kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    size_t rtn_byte;
    loff_t entry_pos;
    rtn_byte = 0;
    entry_pos = 0;
    PDEBUG("fpos before is %lld", *f_pos);
    PDEBUG("Beginning Read of Entry");
    dev->read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer, *f_pos, &rtn_byte);
    PDEBUG("Done Read of Entry");
    if(!dev->read_entry){
	mutex_unlock(&dev->lock);

    	return 0;
    }
    PDEBUG("return byte is %li", rtn_byte);
    *f_pos = *f_pos + dev->read_entry->size;
    PDEBUG("fpos after is %lld", *f_pos);
    retval = retval + dev->read_entry->size;
    strcat(read_buffer,dev->read_entry->buffptr);

    /*for(i=0; i<=9; i++){
    	buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer, entry_pos, &rtn_byte);
    	if(!buffer_entry){
	    	return 0;
    	}
	PDEBUG("return byte is %li", rtn_byte);
	entry_pos = entry_pos + buffer_entry->size;
	retval = retval + buffer_entry->size;
	strcat(read_buffer,buffer_entry->buffptr);
	PDEBUG("Read buffer is %s", read_buffer);
	//kfree(buffer_entry);
    }*/
    PDEBUG("Read buffer is %s", read_buffer);
    PDEBUG("Retval is %li", retval);
    copy_to_user(buf, read_buffer, (strlen(read_buffer)+1));
    kfree(read_buffer);
    //kfree(buffer_entry);
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    size_t new_length = 0;
    struct aesd_dev *dev = filp->private_data;
    char newline = '\n';
    char *write_buffer;
    const char *buffer_to_free;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    
    mutex_lock_interruptible(&dev->lock);
    write_buffer = (char *)kmalloc(count * sizeof(char *),GFP_KERNEL); 
    if(!write_buffer){
	    goto exit;
    }
    memset(write_buffer, 0, count * sizeof(char *));
    PDEBUG("Write_buffer initial value=%s and length=%li",write_buffer,strlen(write_buffer));

    //Copy contents from user space to kernel space
    if(copy_from_user(write_buffer, buf, count)){
	retval = -EFAULT;
	goto exit;
    }

    //Check for newline to indicate partial or complete write
    if(strchr(write_buffer, newline)){
	dev->end_of_packet = 1;
    }

    if(dev->first_packet == 0){
	new_length = strlen(write_buffer)+1;
	PDEBUG("Calling Malloc for buffptr");
	dev->buffer_entry->buffptr = (char *)kmalloc(new_length * sizeof(char),GFP_KERNEL);
	memset(dev->buffer_entry->buffptr, 0, new_length * sizeof(char));
    }
    else{
	new_length = dev->buffer_entry->size + strlen(write_buffer)+1;
	PDEBUG("Calling Realloc for buffptr");
	dev->buffer_entry->buffptr = (char *)krealloc((void *)dev->buffer_entry->buffptr, new_length * sizeof(char),GFP_KERNEL);
    }

    if(!dev->buffer_entry->buffptr){
        goto exit;
    }
    PDEBUG("dev-buffer_entry->buffptr is %p", dev->buffer_entry->buffptr);
    dev->buffer_entry->buffptr = strcat(dev->buffer_entry->buffptr, write_buffer);
    retval = count;
    PDEBUG("Writing Entry: %s", dev->buffer_entry->buffptr);
    dev->buffer_entry->size = new_length;
    dev->first_packet = 1;
    //}


    /*PDEBUG("dev-buffer_entry value is %p", dev->buffer_entry);
    dev->buffer_entry->buffptr = (char *)kmalloc((strlen(write_buffer)+1) * sizeof(char),GFP_KERNEL);
    PDEBUG("dev-buffer_entry->buffptr value is %p", dev->buffer_entry->buffptr);
    if(!dev->buffer_entry->buffptr){
	    goto exit;
    }
    dev->buffer_entry->size = 0; 
    */
    
    //Add contents to entry
/*    if(copy_from_user(dev->buffer_entry->buffptr, buf, count)){
	retval = -EFAULT;
	goto exit;
    }
*/
        
    //Check for newline to indicate partial or complete write
    if(dev->end_of_packet == 1){
    	//Since it is full packet, go ahead and write entry to circular buffer
	PDEBUG("Writing entry to buffer and freeing");
        buffer_to_free = aesd_circular_buffer_add_entry(dev->buffer, dev->buffer_entry);
	if(buffer_to_free != NULL){
	    PDEBUG("Freeing buffer with contents %s", buffer_to_free);
	    kfree(buffer_to_free);
	}
	dev->first_packet = 0;
	dev->end_of_packet = 0;
    }
    kfree(write_buffer);
    PDEBUG("Freeing write buffer");
    PDEBUG("Retval is %ld", retval);
        
    exit:
	//end edits
	mutex_unlock(&dev->lock);
	PDEBUG("Another Print");
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
    aesd_device.buffer_entry = (struct aesd_buffer_entry *)kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    aesd_device.read_entry = (struct aesd_buffer_entry *)kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL); 
    aesd_device.end_of_packet = 0;
    aesd_device.first_packet = 0;
    aesd_device.buffer->in_offs = 0;
    aesd_device.buffer->out_offs = 0;
    mutex_init(&aesd_device.lock);
    //aesd_device.buffer_lock = kmalloc(sizeof(struct semaphore),GFP_KERNEL);
    memset(aesd_device.buffer,0,sizeof(struct aesd_circular_buffer));
    memset(aesd_device.buffer_entry,0,sizeof(struct aesd_buffer_entry));
    memset(aesd_device.read_entry,0,sizeof(struct aesd_buffer_entry));
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
    PDEBUG("CLEANING UP");
    kfree(aesd_device.buffer);
    kfree(aesd_device.buffer_entry);
    kfree(aesd_device.read_entry);


    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
