#*******************************************************************************
#   SUM Chain Ledger App - Makefile
#   Targets: Nano S+ (nanos2), Nano X (nanox)
#*******************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif

include $(BOLOS_SDK)/Makefile.defines

########################################
#        Application configuration     #
########################################

APPNAME      = "SUM Chain"
APPVERSION_M = 1
APPVERSION_N = 0
APPVERSION_P = 0
APPVERSION   = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

# Application source directory
APP_SOURCE_PATH += src
APP_SOURCE_PATH += src/crypto
APP_SOURCE_PATH += src/crypto/blake3

# Application icons
ICON_NANOS   = icons/nanos_app_sumchain.gif
ICON_NANOX   = icons/nanox_app_sumchain.gif
ICON_NANOSP  = icons/nanox_app_sumchain.gif
ICON_STAX    = icons/stax_app_sumchain.gif
ICON_FLEX    = icons/flex_app_sumchain.gif

# Application flags
APP_LOAD_PARAMS = --appFlags 0x000
APP_LOAD_PARAMS += --path ""
APP_LOAD_PARAMS += $(COMMON_LOAD_PARAMS)

# Derive path prefix (SUM Chain uses m/44'/12345'/... where 12345 is placeholder coin type)
# TODO: Replace with actual registered coin type
APP_LOAD_PARAMS += --path "44'/12345'"

ifeq ($(TARGET_NAME),TARGET_NANOS2)
    ICONNAME = $(ICON_NANOSP)
else ifeq ($(TARGET_NAME),TARGET_NANOX)
    ICONNAME = $(ICON_NANOX)
else ifeq ($(TARGET_NAME),TARGET_STAX)
    ICONNAME = $(ICON_STAX)
else ifeq ($(TARGET_NAME),TARGET_FLEX)
    ICONNAME = $(ICON_FLEX)
else
    ICONNAME = $(ICON_NANOS)
endif

########################################
#     Application custom permissions   #
########################################

# No custom permissions needed

########################################
#         NBGL configuration           #
########################################

ENABLE_NBGL_QRCODE = 0
ENABLE_NBGL_KEYBOARD = 0
ENABLE_NBGL_KEYPAD = 0

########################################
#          Features flags              #
########################################

# Enable debugging
DEBUG = 0
ifneq ($(DEBUG),0)
    DEFINES += HAVE_PRINTF
    ifeq ($(TARGET_NAME),TARGET_NANOS)
        DEFINES += PRINTF=screen_printf
    else
        DEFINES += PRINTF=mcu_usb_printf
    endif
else
    DEFINES += PRINTF\(...\)=
endif

# SDK defines
DEFINES += OS_IO_SEPROXYHAL
DEFINES += HAVE_BAGL HAVE_UX_FLOW
DEFINES += HAVE_SPRINTF
DEFINES += HAVE_IO_USB HAVE_L4_USBLIB IO_USB_MAX_ENDPOINTS=4 IO_HID_EP_LENGTH=64 HAVE_USB_APDU
DEFINES += USB_SEGMENT_SIZE=64

# BLE support for Nano X
ifeq ($(TARGET_NAME),TARGET_NANOX)
    DEFINES += HAVE_BLE BLE_COMMAND_TIMEOUT_MS=2000 HAVE_BLE_APDU
endif

# Nano S+ / Nano X specific
ifneq ($(TARGET_NAME),TARGET_NANOS)
    DEFINES += HAVE_GLO096
    DEFINES += BAGL_WIDTH=128 BAGL_HEIGHT=64
    DEFINES += HAVE_BAGL_ELLIPSIS
    DEFINES += HAVE_BAGL_FONT_OPEN_SANS_REGULAR_11PX
    DEFINES += HAVE_BAGL_FONT_OPEN_SANS_EXTRABOLD_11PX
    DEFINES += HAVE_BAGL_FONT_OPEN_SANS_LIGHT_16PX
endif

# App version defines
DEFINES += APPVERSION=\"$(APPVERSION)\"
DEFINES += APPVERSION_MAJOR=$(APPVERSION_M)
DEFINES += APPVERSION_MINOR=$(APPVERSION_N)
DEFINES += APPVERSION_PATCH=$(APPVERSION_P)

# BOLOS SDK marker
DEFINES += HAVE_BOLOS_SDK

########################################
#          Compiler settings           #
########################################

CC      := $(CLANGPATH)clang
CFLAGS  += -O3 -Os
CFLAGS  += -Wall -Wextra -Werror
CFLAGS  += -Wno-unused-parameter
CFLAGS  += -fno-builtin

# Disable SIMD for BLAKE3 (portable only)
CFLAGS  += -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512

AS      := $(GCCPATH)arm-none-eabi-gcc
LD      := $(GCCPATH)arm-none-eabi-gcc
LDFLAGS += -O3 -Os
LDLIBS  += -lm -lgcc -lc

########################################
#          Source files                #
########################################

# Main application sources
APP_SOURCE_FILES += src/main.c
APP_SOURCE_FILES += src/globals.h
APP_SOURCE_FILES += src/crypto.c
APP_SOURCE_FILES += src/address.c
APP_SOURCE_FILES += src/apdu_handlers.c
APP_SOURCE_FILES += src/tx_parser.c
APP_SOURCE_FILES += src/tx_display.c

# BLAKE3 portable implementation (official reference)
APP_SOURCE_FILES += src/crypto/sum_blake3.c
APP_SOURCE_FILES += src/crypto/blake3/blake3.c
APP_SOURCE_FILES += src/crypto/blake3/blake3_portable.c
APP_SOURCE_FILES += src/crypto/blake3/blake3_dispatch.c

########################################
#          Build rules                 #
########################################

include $(BOLOS_SDK)/Makefile.rules

# Custom rules for BLAKE3 (ensure no SIMD)
src/crypto/blake3/%.o: src/crypto/blake3/%.c
	$(CC) $(CFLAGS) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 -c $< -o $@

########################################
#          Dependencies                #
########################################

dep/%.d: %.c Makefile
	@mkdir -p dep
	@$(CC) -MM $(CFLAGS) $< -MF $@ -MT $(<:.c=.o)

########################################
#          Host tests target           #
########################################

.PHONY: test
test:
	$(MAKE) -C tests

.PHONY: clean-test
clean-test:
	$(MAKE) -C tests clean
