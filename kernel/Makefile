# Makefile for the NXP Simulated Temperature Sensor Kernel Module

# Default Kernel directory (standard Linux location)
# This can be overridden from the command line or build script (e.g., for WSL2)
KDIR ?= /lib/modules/$(shell uname -r)/build

# IMPORTANT WSL2 NOTE:
# If building on WSL2 where standard headers aren't installed via apt,
# KDIR needs to point to the manually cloned and prepared kernel source.
# The build.sh script handles overriding KDIR for WSL2.
# Example manual override for WSL2 (if not using build.sh):
# KDIR := /home/omarm/WSL2-Linux-Kernel

# The name of our module will be nxp_simtemp.ko
MODULE_NAME := nxp_simtemp
obj-m += $(MODULE_NAME).o

# List of object files that make up the module
$(MODULE_NAME)-objs := nxp_simtemp_main.o nxp_simtemp_miscdev.o nxp_simtemp_simulator.o nxp_simtemp_sysfs.o nxp_simtemp_locks.o

# Standard targets
all: modules

modules:
	@echo "Building kernel module in $(PWD) using kernel sources from $(KDIR)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "Cleaning kernel module objects in $(PWD)..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -f *.order *.symvers

