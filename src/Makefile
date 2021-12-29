# To assist in cross-compiling
CC=gcc
LIBS="libudev libusb-1.0"
LDFLAGS=-pthread `pkg-config --libs $(LIBS)`
TOPDIR=$(shell pwd)

EEPROM_SUPPORT = y
MSTARDDC_SUPPORT = y

PKG_CONFIG ?= pkg-config

BIGFILES=-D_FILE_OFFSET_BITS=64
CFLAGS=-O2 -std=gnu99 -Wall -I./lusb_build/include $(BIGFILES) `$(PKG_CONFIG) --cflags $(LIBS)` -g

OBJS=flashcmd_api.o \
	spi_controller.o \
	spi_nand_flash.o \
	spi_nor_flash.o \
	ch341a_spi.o \
	timer.o \
	main.o

ifeq ($(EEPROM_SUPPORT),y)
CFLAGS += -DEEPROM_SUPPORT
OBJS += ch341a_i2c.o i2c_eeprom.o
OBJS += bitbang_microwire.o mw_eeprom.o ch341a_gpio.o
endif

ifeq ($(MSTARDDC_SUPPORT),y)
OBJS += mstarddc_spi.o
endif

all: SNANDer

SNANDer: $(OBJS)
	$(CC) $(CFLAGS) -s -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o SNANDer*
