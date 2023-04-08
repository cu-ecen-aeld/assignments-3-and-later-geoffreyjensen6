/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */
#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/printk.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#include "aesd-circular-buffer.h"

#define BUFFER_SIZE 10

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer, size_t char_offset, size_t *entry_offset_byte_rtn )
{
	struct aesd_buffer_entry *output_entry;
	size_t entry_size;
	size_t total_end_entry = 0;
	size_t total_beginning_entry = 0;
	int loop_count = 0;
	uint8_t restore_out_offs;

	//Find where in_offs is when this function is called so they can be restored before exit
	restore_out_offs = buffer->out_offs;
	//PDEBUG("Location of out_offs pointer at start: %i\n", restore_out_offs);

	while(1){
		loop_count = loop_count + 1;
		entry_size = buffer->entry[buffer->out_offs].size;
		//PDEBUG("Entry size is %li\n", entry_size);
		//PDEBUG("Entry is %s\n", buffer->entry[buffer->out_offs].buffptr);
		total_beginning_entry = total_end_entry;
		total_end_entry	= entry_size + total_beginning_entry;
		//PDEBUG("Total @ BE is %li, @EE is %li and char_offset is %li\n", total_beginning_entry, total_end_entry, char_offset);
		if(char_offset >= total_end_entry){
			buffer->out_offs = buffer->out_offs + 1;
			if(buffer->out_offs >= 10){
				buffer->out_offs = 0;
			}
			//PDEBUG("next Out_offs is %i\n", buffer->out_offs);
			if(loop_count >= BUFFER_SIZE){
				buffer->out_offs = restore_out_offs;
				//PDEBUG("Location of in_offs pointer at exit: %i\n", buffer->out_offs);
				return NULL;
			}		
			continue;
		}
		else if(char_offset >= total_beginning_entry){
			*entry_offset_byte_rtn = char_offset - total_beginning_entry;
			//PDEBUG("Out Offs = %i\n", buffer->out_offs);
			output_entry = &buffer->entry[buffer->out_offs];
			buffer->out_offs = restore_out_offs;
			//PDEBUG("Location of in_offs pointer at exit: %i\n", buffer->out_offs);
			return output_entry;
		}
	}
	
	buffer->out_offs = restore_out_offs;
	return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
	if(buffer->entry[buffer->in_offs].buffptr != NULL){
		PDEBUG("Buffer is Full!!!!!!!!!!!!!!!!\n");
		buffer->out_offs = buffer->out_offs + 1;
	}
	buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
	buffer->entry[buffer->in_offs].size = strlen(add_entry->buffptr);	
	buffer->in_offs = buffer->in_offs + 1;
	if(buffer->in_offs >= BUFFER_SIZE){
		buffer->in_offs = 0;
	}
	PDEBUG("FINAL INSTANCE OF CIRCULAR BUFFER");
	aesd_print_circular_buffer(buffer);
}

void aesd_print_circular_buffer(struct aesd_circular_buffer *buffer)
{
	PDEBUG("Buffer Entry 0 = %s", buffer->entry[0].buffptr);
	PDEBUG("Buffer Entry 1 = %s", buffer->entry[1].buffptr);
	PDEBUG("Buffer Entry 2 = %s", buffer->entry[2]);
	PDEBUG("Buffer Entry 3 = %s", buffer->entry[3]);
	PDEBUG("Buffer Entry 4 = %s", buffer->entry[4]);
	PDEBUG("Buffer Entry 5 = %s", buffer->entry[5]);
	PDEBUG("Buffer Entry 6 = %s", buffer->entry[6]);
	PDEBUG("Buffer Entry 7 = %s", buffer->entry[7]);
	PDEBUG("Buffer Entry 8 = %s", buffer->entry[8]);
	PDEBUG("Buffer Entry 9 = %s", buffer->entry[9]);
	PDEBUG("Buffer Entry 10 = %s", buffer->entry[10]);
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
