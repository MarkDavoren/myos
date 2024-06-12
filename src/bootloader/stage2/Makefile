.RECIPEPREFIX := $() $()

SOURCES_C := main.c stdio.c
SOURCES_A := stage2.asm bios.asm
OBJECTS_C=$(patsubst %.c, $(BUILD_DIR)/stage2/%.obj, $(SOURCES_C))
OBJECTS_A=$(patsubst %.asm, $(BUILD_DIR)/stage2/%.obj, $(SOURCES_A))


.PHONY: build all

all: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: $(OBJECTS_A) $(OBJECTS_C)
    $(LD16) NAME $(BUILD_DIR)/stage2.bin FILE \{ $(OBJECTS_A) $(OBJECTS_C) \} OPTION MAP=$(BUILD_DIR)/stage2.map @linker.lnk

$(BUILD_DIR)/stage2/%.obj: %.c
    $(CC16) $(CCFLAGS16) -fo=$@ $<

$(BUILD_DIR)/stage2/%.obj: %.asm
    $(ASM) $(ASMFLAGS) -o $@ $<

build:
    @mkdir -p $(BUILD_DIR)/stage2

clean:
    rm -f $(BUILD_DIR)/stage2.bin
    rm -rf $(BUILD_DIR)/stage2