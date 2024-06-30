include build_scripts/config.mk

.PHONY: all disk_image floppy_image clean always

all: always ext2_disk_image #disk_image # floppy_image

include build_scripts/toolchain.mk

DISK_IMAGE := myos_disk
FLOPPY_IMAGE := myos_floppy.img
IMAGE_COMPONENTS := $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin

# EXT2 disk (partitioned)

ext2_disk_image: $(BUILD_DIR)/$(DISK_IMAGE).ext2

export MTOOLSRC:=$(shell mktemp)
export EXT2TEMP:=$(shell mktemp)
$(BUILD_DIR)/$(DISK_IMAGE).ext2: $(IMAGE_COMPONENTS)
	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
		xxd -r - $(BUILD_DIR)/stage1.bin
	dd if=/dev/zero of=$@ count=64 conv=sparse
	dd if=build/stage1.bin of=$@ conv=notrunc,sparse
	dd if=build/stage2.bin of=$@ seek=1 conv=notrunc,sparse
	dd if=/dev/zero of=$(EXT2TEMP) count=40960 conv=sparse
	mke2fs -t ext2 -L "MYOSEXT2" -d root $(EXT2TEMP)
	echo write build/kernel.bin /kernel.bin | debugfs -w $(EXT2TEMP)
	dd if=$(EXT2TEMP) of=$@ seek=64 conv=sparse,notrunc
	echo "drive c: file=\"$@\" partition=1" > $(MTOOLSRC)
	mpartition -I -c -b 64 -l 40960 c:
	rm -f $(MTOOLSRC) $(EXT2TEMP)



# FAT disk (partitioned)

disk_image: $(BUILD_DIR)/$(DISK_IMAGE).fat

export MTOOLSRC:=$(shell mktemp)
FAT32 = -F # Forces FAT32 even though there aren't enough clusters. fdisk won't recognize it. Unset this for FAT16
$(BUILD_DIR)/$(DISK_IMAGE).fat: $(IMAGE_COMPONENTS)
	# Use the partitioned strategy for hard disks
	# Here we are setting a location in stage1 so it knows how many sectors of stage2.bin it should read in
	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
		xxd -r - $(BUILD_DIR)/stage1.bin
	dd if=/dev/zero of=$@ bs=512 count=263000 > /dev/null 2>&1
	dd if=$(BUILD_DIR)/stage1.bin of=$@ conv=notrunc > /dev/null 2>&1
	dd if=$(BUILD_DIR)/stage2.bin of=$@ seek=1 conv=notrunc > /dev/null 2>&1
	echo "drive c: file=\"$@\" partition=1" > $(MTOOLSRC)
	mpartition -I c:
	mpartition -c -t 256 -h 32 -s 32 c:
	mformat $(FAT32) c:
	mcopy $(BUILD_DIR)/kernel.bin "c:kernel.bin"
	mcopy test.txt "c:test.txt"
	mmd "c:mydir"
	mcopy test2.txt "c:mydir/test.txt"
	rm -f $(MTOOLSRC)

# FAT floppy (non-partitioned)
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
	mcopy -i $@ test2.txt "::mydir/test.txt"

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

# Test files

$(ROOT_DIR)/8MB:
	perl -e 'print pack "L*", 0..0x1fffff' > root/8MB	

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
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(DISK_IMAGE).ext2,index=0,media=disk,format=raw

debug:
	bochs -f bochs_disk_config

run-floppy:
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(FLOPPY_IMAGE),index=0,if=floppy,format=raw -boot order=a

gdb-floppy:
	qemu-system-i386 -s -S -drive file=$(BUILD_DIR)/$(FLOPPY_IMAGE),index=0,if=floppy,format=raw -boot order=a

debug-floppy:
	bochs -f bochs_floppy_config
