#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/ringbuf.h>
#include <kernel/port.h>
#include <kernel/bus.h>
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

int have_ports[2] = {1, 0};
static ps2_addr_t ports[2];
static bus_ops_t ps2_ops;
static device_driver_t ps2_driver = {
	.name = "8042",
};
static bus_t ps2_bus = {
	.device = {
		.name = "ps2",
		.driver = &ps2_driver,
		.type = DEVICE_BUS,
	},
	.ops = &ps2_ops,
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
	int ret = ps2_write(data);
	if(ret < 0){
		return ret;
	}
	return ps2_read();
}

static ssize_t ps2_bus_read(bus_addr_t *addr, void *buf, off_t offset, size_t count) {
	(void)addr;
	(void)offset;
	unsigned char *c = buf;
	ssize_t total = 0;
	while (count > 0) {
		int byte = ps2_read();
		if (byte < 0) break;
		*c = (unsigned char)byte;
		count--;
		c++;
	}
	return total;
}

static int ps2_register_handler(bus_addr_t *addr, interrupt_handler_t handler, void *data) {
	ps2_addr_t *ps2_addr = (ps2_addr_t*)addr;
	switch (port) {
	case 1:
		irq_generic_map(handler,1, data);
		return 0;
	case 2:
		irq_generic_map(handler,12, data);
		return 0;
	default:
		return -EINVAL;
	}
}

static bus_ops_t ps2_ops = {
	.read = ps2_bus_read,
	.register_handler = ps2_register_handler,
};

static void print_device_name(int port){
	if(port == 1){
		kdebugf("ps2 : first port device : ");
	} else {
		kdebugf("ps2 : second port device : ");
	}

	if(ps2_send(port,PS2_IDENTIFY) != PS2_ACK){
		kprintf("unknow device\n");
	}

	int c0 = ps2_read();
	int c1 = ps2_read();

	ports[port-1].device_id[0] = c0;
	ports[port-1].device_id[1] = c1;

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

static void setup_addr(int port){
	list_append(ps2_bus.addresses, &ports[port-1]);
	char name[32];
	sprintf(name, "port%d", port);
	ports[port-1].addr.type = BUS_PS2;
	ports[port-1].addr.name = strdup(name);
	ports[port-1].addr.bus  = &ps2_bus;
	ports[port-1].port = port;
}

static int init_ps2(int argc,char **argv){
	//disable everything
	ps2_send_command(PS2_DISABLE_PORT1);
	ps2_send_command(PS2_DISABLE_PORT2);

	//flush the output buffer
	while (in_byte(PS2_STATUS) & 1) in_byte(PS2_DATA);

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
		have_ports[1] = 1;
	}
	ps2_send_command(PS2_DISABLE_PORT2);
	
	//test ports
	ps2_send_command(PS2_TEST_PORT1);
	if(ps2_read() != 0){
		have_ports[0] = 0;
		kdebugf("ps2 : the first ps2 port didn't pass test (broken controller ?)\n");
	}
	if(have_ports[1]){
		ps2_send_command(PS2_TEST_PORT2);
		if(ps2_read() != 0){
			have_ports[1] = 0;
			kdebugf("ps2 : the second ps2 port didn't pass test (broken controller ?)\n");
		}
	}

	//if no port availible just give up
	if(!(have_ports[0] || have_ports[1])){
		kdebugf("ps2 : both ps2 ports are not availible\n");
		return -ENODEV;
	}

	//setup the configuration byte
	ps2_send_command(PS2_READ_CONF);
	conf = (uint8_t)ps2_read();
	//start by setting all field to 0
	conf &= 0b00000100;	
	//then activate irq
	if(have_ports[0]){
		conf |= 1;
	}
	if(have_ports[1]){
		conf |= 2;
	}
	//now write conf
	ps2_send_command(PS2_WRITE_CONF);
	ps2_write(conf);

	//activate devices
	if(have_ports[0]){
		ps2_send_command(PS2_ENABLE_PORT1);
	}
	if(have_ports[1]){
		ps2_send_command(PS2_ENABLE_PORT2);
	}

	//setup driver and bus
	register_device_driver(&ps2_driver);
	ps2_bus.addresses = new_list();

	//now scan the device on each port
	for (int i=1; i<3; i++) {
		if (!have_ports[i-1]) continue;
		if(ps2_send(i,PS2_DISABLE_SCANING) != PS2_ACK){
			//no device on the port
			have_ports[i-1] = 0;
			kdebugf("ps2 : no device on port %d\n", i);
		} else {
			//identify the device
			print_device_name(i);
			setup_addr(i);
		}
	}	

	//we now want to enbale translation
	if(!have_opt(argc,argv,"--no-translation")){
		conf |= 0x40;
		ps2_send_command(PS2_WRITE_CONF);
		ps2_write(conf);	
	}

	register_device((device_t*)&ps2_bus);

	kdebugf("ps2 : 8042 ps2 controller initialized\n");

	//NOTE : at this point scanning is disable
	//the driver specfic to the device as to enable scaning itself

	//export time
	EXPORT(ps2_read);
	EXPORT(ps2_send);

	return 0;
}

static int fini_ps2(){
	destroy_device((device_t*)&ps2_bus);
	unregister_device_driver(&ps2_driver);
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
