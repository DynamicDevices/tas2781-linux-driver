snd-soc-tasdevice-objs	:=	tasdevice-core.o \
							tasdevice-core-i2c.o \
							tasdevice-core-spi.o \
							tasdevice-misc.o \
							tas2781-irq.o \
							tasdevice-node.o \
							tasdevice-codec.o \
							tasdevice-rw.o  \
							tasdevice-regbin.o \
							tasdevice-dsp.o \
							tasdevice-ctl.o \
							tasdevice-dsp_kernel.o \
							tasdevice-dsp_git.o

obj-$(CONFIG_SND_SOC_TASDEVICE)		+= snd-soc-tasdevice.o
