MODULE = ps3vram

obj-m = $(MODULE).o

KVER := $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build
MDIR := /lib/modules/$(KVER)/extra
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

install: $(MODULE).ko
	install -d $(MDIR)
	install -m 644 -c $(MODULE).ko $(MDIR)
	depmod -a

clean:
	rm -rf *.mod.c *.o *.ko .tmp_versions .$(obj-m:.o=)*
