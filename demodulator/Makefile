#------------------------------------------------------------------------------
# CONFIGURE
# - SDK_PATH   : path to SDK directory
#
# - SD_NAME    : e.g s132, s140
# - SD_VERSION : SoftDevice version e.g 6.0.0
# - SD_HEX     : to bootloader hex binary
#------------------------------------------------------------------------------

SDK_PATH     = lib/sdk/components
SDK11_PATH   = lib/sdk11/components
TUSB_PATH    = lib/tinyusb/src
NRFX_PATH    = lib/nrfx
SD_PATH      = lib/softdevice/$(SD_FILENAME)

GIT_VERSION = $(shell git describe --dirty --always --tags)
GIT_SUBMODULE_VERSIONS = $(shell git submodule status | cut -d' ' -f3,4 | paste -s -d" " -)

#------------------------------------------------------------------------------
# Tool configure
#------------------------------------------------------------------------------

# Toolchain commands
# Should be added to your PATH
CROSS_COMPILE ?= arm-none-eabi-
CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)as
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE    = $(CROSS_COMPILE)size
GDB     = $(CROSS_COMPILE)gdb

MK = mkdir -p
RM = rm -rf

# auto-detect BMP on macOS, otherwise have to specify
BMP_PORT ?= $(shell ls -1 /dev/cu.usbmodem????????1 | head -1)
GDB_BMP = $(GDB) -ex 'target extended-remote $(BMP_PORT)' -ex 'monitor swdp_scan' -ex 'attach 1'

#---------------------------------
# Select the board to build
#---------------------------------
BOARD ?= simmel

# Build directory
BUILD = _build

# Board specific
-include src/boards/$(BOARD)/board.mk

SD_NAME = s140
DFU_DEV_REV = 52840
CFLAGS += -DNRF52833_XXAA -DS140 -DCFG_TUSB_MCU=OPT_MCU_NRF5X
SD_VERSION = 7.0.1

SD_FILENAME  = $(SD_NAME)_nrf52_$(SD_VERSION)
SD_HEX       = $(SD_PATH)/$(SD_FILENAME)_softdevice.hex

# linker by MCU and SoftDevice eg. nrf52840_s140_v6.ld
LD_FILE      = linker/$(MCU_SUB_VARIANT)_$(SD_NAME)_v$(word 1, $(subst ., ,$(SD_VERSION))).ld

# compiled file name
OUT_FILE = nus-test

#------------------------------------------------------------------------------
# SOURCE FILES
#------------------------------------------------------------------------------

