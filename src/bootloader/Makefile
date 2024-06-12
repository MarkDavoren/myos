.RECIPEPREFIX := $() $()

.PHONY: all

all: $(BUILD_DIR)/bootloader.bin

$(BUILD_DIR)/bootloader.bin: boot.asm
    $(ASM) boot.asm -f bin -o $(BUILD_DIR)/bootloader.bin

