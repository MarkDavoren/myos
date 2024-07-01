include build_scripts/config.mk

.PHONY: all ext_disk_image fat_disk_image floppy_image clean always

all: always fat_disk_image  # floppy_image

include build_scripts/toolchain.mk

DISK_IMAGE := myos_disk
FLOPPY_IMAGE := myos_floppy.img
IMAGE_COMPONENTS := $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin

# EXT disk (partitioned)

ext_disk_image: $(BUILD_DIR)/$(DISK_IMAGE).ext

export MTOOLSRC:=$(shell mktemp)
export EXTTEMP:=$(shell mktemp)
$(BUILD_DIR)/$(DISK_IMAGE).ext: $(IMAGE_COMPONENTS) $(ROOT_DIR)/8MB
	# Set stage2 size into stage1
	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
		xxd -r - $(BUILD_DIR)/stage1.bin
	# Create boot area
	dd if=/dev/zero of=$@ count=64 conv=sparse
	dd if=build/stage1.bin of=$@ conv=notrunc,sparse
	dd if=build/stage2.bin of=$@ seek=1 conv=notrunc,sparse
	# Create ext2 fs
	dd if=/dev/zero of=$(EXTTEMP) count=40960 conv=sparse
	mke2fs -t ext2 -L "MYOSEXT2" -d $(ROOT_DIR) $(EXTTEMP)
	echo write build/kernel.bin /kernel.bin | debugfs -w $(EXTTEMP)
	# Concat it onto boot area and create a partition for it
	dd if=$(EXTTEMP) of=$@ seek=64 conv=sparse,notrunc
	echo "drive c: file=\"$@\" partition=1" > $(MTOOLSRC)
	mpartition -I -c -b 64 -l 40960 c:
	# Cleanup
	rm -f $(MTOOLSRC) $(EXTTEMP)

# FAT disk (partitioned)

fat_disk_image: $(BUILD_DIR)/$(DISK_IMAGE).fat

export MTOOLSRC:=$(shell mktemp)
FAT32 = -F # Forces FAT32 even though there aren't enough clusters. fdisk won't recognize it. Unset this for FAT16
$(BUILD_DIR)/$(DISK_IMAGE).fat: $(IMAGE_COMPONENTS) $(ROOT_DIR)/8MB
	# Set stage2 size into stage1
	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
		xxd -r - $(BUILD_DIR)/stage1.bin
	dd if=/dev/zero of=$@ bs=512 count=41024 > /dev/null 2>&1				# 41024 = 40960 + 64
	dd if=$(BUILD_DIR)/stage1.bin of=$@ conv=notrunc > /dev/null 2>&1
	dd if=$(BUILD_DIR)/stage2.bin of=$@ seek=1 conv=notrunc > /dev/null 2>&1
	# Create a FAT partition
	echo "drive c: file=\"$@\" partition=1" > $(MTOOLSRC)
	mpartition -I -c -b 64 -l 40960 c:
	mformat $(FAT32) c:
	# Copy files over -- TODO: investigate recursive copy
	mcopy $(BUILD_DIR)/kernel.bin "c:kernel.bin"
	mcopy $(ROOT_DIR)/test.txt "c:test.txt"
	mcopy $(ROOT_DIR)/8MB "c:8MB"
	mmd "c:mydir"
	mcopy $(ROOT_DIR)/mydir/test2.txt "c:mydir/test2.txt"
	# Cleanup
	rm -f $(MTOOLSRC)

# FAT floppy (non-partitioned)

floppy_image: $(BUILD_DIR)/$(FLOPPY_IMAGE)

$(BUILD_DIR)/$(FLOPPY_IMAGE): $(IMAGE_COMPONENTS) $(ROOT_DIR)/1MB
	# We use the non-partitioned strategy for floppies
	# The -R option sets the reserved sectors field of the BPB. Size of stage2.bin +1 for the MBR
	rm -f $@
	mkfs.fat -C -s 1 -g 2/80 -R $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 + 1 )) $@ 1440
	# Here we are setting a location in stage1 so it knows how many sectors of stage2.bin it should read in
	echo $(shell printf '1b7: %x' $$(( ($(shell stat -c %s $(BUILD_DIR)/stage2.bin) + 511 ) / 512 )) ) |\
		xxd -r - $(BUILD_DIR)/stage1.bin
	# Copy stage 1 over the MBR created by mkfs.fat, but skip the BPB and EBR to use the mkfs.fat values
	dd if=$(BUILD_DIR)/stage1.bin of=$@ bs=1 skip=62 seek=62 conv=notrunc > /dev/null 2>&1
	# Copy stage to the disk starting at sector 1 (after the MBR)
	dd if=$(BUILD_DIR)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc > /dev/null 2>&1
	mcopy -i $@ $(BUILD_DIR)/kernel.bin "::kernel.bin"
	mcopy -i $@ root/test.txt "::test.txt"
	mcopy -i $@ root/1MB "::1MB"
	mmd -i $@ "::mydir"
	mcopy -i $@ root/mydir/test2.txt "::mydir/test2.txt"

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
	perl -e 'print pack "L*", 0..0x1fffff' > $(ROOT_DIR)/8MB	

$(ROOT_DIR)/1MB:
	perl -e 'print pack "L*", 0..0x3ffff' > $(ROOT_DIR)/1MB	

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

run-fat:
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(DISK_IMAGE).fat,index=0,media=disk,format=raw

run-ext:
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(DISK_IMAGE).ext,index=0,media=disk,format=raw

debug:
	bochs -f bochs_disk_config

run-floppy:
	qemu-system-i386 -drive file=$(BUILD_DIR)/$(FLOPPY_IMAGE),index=0,if=floppy,format=raw -boot order=a

gdb-floppy:
	qemu-system-i386 -s -S -drive file=$(BUILD_DIR)/$(FLOPPY_IMAGE),index=0,if=floppy,format=raw -boot order=a

debug-floppy:
	bochs -f bochs_floppy_config
