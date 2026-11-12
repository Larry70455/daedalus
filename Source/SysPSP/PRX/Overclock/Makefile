TARGET = overclock
OBJS = main.o

CFLAGS = -Os -G0 -Wall -fno-pic -mno-check-zero-division
ASFLAGS = $(CFLAGS)

PSP_FW_VERSION=500
BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

LDFLAGS = -nostartfiles
LIBS = -lpsppower_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

EXTRA_TARGETS = pspOverclock.S

pspOverclock.S : $(PRX_EXPORTS)
	psp-build-exports --build-stubs $(PRX_EXPORTS)

all: $(FINAL_TARGET) pspOverclock.S
