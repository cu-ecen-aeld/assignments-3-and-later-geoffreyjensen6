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
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Geoffrey Jensen"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    /**
     * TODO: handle open
     */
    //begin edits
    struct aesd_dev *new_device;
    PDEBUG("open");
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
    struct aesd_dev *dev = filp->private_data;
    char *read_buffer;
    size_t rtn_byte;
    loff_t entry_pos;
    
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    //Lock
    if(mutex_lock_interruptible(&dev->lock)){
	return -ERESTARTSYS;
    }	
    
    rtn_byte = 0;
    entry_pos = 0;

    //Find entry in circular buffer
    PDEBUG("Beginning Read of Entry");
    dev->read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer, *f_pos, &rtn_byte);
    PDEBUG("Done Read of Entry");
    if(!dev->read_entry){
	goto exit;
    }

    //Modify the file position pointer which is passed in to indicate where it stands after the read
    PDEBUG("rtn_byte was %li", rtn_byte);

    *f_pos = *f_pos + dev->read_entry->size - rtn_byte;
    retval = retval + dev->read_entry->size - rtn_byte;

    //Malloc local buffer
    read_buffer = (char *)kmalloc(retval * sizeof(char *),GFP_KERNEL); 
    if(!read_buffer){
	    goto exit;
    }
    memset(read_buffer, 0, (retval * sizeof(char*)));

    //Copy the value from circular buffer to local buffer
    //strcat(read_buffer,dev->read_entry->buffptr);
    PDEBUG("Buffer from read position is %s and length is %li", (dev->read_entry->buffptr + rtn_byte), dev->read_entry->size);
    strncpy(read_buffer, (dev->read_entry->buffptr + rtn_byte), (dev->read_entry->size - rtn_byte));


    //Copy from kernel space to user space
    PDEBUG("Read buffer is %s and length is %li", read_buffer, strlen(read_buffer));
    PDEBUG("Retval is %li", retval);
    if(copy_to_user(buf, read_buffer, (retval+1))){
	retval = -EFAULT;
	goto exit;
    }
    
    exit:
	kfree(read_buffer);
	PDEBUG("Freed read buffer");
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
    
    //Lock
    if(mutex_lock_interruptible(&dev->lock)){
	return -ERESTARTSYS;
    }

    //Malloc local buffer
    write_buffer = (char *)kmalloc(count * sizeof(char *),GFP_KERNEL); 
    if(!write_buffer){
	    goto exit;
    }
    memset(write_buffer, 0, count * sizeof(char *));

    //Copy contents from user space to local buffer in kernel space
    if(copy_from_user(write_buffer, buf, count)){
	retval = -EFAULT;
	goto exit;
    }

    //Check for newline to indicate partial or complete write
    if(strchr(write_buffer, newline)){
	dev->end_of_packet = 1;
    }

    //Maloc buffer and size depending on whether this is a new message or a continuation from a previous
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

    //If Malloc fail, go to exit
    if(!dev->buffer_entry->buffptr){
        goto exit;
    }

    dev->buffer_entry->buffptr = strcat(dev->buffer_entry->buffptr, write_buffer);
    retval = count;
    PDEBUG("Writing Entry: %s", dev->buffer_entry->buffptr);
    dev->buffer_entry->size = new_length;
    dev->first_packet = 1;
        
    //Add to circular buffer if end of packet
    if(dev->end_of_packet == 1){
	PDEBUG("Writing entry to buffer and freeing");
        buffer_to_free = aesd_circular_buffer_add_entry(dev->buffer, dev->buffer_entry);
	*f_pos = *f_pos + dev->buffer_entry->size;
	if(buffer_to_free != NULL){
		PDEBUG("Freeing buffer with contents %s", buffer_to_free);
		kfree(buffer_to_free);
	}
	dev->first_packet = 0;
	dev->end_of_packet = 0;
	PDEBUG("Made it to end of adding entry");
    }
            
    exit:
	kfree(write_buffer);
	PDEBUG("Freeing write buffer");
	PDEBUG("Retval is %ld", retval);
	mutex_unlock(&dev->lock);
	return retval;
}

