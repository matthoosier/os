all: kernel

c_files = $(wildcard *.c)
c_dep_files = $(patsubst %.c, .%.c.depends, $(c_files))
asm_files = $(wildcard *.S)

objs = $(patsubst %.c, %.o, $(c_files)) $(patsubst %.S, %.o, $(asm_files))

-include $(c_dep_files)

asm_temps = $(patsubst %.c, %.s, $(c_files))
preproc_temps = $(patsubst %.c, %.i, $(c_files))

CROSS_COMPILE = arm-none-eabi

ASFLAGS += -Wall -Werror -g
CFLAGS += -Wall -Werror -save-temps -g -march=armv6
LDFLAGS += -Wl,-T,kernel.ldscript

CC = $(CROSS_COMPILE)-gcc
AS = $(CROSS_COMPILE)-as
LD = $(CROSS_COMPILE)-gcc

kernel: kernel.ldscript

%.o: %.c
	@# Update dependencies
	@$(CC) $(CFLAGS) -M -o .$<.depends $<
	@# Build object
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

kernel: $(objs)
	$(LD) $(LDFLAGS) -nostdlib -o $@ $(objs)

clean:
	rm -f $(objs) $(c_dep_files) $(asm_temps) $(preproc_temps) kernel

debug: kernel
	$(CROSS_COMPILE)-gdb kernel --eval="target remote :1234"

run: kernel
	qemu-system-arm -s -S -kernel kernel -cpu arm1136

.PHONY: clean debug run
