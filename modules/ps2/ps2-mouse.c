#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/input.h>
#include <kernel/kheap.h>
#include <kernel/ringbuf.h>
#include <kernel/arch.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <module/ps2.h>
#include <errno.h>
#include <input.h>

#define PS2_SET_RATE 0xF3

static device_driver_t ps2_mouse_driver;

static int set_mouse_rate(int port,int rate){
	if(ps2_send(port,PS2_SET_RATE) != PS2_ACK){
		return -1;
	}
	if(ps2_send(port,rate) != PS2_ACK){
		return -1;
	}
	return 0;
}

static void mouse_handler(fault_frame *frame, void *arg){
	(void)frame;
	input_device_t *mouse = arg;
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

static int mouse_check(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t*)addr;
	if (addr->type != BUS_PS2) return 0;
	if (ps2_addr->device_id[0]) return 0;
	kdebugf("found ps2 mouse on port %d\n", ps2_addr->port);
	return 1;
}

static int mouse_probe(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t*)addr;
	int port = ps2_addr->port;

	// first do a reset
	if (ps2_send(port, 0xFF) != PS2_ACK) {
		kdebugf("mouse reset failed\n");
		return -EIO;
	}
	if (ps2_read() != 0xAA) {
		kdebugf("mouse didn't pass self test\n");
		return -EIO;
	}
	// discard the id of the mouse we aready know that
	ps2_read();
	ps2_read();

	if (ps2_send(port, PS2_ENABLE_SCANING) != PS2_ACK) {
		kdebugf("error while enabling scanning\n");
		return -EIO;
	}

	input_device_t *mouse = kmalloc(sizeof(input_device_t));
	memset(mouse, 0, sizeof(input_device_t));
	mouse->device.number = port;
	mouse->device.driver = &ps2_mouse_driver;
	mouse->device.name = strdup("mouse0");
	mouse->device.addr = addr;
	register_input_device(mouse);
	ps2_register_handler(mouse_handler,port,mouse);
	return 0;
}

static device_driver_t ps2_mouse_driver = {
	.name = "ps2 mouse",
	.check = mouse_check,
	.probe = mouse_probe,
};

int init_mouse(int argc,char **argv){
	(void)argc;
	(void)argv;
	register_device_driver(&ps2_mouse_driver);
	return 0;
}

int fini_mouse(){
	unregister_device_driver(&ps2_mouse_driver);
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
