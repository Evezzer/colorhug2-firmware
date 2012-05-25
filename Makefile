#CC = sdcc-sdcc can't be used as we're using sdcc from svn trunk
CC = /home/hughsie/Code/sdcc/bin/sdcc
INHX2BIN = /usr/libexec/colorhug-inhx32-to-bin

# device options
DEVOPTS =			\
	-mpic16			\
	-p18f46j50		\
	--pstack-model=small

# allow us to use the Microchip-derived headers
DEVOPTS += --use-non-free

CFLAGS = $(DEVOPTS)
LIBS = $(DEVOPTS)

# use Intel hex format
LIBS += --out-fmt-ihx

# put the interrupt vector table at specific address
#LIBS += --ivt-loc=0x8000

# put the code at specific address
#LIBS += --code-loc=0x8000

# use delay()
LIBS += -llibc18f.lib

all: Makefile firmware.bin bootloader.hex

# common stuff
ch-common.o: Makefile ch-common.c
	$(CC) $(CFLAGS) -c ch-common.c
ch-math.o: Makefile ch-math.c
	$(CC) $(CFLAGS) -c ch-math.c

# bootloader
bootloader.o: Makefile firmware.c
	$(CC) $(CFLAGS) -o bootloader.o -DCOLORHUG_BOOTLOADER -c firmware.c
bootloader.hex: Makefile bootloader.o ch-common.o
	$(CC) -o bootloader.hex $(LIBS) bootloader.o ch-common.o

# firmware
firmware.o: Makefile firmware.c
	$(CC) $(CFLAGS) -o firmware.o -c firmware.c
firmware.hex: Makefile firmware.o ch-common.o ch-math.o
	$(CC) -o firmware.hex $(LIBS) firmware.o ch-common.o ch-math.o

# pad this out to be easy to distribute
firmware.bin: Makefile firmware.hex
	$(INHX2BIN) firmware.hex firmware.bin

clean:
	rm -f	\
	*.adb	\
	*.asm	\
	*.cdb	\
	*.hex	\
	*.ihx	\
	*.lk	\
	*.lst	\
	*.map	\
	*.mem	\
	*.o	\
	*.omf	\
	*.rel	\
	*.rst	\
	*.sym
