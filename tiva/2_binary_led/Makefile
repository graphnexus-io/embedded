TARGET := binary-led

BUILD_DIR := build

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
OBJDUMP := arm-none-eabi-objdump
SIZE    := arm-none-eabi-size
GDB     := gdb-multiarch

CPU_FLAGS := \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard

CFLAGS := \
	$(CPU_FLAGS) \
	-std=c11 \
	-O0 \
	-g3 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-ffreestanding \
	-ffunction-sections \
	-fdata-sections

ASFLAGS := \
	$(CPU_FLAGS) \
	-x assembler-with-cpp \
	-g3

LDFLAGS := \
	$(CPU_FLAGS) \
	-T linker.ld \
	-nostdlib \
	-Wl,--gc-sections \
	-Wl,-Map=$(BUILD_DIR)/$(TARGET).map

LDLIBS := -lgcc

C_SOURCES := \
	src/main.c \
	src/gpio.c \
	src/systick.c

ASM_SOURCES := \
	startup/startup_tm4c123.s

C_OBJECTS := \
	$(patsubst %.c,$(BUILD_DIR)/c/%.o,$(C_SOURCES))

ASM_OBJECTS := \
	$(patsubst %.s,$(BUILD_DIR)/asm/%.o,$(ASM_SOURCES))

OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin
HEX := $(BUILD_DIR)/$(TARGET).hex
LST := $(BUILD_DIR)/$(TARGET).lst

.PHONY: all clean flash erase openocd debug size disassembly

all: $(ELF) $(BIN) $(HEX) $(LST)
	$(SIZE) $(ELF)

$(ELF): $(OBJECTS) linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/c/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(BUILD_DIR)/asm/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -Iinclude -c $< -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(LST): $(ELF)
	$(OBJDUMP) -S -d -C $< > $@

size: $(ELF)
	$(SIZE) -G -d $(ELF)

disassembly: $(LST)
	less $(LST)

openocd:
	openocd -f board/ti_ek-tm4c123gxl.cfg

flash: $(ELF)
	openocd \
		-f board/ti_ek-tm4c123gxl.cfg \
		-c "program $(ELF) verify reset exit"

erase:
	openocd \
		-f board/ti_ek-tm4c123gxl.cfg \
		-c "init" \
		-c "reset halt" \
		-c "stellaris mass_erase 0" \
		-c "reset run" \
		-c "shutdown"

debug: $(ELF)
	$(GDB) $(ELF)

clean:
	rm -rf $(BUILD_DIR)
