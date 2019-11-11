# Makefile for the ctrl-zoom firmware.
#
# Usage: set the variables MCU and PROGRAMMER below, then type
#
#     make
#
# to compile the program. Next, connect an ISP programmer to both your
# computer and the ISP header, and type
#
#     make upload
#
# to upload the compiled firmware into the microcontroller.

########################################################################
# Configurable settings.

# Set this to the microcontroller you will be using: either attiny25,
# attiny45 or attiny85. Parts with the V suffix can be used, but the
# suffix should be omitted here.
MCU        = attiny25

# Programmer used for uploading the firmware through ISP. Should be
# known to avrdude. Some programmers need a port to be specified.
# Syntax: either "-c programmer-id" or "-c programmer-id -P port"
# (without the quotes).
PROGRAMMER = -c usbasp

# No changes should be needed past this line.
########################################################################

CFLAGS     = -mmcu=$(MCU) -Os -Wall -Wextra

ctrl-zoom.elf: ctrl-zoom.c
	avr-gcc $(CFLAGS) $< -o $@

upload: ctrl-zoom.elf
	avrdude -p $(MCU) $(PROGRAMMER) -U $<

clean:
	rm ctrl-zoom.elf

.PHONY: clean
