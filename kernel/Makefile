# !! IMPORTANTE !!
# Reemplaza la siguiente ruta con la ruta absoluta donde clonaste el kernel de WSL2
KDIR := /home/omarm/WSL2-Linux-Kernel

# El nombre de nuestro módulo será nxp_simtemp.ko
obj-m += nxp_simtemp.o

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean