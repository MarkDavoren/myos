TARGET_ASMFLAGS += -f elf
TARGET_CFLAGS += -ffreestanding -nostdlib -I.
TARGET_LIBS += -lgcc
TARGET_LINKFLAGS += -T linker.ld -nostdlib

OBJ_DIR :=$(BUILD_DIR)/stage2

SOURCES_C := $(shell find . -name '*.c') 
SOURCES_A := $(shell find . -name '*.asm')
INCLUDE_H := $(shell find . -name '*.h')
INCLUDE_A := $(shell find . -name '*.inc')

OBJECTS_C := $(patsubst %.c, $(OBJ_DIR)/c/%.obj, $(SOURCES_C))
OBJECTS_A := $(patsubst %.asm, $(OBJ_DIR)/asm/%.obj, $(SOURCES_A))
OBJECTS   := $(OBJECTS_C) $(OBJECTS_A)

.PHONY: all clean

all: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: $(OBJECTS)
	$(TARGET_LD) $(TARGET_LINKFLAGS) -Wl,-Map=$(BUILD_DIR)/stage2.map -o $@ $^ $(TARGET_LIBS)

$(OBJ_DIR)/c/%.obj: %.c $(INCLUDE_H)
	@mkdir -p $(@D)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/asm/%.obj: %.asm $(INCLUDE_A)
	@mkdir -p $(@D)
	$(TARGET_ASM) $(TARGET_ASMFLAGS) -o $@ $<

clean:
	rm -f $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/stage2.map
	rm -rf $(OBJ_DIR)