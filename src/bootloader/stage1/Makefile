.RECIPEPREFIX := $() $()

.PHONY: all

all: $(BUILD_DIR)/stage1.bin

$(BUILD_DIR)/stage1.bin: stage1.asm
    $(ASM) stage1.asm -f bin -o $(BUILD_DIR)/stage1.bin
