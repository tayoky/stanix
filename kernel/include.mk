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
    -mcmodel=kernel
CFLAGS += -I ./
CFLAGS += -I ./include/
CFLAGS += -I ../limine/
CFLAGS += -I ./cpu/include/
CFLAGS += -I ./klib/include/
CFLAGS += -I ./mem/include/
CFLAGS += -I ./drivers/include/