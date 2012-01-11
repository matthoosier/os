CROSS_COMPILE = arm-none-eabi

debug:
	./waf
	$(CROSS_COMPILE)-gdb build/image --eval="target remote :1234"

run:
	./waf
	qemu-system-arm -serial stdio -s -S -kernel build/image -cpu arm1136 -M versatilepb

doc:
	doxygen Doxyfile

.PHONY: debug run doc
