c_files = $(wildcard *.c)
asm_files = $(wildcard *.S)

objs = $(patsubst %.c, %.o, $(c_files)) $(patsubst %.S, %.o, $(asm_files))

asm_temps = $(patsubst %.c, %.s, $(c_files))
preproc_temps = $(patsubst %.c, %.i, $(c_files))

CROSS_COMPILE = arm-none-eabi

CFLAGS += -Wall -Werror -save-temps -g
LDFLAGS += -Wl,-T,kernel.ldscript

CC = $(CROSS_COMPILE)-gcc
AS = $(CROSS_COMPILE)-as
LD = $(CROSS_COMPILE)-gcc

all: kernel

kernel: kernel.ldscript

kernel: $(objs)
	$(LD) $(LDFLAGS) -nostdlib -o $@ $(objs)

clean:
	rm -f $(objs) $(asm_temps) $(preproc_temps) kernel

debug: kernel
	$(CROSS_COMPILE)-gdb kernel --eval="target remote :1234"

run: kernel
	qemu-system-arm -s -S -kernel kernel

.PHONY: clean debug run
