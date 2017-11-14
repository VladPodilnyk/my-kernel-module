# Simple kernel module

This porjects is an example of creation a simple 
kernel module. For this project I used kernel version 4.8.

## Getting Started

### Prerequisites

Create working directory and download project
```
$ mkdir <your-working-dir>
$ cd <your-working-dir>
$ git clone https://github.com/VladPodilnyk/my-kernel-module.git
```

Also, you need QEMU and cross-toolchain for ARM.
```
$ sudo apt-get install qemu-system-arm
$ sudo apt-get install gcc-arm-linux-gnueabi
```

Try to build Linux Kernel.
```
$ cd my-kernel-module
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- multi_v7_defconfig
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- zImage -j4
$ cd <your-working-dir>
$ cp my-kernel-module/arch/arm/boot/zImage .
```
You need your own initial RAM disk. You can get on the Internet 
or build it by yourself.
Unpack initrd and prepare it for modifictations.
```
# mkdir rootfs
# cd rootfs
# cpio -idv < ../rootfs.cpio
```

Check your files.
```
$ tree -L 1
.
├── my-kernel-module
├── rootfs
├── rootfs.cpio
└── zImage
```

Run QEMU
```
$ qemu-system-arm \
        -machine virt \
        -kernel zImage \
        -initrd rootfs.cpio \
        -nographic \
        -m 512 \
        --append " \
            root=/dev/ram0 \
            rw \
            console=ttyAMA0,38400 \
            console=ttyS0 \
            mem=512M \
            loglevel=9"
```

## Configure module to use 'simple device' module

Reconfigure source tree to build own driver.
```
$ cd my-kernel-module
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- multi_v7_defconfig
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- menuconfig
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- drivers/misc/simple_dev.ko
```

Add module into ramfs
```
# cd rootfs
# cp ../my-kernel-module/drivers/misc/simple_dev.ko lib/simple_dev.ko
# find . | cpio -o -H newc > ../rootfs.cpio
```

## Playing with 'simple device' module

Run QEMU and use module
```
$ qemu-system-arm \
        -machine virt \
        -kernel zImage \
        -initrd rootfs.cpio \
        -nographic \
        -m 512 \
        --append " \
            root=/dev/ram0 \
            rw \
            console=ttyAMA0,38400 \
            console=ttyS0 \
            mem=512M \
            loglevel=9"
```

After entering use next command to insert and remove module
```
# insmod /lib/simple_dev.ko
[   54.146232] Making your life simplier...
# rmmod /lib/simple_dev.ko
[ 1945.876064] Congrats!!! Your life is simple.
```

In order to see communication between user space and kernel space,
you should mount Debug filesystem
```
# mount -t debugfs none /sys/kernel/debug/
```

Read how many interrupts fired.
```
# cat /sys/kernel/debug/simple-dev/uart-pl011-count
54
```
You can write own values to uart-pl011-count
```
# echo 2 > /sys/kernel/debug/simple-dev/uart-pl011-count
# cat /sys/kernel/debug/simple-dev/uart-pl011-count
5
```
Read how many interrupts fired in particular time.
```
# cat /sys/kernel/debug/simple-dev/uart-pl011-timestamp
115 55-42-569
```
Use 'stop' command to stop recording timestamps.
```
# echo stop > /sys/kernel/debug/simple-dev/uart-pl011-timestamp
[  521.431295] Stop recording.
```
Use 'clear' command to clean history.
```
#  echo clear > /sys/kernel/debug/simple-dev/uart-pl011-timestamp
[  655.140475] Clear records.
# cat /sys/kernel/debug/simple-dev/uart-pl011-timestamp
0 00-00-000
```
Use 'start' command to begin recording.
```
# echo start > /sys/kernel/debug/simple-dev/uart-pl011-timestamp
# cat /sys/kernel/debug/simple-dev/uart-pl011-timestamp
150 06-19-207
```

