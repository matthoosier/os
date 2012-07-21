CROSS_COMPILE = arm-none-eabi

QEMU_SERIAL_STDIO = -serial stdio
QEMU_SERIAL_TELNET = -serial telnet:localhost:1235,server
QEMU_SERIAL_CONSOLE = -nographic

QEMU_SERIAL = $(QEMU_SERIAL_CONSOLE)

debug:
	./waf
	$(CROSS_COMPILE)-gdb -x .gdbinit.arm build/image --eval="target remote :1234"

run:
	./waf
	qemu-system-arm $(QEMU_SERIAL) -s -S -kernel build/image -cpu arm1136 -M versatilepb

.PHONY: debug run doc
