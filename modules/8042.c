#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/ringbuf.h>
#include <kernel/port.h>
#include <kernel/irq.h>
#include <errno.h>

//driver for the 8042 ps2 controller
//work only on x86_64 computer

#define PS2_DATA    0x60
#define PS2_COMMAND 0x64
#define PS2_STATUS  0x64

//controller command

#define PS2_DISABLE_PORT1   0xAD
#define PS2_ENABLE_PORT1    0xAE
#define PS2_DISABLE_PORT2   0xA7
#define PS2_ENABLE_PORT2    0xA8
#define PS2_TEST_CONTROLLER 0xAA
#define PS2_TEST_PORT1      0xAB
#define PS2_TEST_PORT2      0xA9
#define PS2_READ_CONF       0x20
#define PS2_WRITE_CONF      0x60
#define PS2_SEND_PORT2      0xD4

#define PS2_CONTROLLER_TEST_SUCCESSED 0x55
#define PS2_CONTROLLER_TEST_FAILED    0xFC

//devices commands

ring_buffer keyboard_queue;

static void wait_output(){
	while(!(in_byte(PS2_STATUS) & 0x01));
}

static void wait_input(){
	while(in_byte(PS2_STATUS) & 0x02);
}

static void ps2_send_command(uint8_t command){
	wait_input();
	out_byte(PS2_COMMAND,command);
}

uint8_t ps2_read(void){
	wait_output();
	return in_byte(PS2_DATA);
}

static void ps2_write(uint8_t data){
	wait_input();
	out_byte(PS2_DATA,data);
}

void ps2_send(uint8_t port,uint8_t data){
	if(port == 2){
		ps2_send_command(PS2_SEND_PORT2);
	}
	ps2_write(data);
}

void ps2_register_handler(void *handler,uint8_t port){
	switch (port){
	case 1:
		irq_generic_map(handler,1);
		break;
	case 2:
		irq_generic_map(handler,2);
		break;
	default:
		break;
	}
}

static int init_ps2(int argc,char **argv){
	//disable everything
	ps2_send_command(PS2_DISABLE_PORT1);
	ps2_send_command(PS2_DISABLE_PORT2);

	//flush the output buffer
	in_byte(PS2_DATA);

	//test the controller
	ps2_send_command(PS2_TEST_CONTROLLER);
	if(ps2_read() != PS2_CONTROLLER_TEST_SUCCESSED){
		kdebugf("ps2 : the 8042 ps2 controller didn't pass self test (broken controller ?)\n");
		return -ENODEV;
	}

	//try to check for port 2
	char have_port1 = 1;
	char have_port2 = 0;
	ps2_send_command(PS2_ENABLE_PORT2);
	ps2_send_command(PS2_READ_CONF);
	uint8_t conf = ps2_read();
	if(!(conf & (1 << 5))){
		//there is a second port
		have_port2 = 1;
	}
	
	//test ports
	ps2_send_command(PS2_TEST_PORT1);
	if(ps2_read() != 0){
		have_port1 = 0;
		kdebugf("ps2 : the first ps2 port didn't pass test (broken controller ?)\n");
	}
	if(have_port2){
		ps2_send_command(PS2_TEST_PORT2);
		if(ps2_read() != 0){
			have_port2 = 0;
			kdebugf("ps2 : the second ps2 port didn't pass test (broken controller ?)\n");
		}
	}

	//if no port availible just give up
	if(!(have_port1 || have_port2)){
		kdebugf("ps2 : both ps2 ports are not availible\n");
		return -ENODEV;
	}

	//setup the configuration byte
	ps2_send_command(PS2_READ_CONF);
	conf = ps2_read();
	//start by setting all field to 0
	conf &= 0b00110100;
	//then activate irq
	if(have_port1){
		conf |= 1;
	}
	if(have_port2){
		conf |= 2;
	}
	//now write conf
	ps2_send_command(PS2_WRITE_CONF);
	ps2_write(conf);

	//activate devices
	if(have_port1){
		ps2_send_command(PS2_ENABLE_PORT1);
	}
	if(have_port2){
		ps2_send_command(PS2_ENABLE_PORT2);
	}

	kdebugf("ps2 : 8042 ps2 controller initialized\n");

	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_ps2,
	.name = "8042 ps2",
	.author = "tayoky"
};