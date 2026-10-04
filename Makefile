# drivers
obj-m += foo_device.o
obj-m += foo_device_sim.o
# devices
obj-m += foo_driver.o
obj-m += foo_irq_driver.o
obj-m += foo_work_driver.o
obj-m += foo_irq_thread_driver.o
obj-m += foo_completion_driver.o
obj-m += foo_wq_driver.o
obj-m += foo_irq_mutex_driver.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	
