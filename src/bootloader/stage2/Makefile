TARGET_ASMFLAGS += -f elf
TARGET_CFLAGS += -ffreestanding -nostdlib
TARGET_LIBS += -lgcc
TARGET_LINKFLAGS += -T linker.ld -nostdlib

OBJ_DIR :=$(BUILD_DIR)/stage2

SOURCES_C := main.c stdio.c disk.c fat.c utility.c ctype.c string.c
SOURCES_A := stage2.asm x86.asm bios.asm

OBJECTS_C := $(patsubst %.c, $(OBJ_DIR)/%.obj, $(SOURCES_C))
OBJECTS_A := $(patsubst %.asm, $(OBJ_DIR)/%.obj, $(SOURCES_A))
OBJECTS   := $(OBJECTS_C) $(OBJECTS_A)

.PHONY: all clean

all: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: $(OBJECTS)
	$(TARGET_LD) $(TARGET_LINKFLAGS) -Wl,-Map=$(BUILD_DIR)/stage2.map -o $@ $^ $(TARGET_LIBS)

$(OBJ_DIR)/%.obj: %.c
	@mkdir -p $(@D)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.obj: %.asm
	@mkdir -p $(@D)
	$(TARGET_ASM) $(TARGET_ASMFLAGS) -o $@ $<

clean:
	rm -f $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/stage2.map
	rm -rf $(OBJ_DIR)