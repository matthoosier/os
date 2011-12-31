all: image progs tools

depfile = $(dir $(1))$(patsubst %.c,.%.c.depends,$(notdir $(1)))

progs = \
	echo \
	syscall-client \
	$(NULL)

progs: $(progs)

NULL =

tools = \
	elfstats \
	fs-builder \
	$(NULL)

tools: $(tools)

elfstats_objs = \
	elfstats.host.o \
	$(NULL)

fs-builder_objs = \
	fs-builder.host.o \
	$(NULL)

kernel_asm_files = \
	kernel/atomic.S \
	kernel/early-entry.S \
	kernel/high-entry.S \
	kernel/vector.S \
	$(NULL)

built_kernel_c_files = \
	kernel/ramfs_image.c \
	$(NULL)

kernel_c_files = \
	kernel/assert.c \
	kernel/early-mmu.c \
	kernel/init.c \
	kernel/interrupts.c \
	kernel/syscall.c \
	kernel/kmalloc.c \
	kernel/large-object-cache.c \
	kernel/message.c \
	kernel/mmu.c \
	kernel/object-cache.c \
	kernel/once.c \
	kernel/process.c \
	kernel/ramfs.c \
	kernel/small-object-cache.c \
	kernel/stdlib.c \
	kernel/thread.c \
	kernel/timer.c \
	kernel/tree-map.c \
	kernel/vm.c \
	$(built_kernel_c_files) \
	$(NULL)

kernel_c_dep_files = $(foreach f,$(kernel_c_files),$(call depfile,$(f)))

-include $(kernel_c_dep_files)

kernel_objs = \
	$(patsubst %.c, %.ko, $(kernel_c_files)) \
	$(patsubst %.S, %.ko, $(kernel_asm_files)) \
	$(NULL)

kernel_asm_temps = $(patsubst %.c, %.s, $(kernel_c_files))
kernel_preproc_temps = $(patsubst %.c, %.i, $(kernel_c_files))

CROSS_COMPILE = arm-none-eabi

ASFLAGS += -g
CFLAGS += -g -I include
CXXFLAGS += -g

KERNEL_ASFLAGS += $(ASFLAGS) -Wall -Werror
KERNEL_CFLAGS += $(CFLAGS) -Wall -Werror -save-temps -march=armv6 -D__KERNEL__
KERNEL_LDFLAGS += $(LDFLAGS) -Wl,-T,kernel.ldscript

KERNEL_CC = $(CROSS_COMPILE)-gcc
KERNEL_AS = $(CROSS_COMPILE)-as
KERNEL_LD = $(CROSS_COMPILE)-gcc

USER_ASFLAGS += $(ASFLAGS) -Wall -Werror
USER_CFLAGS += $(CFLAGS) -Wall -Werror -save-temps -march=armv6

USER_CC = $(CROSS_COMPILE)-gcc
USER_AS = $(CROSS_COMPILE)-as
USER_LD = $(CROSS_COMPILE)-gcc

image: kernel.ldscript

echo_c_files = \
	libc/crt.c \
	libc/user_io.c \
	libc/user_message.c \
	libc/syscall.c \
	echo.c \
	$(NULL)

syscall_client_c_files = \
	libc/crt.c \
	libc/user_io.c \
	libc/user_message.c \
	libc/syscall.c \
	syscall-client.c \
	$(NULL)

echo_c_dep_files = $(foreach f,$(echo_c_files),$(call depfile,$(f)))

syscall_client_c_dep_files = $(foreach f,$(syscall_client_c_files),$(call depfile,$(f)))

-include $(echo_c_dep_files)
-include $(syscall_client_c_dep_files)

echo_objs = \
	$(patsubst %.c, %.o, $(echo_c_files)) \
	$(NULL)

syscall_client_objs = \
	$(patsubst %.c, %.o, $(syscall_client_c_files)) \
	$(NULL)

echo_asm_temps = $(patsubst %.c, %.s, $(echo_c_files))

syscall_client_asm_temps = $(patsubst %.c, %.s, $(syscall_client_c_files))

echo_preproc_temps = $(patsubst %.c, %.i, $(echo_c_files))

syscall_client_preproc_temps = $(patsubst %.c, %.i, $(syscall_client_c_files))

echo: $(echo_objs)
	$(USER_LD) -nostdlib -o $@ $+ -Wl,-Ttext-segment,0x10000

syscall-client: $(syscall_client_objs)
	$(USER_LD) -nostdlib -o $@ $+ -Wl,-Ttext-segment,0x20000

%.o: %.c
	@# Update dependencies
	@$(USER_CC) $(USER_CFLAGS) -M -MT $@ -o $(call depfile,$<) $<
	@# Build object
	$(USER_CC) $(USER_CFLAGS) -c -o $@ $<

%.ko: %.c
	@# Update dependencies
	@$(KERNEL_CC) $(KERNEL_CFLAGS) -M -MT $@ -o $(call depfile,$<) $<
	@# Build object
	$(KERNEL_CC) $(KERNEL_CFLAGS) -c -o $@ $<

%.ko: %.S
	$(KERNEL_CC) $(KERNEL_ASFLAGS) -c -o $@ $<

kernel/ramfs_image.c: $(progs) fs-builder
	./fs-builder -o $@ -n RamFsImage $(progs)

image: $(kernel_objs)
	$(KERNEL_LD) $(KERNEL_LDFLAGS) -nostdlib -o $@ $(kernel_objs)

%.host.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.host.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

elfstats: $(elfstats_objs)
	$(CC) $(LDFLAGS) -o $@ $<

fs-builder: $(fs-builder_objs)
	$(CXX) $(LDFLAGS) -o $@ $<

clean:
	rm -f \
		$(kernel_objs) \
		$(kernel_c_dep_files) \
		$(kernel_asm_temps) \
		$(kernel_preproc_temps) \
		$(built_kernel_c_files) \
		image \
		$(echo_objs) \
		$(echo_c_dep_files) \
		$(echo_asm_temps) \
		$(echo_preproc_temps) \
		echo \
		$(syscall_client_objs) \
		$(syscall_client_c_dep_files) \
		$(syscall_client_asm_temps) \
		$(syscall_client_preproc_temps) \
		syscall-client \
		$(NULL)
	rm -f \
		$(foreach tool,$(tools),$(tool)) \
		$(foreach tool,$(tools),$($(tool)_objs)) \
		$(NULL)

debug: image
	$(CROSS_COMPILE)-gdb image --eval="target remote :1234"

run: image
	qemu-system-arm -serial stdio -s -S -kernel image -cpu arm1136 -M versatilepb

.PHONY: clean debug run
