KVERSION = $(shell uname -r)
BLDDIR= /lib/modules/$(KVERSION)/build

MODULE_NAME=areg

ccflags-y := -mhard-float -msse

obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-objs := ar.o ar_debugfs.o ar_perfs.o model.o master.o

all: 
	make -C $(BLDDIR) M=$(PWD) modules

clean:
	make -C $(BLDDIR) M=$(PWD) clean
