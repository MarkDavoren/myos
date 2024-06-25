include build_scripts/config.mk

.PHONY: all disk_image floppy_image clean always

all: always disk_image # floppy_image

include build_scripts/toolchain.mk

DISK_IMAGE := myos_disk.img
FLOPPY_IMAGE := myos_floppy.img
IMAGE_COMPONENTS := $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin

disk_image: $(BUILD_DIR)/$(DISK_IMAGE)

export MTOOLSRC:=$(shell mktemp)
$(BUILD_DIR)/$(DISK_IMAGE): $(IMAGE_COMPONENTS)
	# Use the partitioned strategy for hard disks
	# Here we are setting a location in stage1 so it knows how many sectors of stage2.bin it should read in
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

floppy_image: $(BUILD_DIR)/$(FLOPPY_IMAGE)

$(BUILD_DIR)/$(FLOPPY_IMAGE): $(IMAGE_COMPONENTS)
	# We use the non-partitioned strategy for floppies
	# The -R option sets the reserved sectors field of the BPB. Size of stage2.bin +1 for the MBR
	mkfs.fat -C -s 1 -g 2/80 -R $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 + 1 )) $@ 1440
	# Here we are setting a location in stage1 so it knows how many sectors of stage2.bin it should read in
	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
		xxd -r - $(BUILD_DIR)/stage1.bin
	# Copy stage 1 over the MBR created by mkfs.fat, but skip the BPB and EBR to use the mkfs.fat values
	dd if=$(BUILD_DIR)/stage1.bin of=$@ bs=1 skip=62 seek=62 conv=notrunc > /dev/null 2>&1
	# Copy stage to the disk starting at sector 1 (after the MBR)
	dd if=$(BUILD_DIR)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc > /dev/null 2>&1
	mcopy -i $@ $(BUILD_DIR)/kernel.bin "::kernel.bin"
	mcopy -i $@ test.txt "::test.txt"
	mcopy -i $@ test2.txt "::test2.txt"
	mmd -i $@ "::mydir"
	mcopy -i $@ test.txt "::mydir/test.txt"

# $(BUILD_DIR)/$(FLOPPY_IMAGE): $(IMAGE_COMPONENTS)
# 	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
# 		xxd -r - $(BUILD_DIR)/stage1.bin
# 	echo $(shell printf '0e: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 + 1)) ) |\
# 		xxd -r - $(BUILD_DIR)/stage1.bin
# 	dd if=/dev/zero of=$@ bs=512 count=2880 >/dev/null
# 	mkfs.fat -F 12 -n "NBOS" $@ >/dev/null
# 	dd if=$(BUILD_DIR)/stage1.bin of=$@ conv=notrunc
# 	dd if=$(BUILD_DIR)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc
# 	mcopy -i $@ $(BUILD_DIR)/kernel.bin "::kernel.bin"
# 	mcopy -i $@ test.txt "::test.txt"
# 	mcopy -i $@ test2.txt "::test2.txt"
# 	mmd -i $@ "::mydir"
# 	mcopy -i $@ test.txt "::mydir/test.txt"

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
	rm -f $(BUILD_DIR)/$(DISK_IMAGE)
	rm -rf $(BUILD_DIR)

run:
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(DISK_IMAGE),index=0,media=disk,format=raw

debug:
	bochs -f bochs_disk_config

run-floppy:
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(FLOPPY_IMAGE),index=0,if=floppy,format=raw -boot order=a

gdb-floppy:
	qemu-system-i386 -s -S -drive file=$(BUILD_DIR)/$(FLOPPY_IMAGE),index=0,if=floppy,format=raw -boot order=a

debug-floppy:
	bochs -f bochs_floppy_config
