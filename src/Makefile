snd-soc-integrated-tasdevice-objs	:=	tasdevice-core.o \
							tasdevice-core-i2c.o \
							tasdevice-core-spi.o \
							tasdevice-misc.o \
							tas2781-irq.o \
							tas2563-irq.o \
							tasdevice-node.o \
							tasdevice-codec.o \
							tasdevice-rw.o  \
							tasdevice-regbin.o \
							tasdevice-dsp.o \
							tasdevice-ctl.o \
							tasdevice-dsp_kernel.o \
							tasdevice-dsp_git.o

obj-m					+= snd-soc-integrated-tasdevice.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install
