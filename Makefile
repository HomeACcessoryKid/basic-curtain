PROGRAM = main

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	$(abspath esp-wolfssl) \
	$(abspath esp-cjson) \
	$(abspath esp-homekit) \
    $(abspath esp-wifi-config) \
	$(abspath UDPlogger) \
    $(abspath esp-adv-button)
    
FLASH_SIZE ?= 8
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x8C000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

BUTTON_PIN ?= 0
MOVE_PIN ?= 5
DIR_PIN ?= 4
EXTRA_CFLAGS += -DBUTTON_PIN=$(BUTTON_PIN) -DMOVE_PIN=$(MOVE_PIN) -DDIR_PIN=$(DIR_PIN)

EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
#EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud $(ESPBAUD) --elf $(PROGRAM_OUT)

signature:
	$(openssl sha384 -binary -out firmware/main.bin.sig firmware/main.bin)
	$(printf "%08x" `cat firmware/main.bin | wc -c`| xxd -r -p >>firmware/main.bin.sig)
