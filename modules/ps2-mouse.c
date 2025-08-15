#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/ringbuf.h>
#include <kernel/arch.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <module/ps2.h>
#include <errno.h>
#include <input.h>

#define PS2_SET_RATE 0xF3


static int set_mouse_rate(int port,int rate){
	if(ps2_send(port,PS2_SET_RATE) != PS2_ACK){
		return -1;
	}
	if(ps2_send(port,rate) != PS2_ACK){
		return -1;
	}
	return 0;
}

static void mouse_handler(fault_frame *frame){
	(void)frame;
	int flags = ps2_read();
	int x = ps2_read();
	int y = ps2_read();
	if(flags & (1 << 4)){
		x = -x;
	}
	if(flags & (1 << 5)){
		y = -y;
	}
	kdebugf("button %d %d",flags & 2,flags & 1);
	kdebugf("mouse movement %d:%d",x,y);
}

ring_buffer mouse_buffer;

static ssize_t mouse_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	(void)node;

	return ringbuffer_read(buffer,&mouse_buffer,count);
}

int init_mouse(int argc,char **argv){
	(void)argc;
	(void)argv;
	//only check port 2 for the moment

	if(ps2_port_id[2][0]){
		kdebugf("ps2 : no usable ps2 mouse found\n");
		return -ENODEV;
	}

	//first do a reset
	if(ps2_send(2,0xFF) != PS2_ACK){
		kdebugf("ps2 : mouse reset failed\n");
		return -ENODEV;
	}
	if(ps2_read() != 0xAA){
		kdebugf("ps2 : mouse didn't pass self test\n");
		return -ENODEV;
	}
	//discard the id of the mouse we aready know that
	ps2_read();

	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->read = mouse_read;
	if(ps2_send(2,PS2_ENABLE_SCANING) != PS2_ACK){
		kdebugf("ps2 : error while enable scanning\n");
		return -EIO;
	}

	mouse_buffer = new_ringbuffer(sizeof(struct input_event) * 32);

	ps2_register_handler(mouse_handler,2);
	return 0;
}

int fini_mouse(){
	delete_ringbuffer(&mouse_buffer);
	return 0;
}


kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_mouse,
	.fini = fini_mouse,
	.author = "tayoky",
	.name = "ps2 mouse",
	.description = "a ps2 mouse driver",
	.license = "GPL 3",
};