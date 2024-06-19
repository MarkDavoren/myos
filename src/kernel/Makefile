TARGET_ASMFLAGS += -f elf
TARGET_CFLAGS += -ffreestanding -nostdlib -I.
TARGET_LIBS += -lgcc
TARGET_LINKFLAGS += -T linker.ld -nostdlib

OBJ_DIR :=$(BUILD_DIR)/kernel

SOURCES_C := $(shell find $(SOURCEDIR) -name '*.c') 
SOURCES_A := $(shell find $(SOURCEDIR) -name '*.asm')
OBJECTS_C := $(patsubst %.c, $(OBJ_DIR)/%.obj, $(SOURCES_C))
OBJECTS_A := $(patsubst %.asm, $(OBJ_DIR)/%.obj, $(SOURCES_A))
OBJECTS   := $(OBJECTS_C) $(OBJECTS_A)

.PHONY: all build clean

all: build $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: $(OBJECTS)
	$(TARGET_LD) $(TARGET_LINKFLAGS) -Wl,-Map=$(BUILD_DIR)/kernel.map -o $@ $^ $(TARGET_LIBS)

$(OBJ_DIR)/%.obj: %.c
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.obj: %.asm
	@$(TARGET_ASM) $(TARGET_ASMFLAGS) -o $@ $<

build:
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/arch/i686
	mkdir -p $(OBJ_DIR)/hal

clean:
	rm -f $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/kernel.map
	rm -rf $(OBJ_DIR)