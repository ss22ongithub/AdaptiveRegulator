KVERSION = $(shell uname -r)
BLDDIR= /lib/modules/$(KVERSION)/build

MODULE_NAME=areg

# Enable FPU
ccflags-y := -mhard-float -msse

# Enable AR_DEBUG logging
ccflags-y += -DCONFIG_DEBUG_AR

obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-objs := ar.o ar_debugfs.o ar_perfs.o model.o master.o utils.o

all: 
	make -C $(BLDDIR) M=$(PWD) modules

clean:
	make -C $(BLDDIR) M=$(PWD) clean
