# 适用于Linux
qemu-system-i386 -daemonize -m 128M -s -S -netdev tap,id=mynet0,ifname=tap -device rtl8139,netdev=mynet0,mac=52:54:00:c9:18:27 -netdev user,id=mynet1 -device rtl8139,netdev=mynet1,mac=52:54:00:c9:18:33 -drive file=disk1.img,index=0,media=disk,format=raw -drive file=disk2.img,index=1,media=disk,format=raw
