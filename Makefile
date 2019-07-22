PROGRAM = main

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/rboot-ota \
	$(abspath esp-wolfssl) \
	$(abspath esp-cjson) \
	$(abspath esp-homekit)

FLASH_SIZE ?= 8
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x8C000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

BUTTON_PIN ?= 0
MOVE_PIN ?= 5
DIR_PIN ?= 4
EXTRA_CFLAGS += -DBUTTON_PIN=$(BUTTON_PIN) -DMOVE_PIN=$(MOVE_PIN) -DDIR_PIN=$(DIR_PIN)


include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud $(ESPBAUD) --elf $(PROGRAM_OUT)

signature:
	$(openssl sha384 -binary -out firmware/main.bin.sig firmware/main.bin)
	$(printf "%08x" `cat firmware/main.bin | wc -c`| xxd -r -p >>firmware/main.bin.sig)
