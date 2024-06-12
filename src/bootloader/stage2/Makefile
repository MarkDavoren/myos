.RECIPEPREFIX := $() $()

OBJ_DIR :=$(BUILD_DIR)/bootloader/stage2

SOURCES_C := main.c stdio.c
SOURCES_A := stage2.asm x86.asm

OBJECTS_C := $(patsubst %.c, $(OBJ_DIR)/%.obj, $(SOURCES_C))
OBJECTS_A := $(patsubst %.asm, $(OBJ_DIR)/%.obj, $(SOURCES_A))
OBJECTS   := $(OBJECTS_C) $(OBJECTS_A)


.PHONY: build all

all: build $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: $(OBJECTS)
    $(LD16) NAME $(BUILD_DIR)/stage2.bin FILE \{ $(OBJECTS) \} OPTION MAP=$(BUILD_DIR)/stage2.map @linker.lnk

$(OBJ_DIR)/%.obj: %.c
    $(CC16) $(CFLAGS16) -fo=$@ $<

$(OBJ_DIR)/%.obj: %.asm
    $(ASM) $(ASMFLAGS) -o $@ $<

build:
    mkdir -p $(OBJ_DIR)

clean:
    rm -f $(BUILD_DIR)/stage2.bin
    rm -rf $(OBJ_DIR)