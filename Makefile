# ===== Toolchain (override from env if you like) =====
CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

# ===== Part selection =====
PART ?= __SAMD21G18A__

# ===== Pack include paths (adjust versions if needed) =====
CMSIS   ?= $(HOME)/.cache/arm/packs/ARM/CMSIS/6.2.0/CMSIS/Core/Include
DFP_INC ?= $(HOME)/.cache/arm/packs/Keil/SAMD21_DFP/1.3.1/Device/SAMD21A/Include

# ===== Layout =====
ENV := env
SRC := src

# ==== Peripherals =====
STORAGE ?= $(SRC)/peripherals/storage
SD      ?= $(SRC)/peripherals/storage/sd-helper
RADIO   ?= $(SRC)/peripherals/radio
SENSORS ?= $(SRC)/peripherals/sensors
AHT     ?= $(SENSORS)/aht20
BOARD   ?= $(SRC)/peripherals/board

# ==== Drivers =====
SPI   ?= $(SRC)/drivers/spi
CLOCK ?= $(SRC)/drivers/clock
I2C-M ?= $(SRC)/drivers/i2c/i2c-master
I2C-S ?= $(SRC)/drivers/i2c/i2c-slave
I2C-H ?= $(SRC)/drivers/i2c/i2c-helper
UART  ?= $(SRC)/drivers/uart

# ===== Sources =====
SRCS := \
  $(SRC)/main.c \
  $(SRC)/variables.c \
  $(STORAGE)/storage.c \
  $(SD)/sd-helper.c \
  $(AHT)/aht20.c \
  $(RADIO)/radio.c \
  $(BOARD)/board.c \
  $(CLOCK)/clock.c \
  $(SPI)/spi.c \
  $(I2C-S)/i2c-slave.c \
  $(I2C-M)/i2c-master.c \
  $(I2C-H)/i2c-helper.c \
  $(UART)/uart.c \
  $(ENV)/syscalls_min.c \
  $(ENV)/system_samd21.c \
  $(ENV)/startup_samd21_gcc.c

# ===== Outputs =====
BUILDDIR := .build
OUTDIR   := .out
TARGET   := samd21g18-blinky
ELF := $(OUTDIR)/$(TARGET).elf
HEX := $(OUTDIR)/$(TARGET).hex
BIN := $(OUTDIR)/$(TARGET).bin
LD  := $(ENV)/samd21g18a.ld

# ===== Flags =====
# C compilation flags (no -specs here)
CFLAGS := -Os -ffunction-sections -fdata-sections \
  -mcpu=cortex-m0plus -mthumb -mlittle-endian \
  -O2 -Wall -Wextra -Werror=implicit-function-declaration -Wundef -Wshadow -Wdouble-promotion -Wformat=2 \
  -std=c11 -g3 \
  -D_RTE_ -D$(PART) -MMD -MP \
  -I$(CMSIS) -I$(DFP_INC) -I$(ENV) -I$(SRC) -I$(I2C-H) -I$(I2C-S) -I$(I2C-M) -I$(UART) -I$(SPI) \
  -I$(CLOCK) -I$(STORAGE) -I$(RADIO) -I$(BOARD) -I$(SD) -I$(AHT)

# Linker flags — put specs here ONCE
LDFLAGS := \
  -specs=nano.specs -u _printf_float -u _scanf_float \
  -Wl,--gc-sections -Wl,-Map=$(OUTDIR)/$(TARGET).map \
  -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft \
  -T env/samd21g18a.ld

# Libraries — group + libm + nosys
LDLIBS := -Wl,--start-group -lc -lm -lnosys -lgcc -Wl,--end-group

OBJS := $(SRCS:%=$(BUILDDIR)/%.obj)
DEPS := $(OBJS:.obj=.d)

# ===== Rules =====
.PHONY: clean build write write-read flash debug

build: $(ELF) $(HEX) $(BIN)
	$(SIZE) $(ELF)

$(BUILDDIR)/%.c.obj: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS) $(LD)
	@mkdir -p $(OUTDIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILDDIR) $(OUTDIR)

# include auto-deps
-include $(DEPS)

# Flash with OpenOCD
OPENOCD=openocd
OPENOCD_IF=interface/cmsis-dap.cfg
OPENOCD_TARGET=target/at91samdXX.cfg

write-read: $(ELF)
	$(OPENOCD) -f $(OPENOCD_IF) -f $(OPENOCD_TARGET) \
		-c "init; arm semihosting enable; program $(ELF) verify; reset run"

write: $(ELF)
	$(OPENOCD) -f $(OPENOCD_IF) -f $(OPENOCD_TARGET) \
		-c "init; arm semihosting disable; program $(ELF) verify; reset halt; shutdown"

flash:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) write

debug:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) write-read
