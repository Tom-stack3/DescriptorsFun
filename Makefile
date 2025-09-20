KDIR := $(HOME)/dev/kernel-dev/build/linux-x86-basic
PWD := $(shell pwd)

obj-m := dscfun.o

ccflags-y := -g -O0

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
