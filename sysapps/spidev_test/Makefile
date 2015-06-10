#
# 'make depend' uses makedepend to automatically generate dependencies
#
# 'make'		build executable file
#
# 'make clean'	removes all .o and executable files
#

VERSION_STR="\"0.1\""

# default the C compiler to use
#
CROSS_COMPILE ?= arm-uclinuxeabi-
ARCH = arm

# yes, quite ugly, i know, copy/past from busybox build process
CFLAGS = -march=armv7-m -mtune=cortex-m4 -mlittle-endian -mthumb -Os -ffast-math 
CFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections -fno-common 
CFLAGS += --param max-inline-insns-single=1000 -Wl,-elf2flt=-s -Wl,-elf2flt=16384 
CFLAGS += -Wl,-elf2flt -static -I../staging/include/

LDFLAGS = -march=armv7-m -mtune=cortex-m4 -mlittle-endian -mthumb -Os -ffast-math 
LDFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections -fno-common 
LDFLAGS += --param max-inline-insns-single=1000 -Wl,-elf2flt=-s -Wl,-elf2flt=16384 -Wl,-elf2flt -static

AS              = $(CROSS_COMPILE)as
CC              = $(CROSS_COMPILE)gcc
LD              = $(CC)
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump


# define any  ../../../../default/linux-2.6.30/include/
INCLUDES= -I../../uclinux/ 

# define library paths in addition to /usr/lib
# 	if u wanted to include libraries not in /usr/lib I'd specify
# 	their path using -Lpath, something like:
LFLAGS=

# define any libraries to linlk into executable
#
LIBS = 


# define the C object files
SRCS += spidev_test.c

# define the C object files 
OBJS = $(SRCS:.c=.o)

# define the executable file
MAIN = spidev_test

.PHONY : depend clean

all:		$(MAIN)
			@echo compiled...

$(MAIN):	$(OBJS)	
			$(CC) $(LDFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
			$(CC)  $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
			$(RM) *.o *~ $(MAIN)

depend:		$(SRCS)
			makedepend $(INCLUDES) $^

install : $(MAIN)
	cp $(MAIN) ../../rootfs/usr/bin/
