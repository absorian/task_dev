obj-m += task_dev.o
task_dev-objs := task_dev_module.o ringbuffer.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load:
	sudo insmod $(basename $(obj-m)).ko rb-size=1024
unload:
	sudo rmmod $(basename $(obj-m))
