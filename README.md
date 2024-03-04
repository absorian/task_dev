# Task device
Using a ringbuffer, this character device can transfer data to/from processes.
The name implied from the fact that this is a job entry test task received to the author.

### Features
- Blocking/Nonblocking mode switch using ioctl
- Last read/write access information using ioctl
- Setting ringbuffer size using kernel module parameters

## Usage
Requires `make`, linux 2.6+ (I think), and linux kernel headers.
Note: you can change default ringbuffer size in Makefile (`rb-size`).
```
  make
  # loads task_dev into kernel
  make load
  # to remove from kernel
  make unload
```
```
  echo "hello" > /dev/task_dev
  cat /dev/task_dev
```
## ioctl
Include `task_dev.h` to access ioctl macros.
```
  // Change nonblock state 0/1
  int on = 0;
  if (ioctl(fd, TDEV_IOC_NONBLOCK, &on) < 0) { /* err */ }
```
```
  // Get device info
  struct tdev_ioc_info info;
  if (ioctl(fd, TDEV_IOC_GETINFO, &info) < 0) { /* err */ }
```
Note: `tdev_ioc_info.timestamp` is counting nanosecs in UTC time.
