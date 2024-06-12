.RECIPEPREFIX := $() $()

.PHONY: all

all: build $(BUILD_DIR)/stage1.bin

$(BUILD_DIR)/stage1.bin: stage1.asm
    $(ASM) stage1.asm -f bin -o $(BUILD_DIR)/stage1.bin

build:
    mkdir -p $(BUILD_DIR)

clean:
    rm -f $(BUILD_DIR)/stage1.bin