# all files in src
C_SRC += $(wildcard src/*.c)
C_SRC += $(wildcard src/bpsk/*.c)
C_SRC += $(wildcard src/cmsis/source/*.c)

# all sources files in specific board
C_SRC += $(wildcard src/boards/$(BOARD)/*.c)

# nrfx
C_SRC += $(NRFX_PATH)/drivers/src/nrfx_power.c
C_SRC += $(NRFX_PATH)/drivers/src/nrfx_nvmc.c
C_SRC += $(NRFX_PATH)/mdk/system_$(MCU_SUB_VARIANT).c

# tinyusb
C_SRC += $(TUSB_PATH)/tusb.c
C_SRC += $(TUSB_PATH)/common/tusb_fifo.c
C_SRC += $(TUSB_PATH)/device/usbd.c
C_SRC += $(TUSB_PATH)/device/usbd_control.c
C_SRC += $(TUSB_PATH)/portable/nordic/nrf5x/dcd_nrf5x.c
C_SRC += $(TUSB_PATH)/class/cdc/cdc_device.c
IPATH += lib/tinyusb/hw/mcu/nordic/nrf5x/s140_nrf52_6.1.1_API/include/nrf52
IPATH += lib/tinyusb/hw/mcu/nordic/nrf5x/s140_nrf52_6.1.1_API/include

#------------------------------------------------------------------------------
# Assembly Files
#------------------------------------------------------------------------------
ASM_SRC ?= $(NRFX_PATH)/mdk/gcc_startup_$(MCU_SUB_VARIANT).S

#------------------------------------------------------------------------------
# INCLUDE PATH
#------------------------------------------------------------------------------

# src
IPATH += src
IPATH += src/boards/$(BOARD)

IPATH += src/cmsis/include
IPATH += src/bpsk
IPATH += src/boards
IPATH += $(TUSB_PATH)

# nrfx
IPATH += $(NRFX_PATH)
IPATH += $(NRFX_PATH)/mdk
IPATH += $(NRFX_PATH)/hal
IPATH += $(NRFX_PATH)/drivers/include
IPATH += $(NRFX_PATH)/drivers/src

IPATH += $(SDK11_PATH)/libraries/bootloader_dfu/hci_transport
IPATH += $(SDK11_PATH)/libraries/bootloader_dfu
IPATH += $(SDK11_PATH)/drivers_nrf/pstorage
IPATH += $(SDK11_PATH)/ble/common
IPATH += $(SDK11_PATH)/ble/ble_services/ble_dfu
IPATH += $(SDK11_PATH)/ble/ble_services/ble_dis

IPATH += $(SDK_PATH)/libraries/timer
IPATH += $(SDK_PATH)/libraries/scheduler
IPATH += $(SDK_PATH)/libraries/crc16
IPATH += $(SDK_PATH)/libraries/util
IPATH += $(SDK_PATH)/libraries/hci/config
IPATH += $(SDK_PATH)/libraries/uart
IPATH += $(SDK_PATH)/libraries/hci
IPATH += $(SDK_PATH)/drivers_nrf/delay

# Softdevice
IPATH += $(SD_PATH)/$(SD_FILENAME)_API/include
IPATH += $(SD_PATH)/$(SD_FILENAME)_API/include/nrf52

INC_PATHS = $(addprefix -I,$(IPATH))

#------------------------------------------------------------------------------
# Compiler Flags
#------------------------------------------------------------------------------

# Debugging/Optimization
CFLAGS += -O3 -ggdb3

CFLAGS += -DPLAYGROUND -DARM_MATH_CM4 -D__FPU_PRESENT=1

#flags common to all targets
CFLAGS += \
	-mthumb \
	-mabi=aapcs \
	-mcpu=cortex-m4 \
	-mtune=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-ffunction-sections \
	-fdata-sections \
	-fno-builtin \
	-fshort-enums \
	-fstack-usage \
	-fno-strict-aliasing \
	-Wall \
	-Wextra \
	-Werror \
	-Wfatal-errors \
	-Werror-implicit-function-declaration \
	-Wundef \
	-Wshadow \
	-Wwrite-strings \
	-Wsign-compare \
	-Wmissing-format-attribute \
	-Wno-endif-labels \
	-Wunreachable-code

# Suppress warning caused by SDK
CFLAGS += -Wno-unused-parameter -Wno-expansion-to-defined

# TinyUSB tusb_hal_nrf_power_event
CFLAGS += -Wno-cast-function-type

# Defined Symbol (MACROS)
CFLAGS += -D__HEAP_SIZE=0
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DCONFIG_NFCT_PINS_AS_GPIOS
CFLAGS += -DNO_MAIN
# CFLAGS += -DSOFTDEVICE_PRESENT

CFLAGS += -DUF2_VERSION='"$(GIT_VERSION) $(GIT_SUBMODULE_VERSIONS) $(SD_NAME) $(SD_VERSION)"'

_VER = $(subst ., ,$(word 1, $(subst -, ,$(GIT_VERSION))))

#------------------------------------------------------------------------------
# Linker Flags
#------------------------------------------------------------------------------

LDFLAGS += \
	$(CFLAGS) \
	-Wl,-L,linker -Wl,-T,$(LD_FILE) \
	-Wl,-Map=$@.map -Wl,-cref -Wl,-gc-sections \
	-specs=nosys.specs -specs=nano.specs

LIBS += -lm -lc

#------------------------------------------------------------------------------
# Assembler flags
#------------------------------------------------------------------------------
ASFLAGS += $(CFLAGS)


#function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))

C_SOURCE_FILE_NAMES = $(notdir $(C_SRC))
C_PATHS = $(call remduplicates, $(dir $(C_SRC) ) )
C_OBJECTS = $(addprefix $(BUILD)/, $(C_SOURCE_FILE_NAMES:.c=.o) )

ASM_SOURCE_FILE_NAMES = $(notdir $(ASM_SRC))
ASM_PATHS = $(call remduplicates, $(dir $(ASM_SRC) ))
ASM_OBJECTS = $(addprefix $(BUILD)/, $(ASM_SOURCE_FILE_NAMES:.S=.o) )

vpath %.c $(C_PATHS)
vpath %.S $(ASM_PATHS)

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

#------------------------------------------------------------------------------
# BUILD TARGETS
#------------------------------------------------------------------------------

# Verbose mode (V=). 0: default, 1: print out CFLAG, LDFLAG 2: print all compile command
ifeq ("$(V)","1")
$(info CFLAGS   $(CFLAGS))
$(info )
$(info LDFLAGS  $(LDFLAGS))
$(info )
$(info ASFLAGS $(ASFLAGS))
$(info )
endif

.PHONY: all clean flash sd gdb

# default target to build
all: $(BUILD)/$(OUT_FILE)-nosd.hex
	@echo "SoftDevice hex file:"
	@echo "    $(SD_HEX)"
	@echo "Bootloader hex file:"
	@echo "    $(BUILD)/$(OUT_FILE)-nosd.hex"
	@echo "Load these two files using openocd."

#------------------- Flash target -------------------

check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))

gdb: $(BUILD)/$(OUT_FILE)-nosd.elf
	$(GDB_BMP) $<

#------------------- Compile rules -------------------

## Create build directories
$(BUILD):
	@$(MK) $@

clean:
	@$(RM) $(BUILD)

# Create objects from C SRC files
$(BUILD)/%.o: %.c
	@echo CC $(notdir $<)
	@$(CC) $(CFLAGS) $(INC_PATHS) -c -o $@ $<

# Assemble files
$(BUILD)/%.o: %.S
	@echo AS $(notdir $<)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) $(INC_PATHS) -c -o $@ $<

# Link
$(BUILD)/$(OUT_FILE)-nosd.elf: $(BUILD) $(OBJECTS)
	@echo LD $(OUT_FILE)-nosd.elf
	@$(CC) -o $@ $(LDFLAGS) $(OBJECTS) -Wl,--start-group $(LIBS) -Wl,--end-group
	@$(SIZE) $@

#------------------- Binary generator -------------------

## Create binary .hex file from the .elf file
$(BUILD)/$(OUT_FILE)-nosd.hex: $(BUILD)/$(OUT_FILE)-nosd.elf
	@echo CR $(OUT_FILE)-nosd.hex
	@$(OBJCOPY) -O ihex $< $@
