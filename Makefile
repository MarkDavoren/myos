.RECIPEPREFIX := $() $()

ASM=nasm

SRC_DIR=src
BUILD_DIR=build

.PHONY: floppy_image clean init

#
# Floppy image
#
floppy_image: $(BUILD_DIR)/main_floppy.img


$(BUILD_DIR)/main_floppy.img: $(BUILD_DIR)/bootloader.bin $(BUILD_DIR)/kernel.bin
    dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
    mkfs.fat -F 12 -n "MYOS" $(BUILD_DIR)/main_floppy.img
    dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
    mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::kernel.bin"
	
#
# Bootloader
#
$(BUILD_DIR)/bootloader.bin: $(SRC_DIR)/bootloader/boot.asm
    $(ASM) $(SRC_DIR)/bootloader/boot.asm -f bin -o $(BUILD_DIR)/bootloader.bin

#
# Kernel
#
$(BUILD_DIR)/kernel.bin: $(SRC_DIR)/kernel/main.asm
    $(ASM) $(SRC_DIR)/kernel/main.asm -f bin -o $(BUILD_DIR)/kernel.bin

#
# Initialization
#
init:
    mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
    rm -rf $(BUILD_DIR)

all: clean init floppy_image
