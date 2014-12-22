MCU       := atmega32
DUDEMCU   := m32
FLASHSIZE := 32768
RAMSIZE   := 2048
LFUSE     ?= 9F # BODLEVEL=1 BODEN=0 SUT=01 CKSEL=1111
HFUSE     ?= 01 # OCDEN=0 JTAGEN=0 SPIEN=0 CKOPT=0 EESAVE=0 BOOTSZ=00 BOOTRST=1
LOCK      ?= 3F

CC        := avr-gcc
LD        := avr-ld
NM        := avr-nm
SIZE      := avr-size
OBJCOPY   := avr-objcopy
DUDE      := sudo avrdude -p $(DUDEMCU) -c stk500v2 -P usb -q -q

CFLAGS	  := -DF_CPU=14745600 -O2 -std=gnu99 -pipe -mmcu=$(MCU) \
	         -fpack-struct -frename-registers -fshort-enums \
			 -Wall -Wextra -Wshadow \
			 -Wl,-u,vfprintf -lprintf_min

.PHONY: build clean erase flash fuses lock burn

build: demo.hex demo.bin
	@ $(SIZE) demo | cut -f1,2,3 | ( \
	read foo; read text data bss; \
	code=$$(($$text+$$data)); \
	codepercent=$$((100*$$code/$(FLASHSIZE))); \
	mem=$$(($$data+$$bss)); \
	mempercent=$$((100*$$mem/$(RAMSIZE))); \
	echo "code: $$code bytes ($$codepercent%); ram: $$mem bytes ($$mempercent%)" );

demo.hex: demo
	$(OBJCOPY) -O ihex -j .text -j .data $^ $@
demo.bin: demo
	$(OBJCOPY) -O binary -j .text -j .data $^ $@

demo: demo.c iinic.c iinic.h
	$(CC) $(CFLAGS) demo.c iinic.c -o $@

clean:
	$(RM) demo demo.bin demo.hex

erase:
	$(DUDE) -e
reset:
	$(DUDE)
flash: demo.hex
	$(DUDE) -e -U flash:w:demo.hex:i
fuses:
	$(DUDE) -u -U lfuse:w:0x$(strip $(LFUSE)):m -U hfuse:w:0x$(strip $(HFUSE)):m
lock:
	$(DUDE) -U lock:w:0x$(strip $(LOCK)):m
burn: demo.hex
	$(DUDE) -e -u -U flash:w:demo.hex:i \
                  -U lfuse:w:0x$(strip $(LFUSE)):m -U hfuse:w:0x$(strip $(HFUSE)):m \
                  -U lock:w:0x$(strip $(LOCK)):m
