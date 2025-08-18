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

# ===== Sources =====
SRCS := \
  $(SRC)/main.c \
  $(ENV)/syscalls_min.c \
  $(ENV)/system_samd21.c \
  $(ENV)/startup_samd21_gcc.c

# ===== Outputs =====
BUILDDIR := build
OUTDIR   := out
TARGET   := samd21g18-blinky
ELF := $(OUTDIR)/$(TARGET).elf
HEX := $(OUTDIR)/$(TARGET).hex
BIN := $(OUTDIR)/$(TARGET).bin
LD  := $(ENV)/samd21g18a.ld

# ===== Flags =====
CFLAGS  := -mcpu=cortex-m0plus -mthumb -mlittle-endian \
           -O2 -Wall -Wextra -Werror=implicit-function-declaration -Wundef -Wshadow -Wdouble-promotion -Wformat=2 \
           -std=c11 -fdata-sections -ffunction-sections -g3 \
           -D_RTE_ -D$(PART) -MMD -MP \
           -I$(CMSIS) -I$(DFP_INC) -I$(ENV) -I$(SRC)

LDFLAGS := -mcpu=cortex-m0plus -mthumb -mlittle-endian \
           -Wl,--gc-sections -Wl,-Map=$(OUTDIR)/$(TARGET).map \
           -specs=nano.specs -T $(LD)
LDLIBS  := -lc -lgcc

OBJS := $(SRCS:%=$(BUILDDIR)/%.obj)
DEPS := $(OBJS:.obj=.d)

# ===== Rules =====
.PHONY: all clean

all: $(ELF) $(HEX) $(BIN)
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

flash: $(ELF)
	$(OPENOCD) -f $(OPENOCD_IF) -f $(OPENOCD_TARGET) \
		-c "program $(ELF) verify reset exit"
