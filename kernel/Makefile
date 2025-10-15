# !! IMPORTANTE !!
# Reemplaza la siguiente ruta con la ruta absoluta donde clonaste el kernel de WSL2
KDIR := /home/omarm/WSL2-Linux-Kernel
#KDIR ?= /lib/modules/$(shell uname -r)/build
# El nombre de nuestro módulo será nxp_simtemp.ko
obj-m += nxp_simtemp.o
nxp_simtemp-objs := nxp_simtemp_main.o nxp_simtemp_miscdev.o nxp_simtemp_simulator.o nxp_simtemp_sysfs.o

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean