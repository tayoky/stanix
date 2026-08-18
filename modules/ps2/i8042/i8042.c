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

#define I8042_DATA    0x60
#define I8042_COMMAND 0x64
#define I8042_STATUS  0x64

// i8042 controller command

#define I8042_DISABLE_PORT1   0xAD
#define I8042_ENABLE_PORT1    0xAE
#define I8042_DISABLE_PORT2   0xA7
#define I8042_ENABLE_PORT2    0xA8
#define I8042_TEST_CONTROLLER 0xAA
#define I8042_TEST_PORT1      0xAB
#define I8042_TEST_PORT2      0xA9
#define I8042_READ_CCB        0x20
#define I8042_WRITE_CCB       0x60
#define I8042_SEND_PORT2      0xD4

#define I8042_CONTROLLER_TEST_SUCCEED 0x55
#define I8042_CONTROLLER_TEST_FAILED  0xFC

int have_ports[2] = { 1, 0 };
static ps2_dev_t ports[2];
static int no_translation;

// i8042 controller I/O

static int i8042_wait_output(void) {
	for (size_t i = 0; i < 100000; i++) {
		if (in_byte(I8042_STATUS) & 0x01) {
			return 0;
		}
		io_wait();
	}

	return -ETIMEDOUT;
}

static int i8042_wait_input(void) {
	for (size_t i = 0; i < 100000; i++) {
		if (!(in_byte(I8042_STATUS) & 0x02)) {
			return 0;
		}
		io_wait();
	}

	return -ETIMEDOUT;
}

static int i8042_send_command(uint8_t command) {
	int ret = i8042_wait_input();
	if (ret < 0) return ret;
	out_byte(I8042_COMMAND, command);
	return 0;
}

static int i8042_read(void) {
	int ret = i8042_wait_output();
	if (ret < 0) return ret;
	return in_byte(I8042_DATA);
}

static int i8042_write(uint8_t data) {
	int ret = i8042_wait_input();
	if (ret < 0) return ret;
	out_byte(I8042_DATA, data);
	return 0;
}

int i8042_read_ccb(uint8_t *ccb) {
	int ret = i8042_send_command(I8042_READ_CCB);
	if (ret < 0) return ret;
	ret = i8042_read();
	if (ret < 0) return ret;
	*ccb = (uint8_t)ret;
	return 0;
}

int i8042_write_ccb(uint8_t ccb) {
	int ret = i8042_send_command(I8042_WRITE_CCB);
	if (ret < 0) return ret;
	return i8042_write(ccb);
}

void i8042_flush(void) {
	while (in_byte(I8042_STATUS) & 1) in_byte(I8042_DATA);
}

// i8042 devices I/O

static int i8042_device_send(devnode_t *controller, ps2_dev_t *device, uint8_t data) {
	(void)controller;
	if (device == &ports[1]) {
		int ret = i8042_send_command(I8042_SEND_PORT2);
		if (ret < 0) return ret;
	}
	return i8042_write(data);
}

static int i8042_device_read(devnode_t *controller, ps2_dev_t *device) {
	(void)controller;
	(void)device;
	// TODO : do some checking ??
	return i8042_read();
}

static void setup_ps2_dev(devnode_t *bus, int port) {
	ports[port - 1].devnode.type = BUS_PS2;

	// allocate irqs
	hwirq_t hwirq = port == 1 ? 1 : 12;
	irq_t *irq = irq_get_from_hwirq(main_irq_chip, hwirq);
	kassert(irq);
	bus_add_fixed_resource_desc(&ports[port - 1].devnode, irq->hwirq, 1, RESOURCE_IRQ, PS2_RID_IRQ);
	ps2_add_device(bus, &ports[port - 1]);
}

static int i8042_probe(devnode_t *devnode) {

	// disable everything
	i8042_send_command(I8042_DISABLE_PORT1);
	i8042_send_command(I8042_DISABLE_PORT2);

	i8042_flush();

	// test the controller
	i8042_send_command(I8042_TEST_CONTROLLER);
	if (i8042_read() != I8042_CONTROLLER_TEST_SUCCEED) {
		kdebugf("ps2 : the 8042 ps2 controller didn't pass self test (broken controller ?)\n");
		return -ENODEV;
	}

	// try to check for port 2
	i8042_send_command(I8042_ENABLE_PORT2);
	uint8_t conf = 0;
	i8042_read_ccb(&conf);
	if (!(conf & (1 << 5))) {
		// there is a second port
		have_ports[1] = 1;
	}
	i8042_send_command(I8042_DISABLE_PORT2);

	// test ports
	i8042_send_command(I8042_TEST_PORT1);
	if (i8042_read() != 0) {
		have_ports[0] = 0;
		kdebugf("ps2 : the first ps2 port didn't pass test (broken controller ?)\n");
	}
	if (have_ports[1]) {
		i8042_send_command(I8042_TEST_PORT2);
		if (i8042_read() != 0) {
			have_ports[1] = 0;
			kdebugf("ps2 : the second ps2 port didn't pass test (broken controller or non present aux port ?)\n");
		}
	}

	// if no port available just give up
	if (!(have_ports[0] || have_ports[1])) {
		kdebugf("ps2 : both ps2 ports are not available\n");
		return -ENODEV;
	}

	// setup the configuration byte
	i8042_read_ccb(&conf);

	// start by setting fields to 0
	conf &= ~1;
	conf &= ~2;
	conf &= ~0x40;

	// then activate irq
	if (have_ports[0]) {
		conf |= 1;
	}
	if (have_ports[1]) {
		conf |= 2;
	}

	// activate devices
	if (have_ports[0]) {
		i8042_send_command(I8042_ENABLE_PORT1);
	}
	if (have_ports[1]) {
		i8042_send_command(I8042_ENABLE_PORT2);
	}

	// now scan the device on each port
	for (int i=1; i < 3; i++) {
		if (!have_ports[i - 1]) continue;
		setup_ps2_dev(devnode, i);
	}

	// we now want to enable translation
	if (!no_translation) {
		conf |= 0x40;
	}

	// now write conf
	i8042_write_ccb(conf);

	kdebugf("ps2 : 8042 ps2 controller initialized\n");

	// NOTE : at this point scanning is disable
	// the driver specific to the device as to enable scanning itself

	return 0;
}

static ps2_driver_t i8042_driver = {
	.driver = {
		.name = "i8042",
		.device_name = "ps2",
		.probe = i8042_probe,
		.buses = BUSES("root"),
	},
	.send = i8042_device_send,
	.read = i8042_device_read,
};

static int init_i8042(int argc, char **argv) {
	no_translation = have_opt(argc, argv, "--no-translation");

	driver_register(&i8042_driver.driver);

	// hardly attach a i8042 bus to root
	bus_attach_child(bus_get_root(), NULL, "ps2", UNIT_NOUNIT);

	return 0;
}

static int fini_i8042() {
	driver_unregister(&i8042_driver.driver);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_i8042,
	.fini = fini_i8042,
	.name = "i8042 controller driver",
	.author = "tayoky"
};
