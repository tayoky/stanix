#include "serial.h"
#include "asm.h"
#include "limine.h"
#include <stdint.h>

//define some const for later

#define PL011_BASE 0x09000000 //TODO get this from device tree

#define UART_EN  1UL << 0 //UART enable
#define UART_TXE 1UL << 8 //transmit enable
#define UART_RXE 1UL << 9 //receive enable

//word len for control line
#define UART_WLEN_5BITS 0UL << 5
#define UART_WLEN_6BITS 1UL << 5
#define UART_WLEN_7BITS 2UL << 5
#define UART_WLEN_8BITS 3UL << 5

//control line bit
#define UART_BRK 1UL << 0 //send break
#define UART_PEN 1UL << 1 //parity enable
#define UART_EPS 1UL << 2 //even parity select
#define UART_FEN 1UL << 4 //enable FIFOs
#define UART_SPS 1UL << 7 //stick parity select

//flags registers
#define UART_TXFE 1UL << 7 //transmit FIFO empty
#define UART_RXFF 1UL << 6 //receive FIFO full
#define UART_TXFF 1UL << 5 //transmit FIFO full
#define UART_RXFE 1UL << 4 //receive FIFO empty
#define UART_BUSY 1UL << 3 //UART busy
#define UART_DCD  1UL << 2 //data carrier detect
#define UART_DSR  1UL << 1 //data set ready
#define UART_CTS  1UL << 0 //clear to send

#define UART_RXIFLSEL 2UL << 3UL
#define UART_TXIFLSEL 2UL << 0UL
#define UART_RXIFLSEL_POS  6
#define UART_TXIFLSEL_POS  4 
#define UART_RXIFLSEL_MASK 0x7 << UART_RXIFLSEL_POS
#define UART_TXIFLSEL_MASK 0x7 << UART_TXIFLSEL_POS

//this a modified version of
//https://github.com/ProfessorLongBeard/HelixOS/blob/v0.0.1-dev/kernel/include/devices/pl011.h
 
typedef struct uart{
	uint32_t    uart_dr;                // 0x000 - UART data register
	uint32_t    uart_sr_cr;             // 0x004 - UART status/error clear register
	uint8_t     __reserved1[16];        // 0x008 - 0x018 reserved
	uint32_t    uart_fr;                // 0x018 - UART flags registers
	uint32_t    __reserved2;            // 0x01C - reserved
	uint32_t    uart_lpr;               // 0x020 - UART low-power counter register
	uint32_t    uart_ibrd;              // 0x024 - UART integer baudrate register
	uint32_t    uart_fbrd;              // 0x028 - UART fractal baudrate register
	uint32_t    uart_lcrh;              // 0x02C - UART line control register
	uint32_t    uart_cr;                // 0x030 - UART control register
	uint32_t    uart_ifls;              // 0x034 - UART interrupt FIFO level select register
	uint32_t    uart_imsc;              // 0x038 - UART interrupt mask set/clear register
	uint32_t    uart_ris;               // 0x03C - UART raw interrupt status register
	uint32_t    uart_mis;               // 0x040 - UART masked interrupt status register
	uint32_t    uart_icr;               // 0x044 - UART interrupt clear register
	uint32_t    uart_macr;              // 0x048 - UART DMA control register
} __attribute__((packed)) uart;

uart *pl011 = PL011_BASE;

void set_baudrate(uart *port,uint64_t baudrate){
	uint64_t divisor = 24000000 /baudrate;
	port->uart_ibrd = divisor & 0xFFFF;
	port->uart_fbrd = (divisor >> 16) & 0xFFFF;
}

extern volatile struct limine_hhdm_request *hhdm_request;

int init_serial(){
	pl011 += hhdm_request->response->offset;
	//disable port during config
	pl011->uart_cr &= ~(UART_EN);

	//8 bits word
	pl011->uart_lcrh |= UART_WLEN_8BITS;

	//enable fifo buffer to keep char until we read them
	pl011->uart_lcrh |= UART_FEN;

	//enable parity checks
	pl011->uart_lcrh |= UART_EPS | UART_PEN;

	set_baudrate(pl011,16 * 115200);

	pl011->uart_fr &= ~(UART_RXIFLSEL_MASK | UART_TXIFLSEL_MASK);
	pl011->uart_fr |= (UART_RXIFLSEL << UART_RXIFLSEL_POS) | (UART_TXIFLSEL << UART_TXIFLSEL_POS);

	pl011->uart_imsc = 0;

	//enable port for receving and sending data
	pl011->uart_cr |= UART_EN | UART_RXE | UART_TXE;

	write_serial("hello world\n");
	halt();
}

void write_serial_char(char c){
	//wait until the transmit fifo is not full
	//so we have place to write to it
	while(pl011->uart_fr & UART_TXFF);
	pl011->uart_dr = (uint32_t)c;
}
void write_serial(const char *str){
	while(*str){
		write_serial_char(*str);
		str++;
	}
}