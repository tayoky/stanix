#we force flags cause -O2 cause a crash
CFLAGS = -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -fsanitize=undefined
CFLAGS += -I ./
CFLAGS += -I ./include/
CFLAGS += -I ../limine/
CFLAGS += -I ./arch/${ARCH}/cpu/include/
CFLAGS += -I ./arch/${ARCH}/include/
CFLAGS += -I ./arch/${ARCH}/irq/include/
CFLAGS += -I ./klib/include/
CFLAGS += -I ./mem/include/
CFLAGS += -I ./drivers/include/
CFLAGS += -I ./fs/include/
CFLAGS += -I ./task/include/