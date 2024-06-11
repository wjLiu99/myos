# 适用于Linux
qemu-system-i386  -daemonize -m 128M   -drive file=/root/myos/image/disk1.img,index=0,media=disk,format=raw -drive file=/root/myos/image/disk2.img,index=1,media=disk,format=raw -S -s  