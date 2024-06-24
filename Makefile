include build_scripts/config.mk

.PHONY: all disk_image clean always

all: always disk_image

include build_scripts/toolchain.mk

IMAGE:=myos_disk.img
IMAGE_COMPONENTS := $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin

disk_image: $(BUILD_DIR)/$(IMAGE)

export MTOOLSRC:=$(shell mktemp)
$(BUILD_DIR)/$(IMAGE): $(IMAGE_COMPONENTS)
	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
		xxd -r - $(BUILD_DIR)/stage1.bin
	dd if=/dev/zero of=$@ bs=512 count=3840 > /dev/null 2>&1
	dd if=$(BUILD_DIR)/stage1.bin of=$@ conv=notrunc > /dev/null 2>&1
	dd if=$(BUILD_DIR)/stage2.bin of=$@ seek=1 conv=notrunc > /dev/null 2>&1
	echo "drive c: file=\"$@\" partition=1" > $(MTOOLSRC)
	mpartition -I c:
	mpartition -c -t 30 -h 4 -s 32 c:
	mformat c:
	mcopy $(BUILD_DIR)/kernel.bin "c:kernel.bin"
	mcopy test.txt "c:test.txt"
	rm -f $(MTOOLSRC)

#
# Bootloader
#
$(BUILD_DIR)/stage1.bin: always
	$(MAKE) -C src/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR))

$(BUILD_DIR)/stage2.bin: always
	$(MAKE) -C src/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Kernel
#
$(BUILD_DIR)/kernel.bin: always
	@$(MAKE) -C src/kernel BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Always
#
always:
	mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
	@$(MAKE) -C src/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	@$(MAKE) -C src/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	@$(MAKE) -C src/kernel BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	rm -f $(BUILD_DIR)/$(IMAGE)
	rm -rf $(BUILD_DIR)

run:
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(IMAGE),index=0,media=disk,format=raw

debug:
	bochs -f bochs_config
