#include "ps2.h"
#include "irq.h"
#include "panic.h"
#include "print.h"
#include "port.h"
#include <kernel/vfs.h>
#include "ringbuf.h"
#include "string.h"
#include <kernel/kernel.h>
#include "time.h"
#include <input.h>

//this driver won't work on aarch64

#define PS2_DATA    0x60
#define PS2_COMMAND 0x64
#define PS2_STATUS  0x64

#define PS2_DISABLE_PORT1  0xAD
#define PS2_ENABLE_PORT1   0xAE
#define PS2_DISABLE_PORT2  0xA7
#define PS2_ENABLE_PORT2   0xA8

#define PS2_READ_CONF  0x20
#define PS2_WRITE_CONF 0x60
#define PS2_SET_SCANCODE_SET 0xF0

ring_buffer keyboard_queue;

static void wait_output(){
#ifdef x86_64
	while(!(in_byte(PS2_STATUS) & 0x01));
#else
#endif
}

static void wait_input(){
#ifdef x86_64
	while(in_byte(PS2_STATUS) & 0x02);
#else
#endif
}

static void ps2_send_command(uint8_t command){
	wait_input();
#ifdef x86_64
	out_byte(PS2_COMMAND,command);
#else
#endif
}

static uint8_t ps2_read(void){
	wait_output();
#ifdef x86_64
	return in_byte(PS2_DATA);
#else
	return 0;
#endif
}

static void ps2_write(uint8_t data){
	wait_input();
#ifdef x86_64
	out_byte(PS2_DATA,data);
#else
#endif
}

static void keyboard_write(uint8_t data){
	ps2_write(data);
}

#define ESC "\033"


const char kbd_us[128] = {
	0,0,
	'1','2','3','4','5','6','7','8','9','0',
	'-','=','\b',
	'\t', /* tab */
	'q','w','e','r','t','y','u','i','o','p','[',']','\n',
	0, /* control */
	'a','s','d','f','g','h','j','k','l',';','\'', '`',
	0, /* left shift */
	'\\','z','x','c','v','b','n','m',',','.','/',
	0, /* right shift */
	'*',
	0, /* alt */
	' ', /* space */
	0, /* caps lock */
	0, /* F1 [59] */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, /* ... F10 */
	0, /* 69 num lock */
	0, /* scroll lock */
	0, /* home */
	0, /* up arrow */
	0, /* page up */
	'-',
	0, /* left arrow */
	0,
	0, /* right arrow */
	'+',
	0, /* 79 end */
	0, /* down arrow */
	0, /* page down */
	0, /* insert */
	0, /* delete */
	0, 0, 0,
	0, /* F11 */
	0, /* F12 */
	0, /* everything else */
};

void keyboard_handler(fault_frame *frame){
	(void)frame;

	uint8_t scancode = ps2_read();
	int press = 1;
	if(scancode & 0x80){
		scancode -= 0x80;
		press = 0;
	}

	struct input_event event;
	event.timestamp = time;
	event.ie_class = IE_CLASS_KEYBOARD;
	event.ie_subclass = IE_SUBCLASS_PS2_KBD;
	event.ie_type = IE_KEY_EVENT;
	if(press){
		event.ie_key.flags = IE_KEY_PRESS;
	} else {
		event.ie_key.flags = IE_KEY_RELEASE;
	}

	if(kbd_us[scancode]){
		event.ie_key.c = kbd_us[scancode];
		event.ie_key.flags |= IE_KEY_GRAPH;
	}
	event.ie_key.scancode = scancode;

	ringbuffer_write(&event,&keyboard_queue,sizeof(event));
}
ssize_t kbd_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)node;
	(void)offset;
	return ringbuffer_read(buffer,&keyboard_queue,count);
}

vfs_node kbd_dev = {
	.read = kbd_read,
	.flags = VFS_DEV,
};


void init_ps2(void){
	kstatus("init ps2... ");

	//on aarch64 the driver won't work
#ifndef x86_64
	kfail();
	kinfof("the ps2 driver don't work on aarch64\n");
#endif
	//todo check if ps2 is present

	//disable
	ps2_send_command(PS2_DISABLE_PORT1);
	ps2_send_command(PS2_DISABLE_PORT2);

	//flush output
#ifdef x86_64
	in_byte(PS2_DATA);
#endif

	//modify configuration
	ps2_send_command(PS2_READ_CONF);
	uint8_t conf = ps2_read();

	conf |= 0x01 ; //Activate irq for port1

	//write conf back
	ps2_send_command(PS2_WRITE_CONF);
	ps2_write(conf);

	//re enable port 1
	ps2_send_command(PS2_ENABLE_PORT1);
#ifdef x86_64
	irq_generic_map(keyboard_handler,1);
#endif

	//set scancode set 3
	keyboard_write(PS2_SET_SCANCODE_SET);
	keyboard_write(2);

	//init queue
	keyboard_queue = new_ringbuffer(sizeof(struct input_event) * 25);

	//create device
	if(vfs_mount("/dev/kb0",&kbd_dev)){
		kfail();
		kinfof("fail to create dev /dev/kb0\n");
	}

	kok();
}
