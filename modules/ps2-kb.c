#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/ringbuf.h>
#include <kernel/time.h>
#include <kernel/arch.h>
#include <module/ps2.h>
#include <input.h>

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

ring_buffer keyboard_queue;

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

static int init_ps2kb(int argc,char **argv){
	
	return 0;
}

static int fini_ps2kb(){
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_ps2kb,
	.fini = fini_ps2kb,
	.name = "ps2 keyboard",
	.description = "driver for ps2 keyboard",
	.author = "tayoky",
	.liscence = "GPL 3"
};