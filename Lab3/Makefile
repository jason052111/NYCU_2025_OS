obj-m += kfetch_mod_314551147.o

KDIR ?= $(PWD)/linux-v6.8-devkit
ARCH ?= riscv
CROSS_COMPILE ?= riscv64-linux-gnu-

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean

