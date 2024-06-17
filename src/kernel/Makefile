TARGET_ASMFLAGS += -f elf
TARGET_CFLAGS += -ffreestanding -nostdlib
TARGET_LIBS += -lgcc
TARGET_LINKFLAGS += -T linker.ld -nostdlib

OBJ_DIR :=$(BUILD_DIR)/kernel

SOURCES_C := $(wildcard *.c)
SOURCES_A := $(wildcard *.asm)
OBJECTS_C := $(patsubst %.c, $(BUILD_DIR)/kernel/c/%.obj, $(SOURCES_C))
OBJECTS_A := $(patsubst %.asm, $(BUILD_DIR)/kernel/asm/%.obj, $(SOURCES_A))
OBJECTS   := $(OBJECTS_C) $(OBJECTS_A)

.PHONY: all build clean

all: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: $(OBJECTS)
	$(TARGET_LD) $(TARGET_LINKFLAGS) -Wl,-Map=$(BUILD_DIR)/kernel.map -o $@ $^ $(TARGET_LIBS)

$(OBJ_DIR)/%.obj: %.c
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.obj: %.asm
	@$(TARGET_ASM) $(TARGET_ASMFLAGS) -o $@ $<

build:
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(BUILD_DIR)/kernel.bin
	rm -rf $(OBJ_DIR)