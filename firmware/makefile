#
#             LUFA Library
#     Copyright (C) Dean Camera, 2015.
#
#  dean [at] fourwalledcubicle [dot] com
#           www.lufa-lib.org
#
# --------------------------------------
#         LUFA Project Makefile.
# --------------------------------------

# Run "make help" for target help.

MCU          = atmega32u4
ARCH         = AVR8
BOARD        = ADAFRUITU4
F_CPU        = 8000000
F_USB        = $(F_CPU)
OPTIMIZATION = s
TARGET       = USBtoSerial
SRC          = $(TARGET).c Descriptors.c \
               USBTerminal.c \
               CellularComm_SIM800.c \
               CellularTCPIP_SIM800.c \
               TCPIPConsole.c \
               Console.c \
               CommandProcessor.c \
               WaterLevelDisplay.c \
               Display.c \
               TFT_HXD8357D.c \
               PowerMonitor.c \
               SDCard.c \
               SystemTime.c \
               SoftwareSerialTx.c \
               SoftwareSerialRx0.c \
               SIM800.c \
               EEPROM_Util.c \
               EEPROMStorage.c \
               IOPortBitField.c \
               MessageIDQueue.c \
               ByteQueue.c \
               SPIAsync.c \
               StringUtils.c \
               CharString.c \
               CharStringSpan.c \
               $(LUFA_SRC_USB) $(LUFA_SRC_USBCLASS) \
               RAMSentinel.c
LUFA_PATH    = ../../../LUFA
CC_FLAGS     = -DUSE_LUFA_CONFIG_HEADER -IConfig/ -IC:/WinAVR-20100110/avr/bin/
LD_FLAGS     =


# Default target
all:

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_doxygen.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
include $(LUFA_PATH)/Build/lufa_hid.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
include $(LUFA_PATH)/Build/lufa_atprogram.mk
