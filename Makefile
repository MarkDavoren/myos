.RECIPEPREFIX := $() $()

ASM=nasm
export ASM

SRC_DIR=src
BUILD_DIR=build

COMPONENTS := bootloader kernel
BINARY_PATHS := $(addprefix $(BUILD_DIR)/,$(addsuffix .bin,$(notdir $(COMPONENTS))))

.PHONY: all build floppy_image bootloader kernel clean

all: build floppy_image

#
# Floppy image
#
floppy_image: $(COMPONENTS) $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: $(BINARY_PATHS)
    dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
    mkfs.fat -F 12 -n "MYOS" $(BUILD_DIR)/main_floppy.img
    dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
    mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::kernel.bin"
	
$(COMPONENTS):
    $(MAKE) -C $(SRC_DIR)/$@ BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Setup build
#
build:
    @mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
    rm -rf $(BUILD_DIR)
