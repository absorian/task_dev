# target all by default
make

# install kernel module
sudo insmod $MODNAME.ko

# remove kernel module
sudo rmmod $MODNAME

# see devices and their major nr
cat /proc/devices

# see kernel log
sudo dmesg

# create device file with according major and minor nrs
# no need if module makes it dynamically
sudo mknod /dev/$DEVNAME $DEVTYPE $MAJOR $MINOR

