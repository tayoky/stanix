#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/ringbuf.h>
#include <kernel/port.h>
#include <kernel/irq.h>
#include <module/ps2.h>
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

char ps2_have_port1 = 1;
char ps2_have_port2 = 0;

int ps2_port_id[3][2] = {
	{0,0},
	{0,0},
	{0,0}
};

static int wait_output(){
	for (size_t i = 0; i < 10000; i++){
		if(in_byte(PS2_STATUS) & 0x01){
			return 0;
		}
	}

	return -1;
}

static int wait_input(){
	for (size_t i = 0; i < 10000; i++){
		if(!(in_byte(PS2_STATUS) & 0x02)){
			return 0;
		}
	}

	return -1;
}

static void ps2_send_command(uint8_t command){
	wait_input();
	out_byte(PS2_COMMAND,command);
}

int ps2_read(void){
	if(wait_output()){
		return -1;
	}
	return in_byte(PS2_DATA);
}

static int ps2_write(uint8_t data){
	if(wait_input()){
		return -1;
	}
	out_byte(PS2_DATA,data);
	return 0;
}

int ps2_send(uint8_t port,uint8_t data){
	if(port == 2){
		ps2_send_command(PS2_SEND_PORT2);
	}
	return ps2_write(data);
}

void ps2_register_handler(void *handler,uint8_t port){
	switch (port){
	case 1:
		kdebugf("registers handler for first ps2 irq\n");
		irq_generic_map(handler,1);
		break;
	case 2:
		irq_generic_map(handler,12);
		break;
	default:
		break;
	}
}

static void print_device_name(int port){
	if(port == 1){
		kdebugf("ps2 : first port device : ");
	} else {
		kdebugf("ps2 : second port device : ");
	}

	if(ps2_send(port,PS2_IDENTIFY)){
		kprintf("unknow device\n");
	}

	if(ps2_read() != PS2_ACK){
		kprintf("unknow device\n");
	}

	int c0 = ps2_read();
	int c1 = ps2_read();

	ps2_port_id[port][0] = c0;
	ps2_port_id[port][1] = c1;

	switch(c0){
	case -1: //-1 mean no byte
		kprintf("Ancient AT keyboard\n");
		break;
	case 0x00:
		kprintf("Standard PS/2 mouse\n");
		break;
	case 0x03:
		kprintf("Mouse with scroll wheel\n");
		break;
	case 0x04:
		kprintf("5-button mouse\n");
		break;
	case 0xAB:
		switch(c1){
		case 0x83:
		case 0xC1:
			kprintf("MF2 keybaord\n");
			break;
		case 0x84:
			kprintf("Short Keyboard\n");
			break;
		case 0x85:
			kprintf("122-Key Host Connect(ed) Keyboard\n");
			break;
		case 0x86:
			kprintf("122-key keyboards\n");
			break;
		default:
			kprintf("unknow keyboard %x:%x\n",c0,c1);
			break;
		}
		break;
	case 0xAC:
		switch(c1){
		case 0xA1:
			kprintf("NCD Sun layout keyboard\n");
			break;
		default:
			kprintf("unknow device\n");
			break;
		}
		break;
	default:
		kprintf("unknow device : %x:%x\n",c0,c1);
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
	ps2_send_command(PS2_ENABLE_PORT2);
	ps2_send_command(PS2_READ_CONF);
	uint8_t conf = (uint8_t)ps2_read();
	if(!(conf & (1 << 5))){
		//there is a second port
		ps2_have_port2 = 1;
	}
	ps2_send_command(PS2_DISABLE_PORT2);
	
	//test ports
	ps2_send_command(PS2_TEST_PORT1);
	if(ps2_read() != 0){
		ps2_have_port1 = 0;
		kdebugf("ps2 : the first ps2 port didn't pass test (broken controller ?)\n");
	}
	if(ps2_have_port2){
		ps2_send_command(PS2_TEST_PORT2);
		if(ps2_read() != 0){
			ps2_have_port2 = 0;
			kdebugf("ps2 : the second ps2 port didn't pass test (broken controller ?)\n");
		}
	}

	//if no port availible just give up
	if(!(ps2_have_port1 || ps2_have_port2)){
		kdebugf("ps2 : both ps2 ports are not availible\n");
		return -ENODEV;
	}

	//setup the configuration byte
	ps2_send_command(PS2_READ_CONF);
	conf = (uint8_t)ps2_read();
	//start by setting all field to 0
	conf &= 0b00000100;	
	//then activate irq
	if(ps2_have_port1){
		conf |= 1;
	}
	if(ps2_have_port2){
		conf |= 2;
	}
	//now write conf
	ps2_send_command(PS2_WRITE_CONF);
	ps2_write(conf);

	//activate devices
	if(ps2_have_port1){
		ps2_send_command(PS2_ENABLE_PORT1);
	}
	if(ps2_have_port2){
		ps2_send_command(PS2_ENABLE_PORT2);
	}

	//now scan the device on each port
	if(ps2_have_port1){
		if(ps2_send(1,PS2_DISABLE_SCANING)){
			//no device on the port
			ps2_have_port1 = 0;
			kdebugf("ps2 : no device on first port\n");
		} else if(ps2_read() != PS2_ACK){
			ps2_have_port1 = 0;
			kdebugf("ps2 : no device on first port\n");
		} else {
			//identify the device
			print_device_name(1);
			ps2_send(1,PS2_ENABLE_SCANING);
			ps2_read();
		}
	}
	
	//now scan the device on each port
	if(ps2_have_port2){
		if(ps2_send(2,PS2_DISABLE_SCANING)){
			//no device on the port
			ps2_have_port2 = 0;
			kdebugf("ps2 : no device on second port\n");
		} else if(ps2_read() != PS2_ACK){
			ps2_have_port2 = 0;
			kdebugf("ps2 : no device on second port\n");
		} else {
			print_device_name(2);
			ps2_send(2,PS2_ENABLE_SCANING);
			ps2_read();
		}
	}

	kdebugf("ps2 : 8042 ps2 controller initialized\n");

	//export time
	EXPORT(ps2_have_port1);
	EXPORT(ps2_have_port2);
	EXPORT(ps2_port_id);
	EXPORT(ps2_register_handler);
	EXPORT(ps2_read);
	EXPORT(ps2_send);

	return 0;
}

static int fini_ps2(){
	UNEXPORT(ps2_have_port1);
	UNEXPORT(ps2_have_port2);
	UNEXPORT(ps2_port_id);
	UNEXPORT(ps2_register_handler);
	UNEXPORT(ps2_read);
	UNEXPORT(ps2_send);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_ps2,
	.fini = fini_ps2,
	.name = "8042 ps2",
	.author = "tayoky"
};