.RECIPEPREFIX := $() $()

.PHONY: all 

all: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: main.asm
    $(ASM) main.asm -f bin -o $(BUILD_DIR)/kernel.bin

