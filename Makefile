.RECIPEPREFIX := $() $()

export ASM := nasm
export ASMFLAGS := -f obj
export CC16 := /usr/bin/watcom/binl64/wcc
export CFLAGS16 := -4 -d3 -s -wx -ms -zl -zq
export LD16 := /usr/bin/watcom/binl64/wlink

SRC_DIR=src
BUILD_DIR=build

SUBDIRS := bootloader

.PHONY: all build floppy_image bootloader kernel clean

all: build floppy_image

#
# Floppy image
#
floppy_image: $(SUBDIRS) $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin
    dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
    mkfs.fat -F 12 -n "MYOS" $(BUILD_DIR)/main_floppy.img
    dd if=$(BUILD_DIR)/stage1.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
    mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/stage2.bin "::stage2.bin"
#    mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::kernel.bin"
	
$(SUBDIRS):
    $(MAKE) -C $(SRC_DIR)/$@ BUILD_DIR=$(abspath $(BUILD_DIR)) $(MAKECMDGOALS)

#
# Setup build
#
build:
    mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
    rm -rf $(BUILD_DIR)
