obj-m += task_dev_m.o
task_dev_m-objs := task_dev.o ringbuffer.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
