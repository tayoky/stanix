#ifndef KERNEL_APIC_H
#define KERNEL_APIC_H

#include <stdint.h>

typedef struct ioapic {
	uintptr_t address;
	uint32_t gsi_base;
	uint8_t id;
} ioapic_t;

#define LOCAL_APIC_REG_ID            0x020
#define LOCAL_APIC_REG_VERSION       0x030
#define LOCAL_APIC_REG_TPR           0x080 // task priority
#define LOCAL_APIC_REG_APR           0x090 // arbitration priority
#define LOCAL_APIC_REG_PPR           0x0a0 // processor priority
#define LOCAL_APIC_REG_EOI           0x0b0 // end of interrupt
#define LOCAL_APIC_REG_RRD           0x0c0 // remote read
#define LOCAL_APIC_REG_LOGICAL_DEST  0x0d0 // logical destionation
#define LOCAL_APIC_REG_DEST_FMT      0x0e0 // destination format
#define LOCAL_APIC_REG_SPURIOUS      0x0f0 // spurious interrupt vector
#define LOCAL_APIC_REG_ISR           0x100 // in service
#define LOCAL_APIC_REG_TMR           0x180 // trigger mode
#define LOCAL_APIC_REG_IRR           0x200 // interrupt request
#define LOCAL_APIC_REG_ERROR         0x280 // error status
#define LOCAL_APIC_REG_CMCI          0x2f0 // LVT corrected machine check interrupt
#define LOCAL_APIC_REG_ICR           0x300 // interrupt command register
#define LOCAL_APIC_REG_LVT_TIMER     0x320 // LVT timer
#define LOCAL_APIC_REG_LVT_THERMAL   0x330 // LVT thermal sensor
#define LOCAL_APIC_REG_LVT_PERF      0x340 // LVT perfomance monitoring counters
#define LOCAL_APIC_REG_LVT_LINT0     0x350 // LVT LINT0
#define LOCAL_APIC_REG_LVT_LINT1     0x360 // LVT LINT1
#define LOCAL_APIC_REG_LVT_ERROR     0x370 // LVT error status
#define LOCAL_APIC_REG_INITIAL_COUNT 0x380 // timer inital count
#define LOCAL_APIC_REG_CURRENT_COUNT 0x390 // timer current count
#define LOCAL_APIC_REG_DIVIDE_CONF   0x3e0 // timer divide configuration

int have_apic(void);
void init_apic(void);

#endif
