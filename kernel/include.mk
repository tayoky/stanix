CFLAGS = -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -m64 \
    -march=x86-64 \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -fsanitize=undefined
CFLAGS += -I ./
CFLAGS += -I ./include/
CFLAGS += -I ../limine/
CFLAGS += -I ./arch/${ARCH}/cpu/include/
CFLAGS += -I ./arch/${ARCH}/include/
CFLAGS += -I ./klib/include/
CFLAGS += -I ./mem/include/
CFLAGS += -I ./drivers/include/
CFLAGS += -I ./fs/include/
CFLAGS += -I ./drivers/irq/include/
CFLAGS += -I ./task/include/