loff_t aesd_seek(struct file *filp, loff_t offset, int whence){
    struct aesd_dev *dev = filp->private_data;
    loff_t requested_location;
    loff_t buffer_size;
    buffer_size = get_aesd_buffer_length(dev->buffer);
    requested_location = fixed_size_llseek(filp, offset, whence, buffer_size);
    PDEBUG("Requested Position is %li for offset %li", requested_location, offset);
    filp->f_pos = requested_location;
/*
    // Seek Current
    if(seek_op == 1){
	requested_location = filp->f_pos + offset;
    }
    // Seek End
    else if(seek_op == 2){
	requested_location = dev->size + offset;
    }
    // Seek Set has no action
    else if (seek_op != 0){
	return EINVAL;
    }
    //Need to make a variable part of dev called size and incrase or decrease whenever and entyr changes`

   filp->f_pos = requested_location;
   */

   return requested_location;

}

static long aesd_adjust_file_offset(struct file *filp,unsigned int write_cmd, unsigned int write_cmd_offset){
	struct aesd_dev *dev = filp->private_data;
	uint8_t seeked_out_offs;
	uint8_t current_out_offs;
	size_t total_size;
	int i;
	loff_t requested_position;
	PDEBUG("IN IOCTL FUNCTION");
	if(write_cmd > 10 || write_cmd < 0){
		PDEBUG("Invalid Write Command");
		return -EINVAL;
	}
	seeked_out_offs = (dev->buffer->out_offs + write_cmd) % 11;
	PDEBUG("Previously dev->buffer->out_offs was %i and now it will be %i", dev->buffer->out_offs, seeked_out_offs);
	if(dev->buffer->entry[seeked_out_offs].size < write_cmd_offset){
		PDEBUG("Invalid Write Command Offset");
		PDEBUG("Write command offset is %i and buffer entry has size %li", write_cmd_offset, dev->buffer->entry[seeked_out_offs].size);
		return -EINVAL;
	}
	PDEBUG("Valid Write command and command offset");
	
	current_out_offs = 0;
	total_size = 0;
    	//dev->read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer, *f_pos, &rtn_byte);
	PDEBUG("f_pos is %lld. Updating now...", filp->f_pos);
	for(i=0;i<=10;i++){
		current_out_offs = dev->buffer->out_offs + i;
		if(i != write_cmd){
			total_size = dev->buffer->entry[current_out_offs].size + total_size;
			PDEBUG("Not at right entry yet. Adding %li to total which is now %li", dev->buffer->entry[current_out_offs].size, total_size);
			continue;
		}
		total_size = total_size + write_cmd_offset;
		PDEBUG("At right entry now. Adding %li to total which is now %li", write_cmd_offset, total_size);
		break;
	}

	filp->f_pos = total_size;
	PDEBUG("f_pos is now %lld", filp->f_pos);

	return 0;	
}

long aesd_ioctl(struct file *filp,unsigned int cmd, unsigned long arg){
    int retval = 0;
    switch(cmd) {
	    case AESDCHAR_IOCSEEKTO:
	    {
		struct aesd_seekto seekto;
		if(copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0){
		    retval = EFAULT;
		}
		else{
		    retval = aesd_adjust_file_offset(filp,seekto.write_cmd,seekto.write_cmd_offset);
		}
		break;
	    }
	    default:
	    {
		return -ENOTTY;
	    }
   }
   return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =	aesd_seek,
    .unlocked_ioctl = aesd_ioctl,
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

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    kfree(aesd_device.read_entry);
    kfree(aesd_device.buffer_entry);
    kfree(aesd_device.buffer); 

    unregister_chrdev_region(devno, 1);
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
    aesd_device.buffer = (struct aesd_circular_buffer *)kmalloc(sizeof(struct aesd_circular_buffer),GFP_KERNEL);
    if(!aesd_device.buffer){
	    goto fail;
    }
    aesd_device.buffer_entry = (struct aesd_buffer_entry *)kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL);
    if(!aesd_device.buffer_entry){
	    goto fail;
    }
    aesd_device.read_entry = (struct aesd_buffer_entry *)kmalloc(sizeof(struct aesd_buffer_entry),GFP_KERNEL); 
    if(!aesd_device.read_entry){
	    goto fail;
    }
    memset(aesd_device.buffer,0,sizeof(struct aesd_circular_buffer));
    memset(aesd_device.buffer_entry,0,sizeof(struct aesd_buffer_entry));
    memset(aesd_device.read_entry,0,sizeof(struct aesd_buffer_entry));
    aesd_device.end_of_packet = 0;
    aesd_device.first_packet = 0;
    aesd_device.buffer->in_offs = 0;
    aesd_device.buffer->out_offs = 0;
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

    fail:
    	aesd_cleanup_module();
	return result;
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
