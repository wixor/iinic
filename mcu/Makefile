MCU	      := atmega16
DUDEMCU   := m16
FLASHSIZE := 16384
RAMSIZE   := 1024
LFUSE     ?= E1 # BODLEVEL=1 BODEN=1 SUT=10 CKSEL=0001 (1MHz internal RC, 65ms boot, brown-out detector disabled)
HFUSE     ?= DF # OCDEN=1 JTAGEN=1 SPIEN=0 CKOPT=1 EESAVE=1 BOOTSZ=11 BOOTRST=1 (OCD, JTAG disabled, SPI enabled)
LOCK      ?= 00

CC	  	  := avr-gcc
LD	  	  := avr-ld
NM		  := avr-nm
SIZE	  := avr-size
OBJCOPY   := avr-objcopy
DUDE      := avrdude -p $(DUDEMCU) -c stk500v2 -P /dev/ttyS0 -q -q

CFLAGS	  := -DF_CPU=14745600 -Os -std=gnu99 -pipe -mmcu=$(MCU) -Wall -Wextra -Wshadow

.PHONY: build clean erase flash fuses lock burn

build: iinic.hex iinic.bin
	@ $(SIZE) iinic | cut -f1,2,3 | ( \
	read foo; read text data bss; \
	code=$$(($$text+$$data)); \
	codepercent=$$((100*$$code/$(FLASHSIZE))); \
	mem=$$(($$data+$$bss)); \
	mempercent=$$((100*$$mem/$(RAMSIZE))); \
	echo "code: $$code bytes ($$codepercent%); ram: $$mem bytes ($$mempercent%)" );

iinic.hex: iinic
	$(OBJCOPY) -O ihex -j .text -j .data $^ $@
iinic.bin: iinic
	$(OBJCOPY) -O binary -j .text -j .data $^ $@

iinic: iinic.c

clean:
	$(RM) iinic iinic.bin iinic.hex

erase:
	$(DUDE) -e
flash: iinic.hex
	$(DUDE) -e -U flash:w:iinic.hex:i
fuses:
	$(DUDE) -u -U lfuse:w:0x$(strip $(LFUSE)):m -U hfuse:w:0x$(strip $(HFUSE)):m
lock:
	$(DUDE) -U lock:w:0x$(strip $(LOCK)):m
burn: iinic.hex
	$(DUDE) -e -u -U flash:w:iinic.hex:i \
                  -U lfuse:w:0x$(strip $(LFUSE)):m -U hfuse:w:0x$(strip $(HFUSE)):m \
                  -U lock:w:0x$(strip $(LOCK)):m
