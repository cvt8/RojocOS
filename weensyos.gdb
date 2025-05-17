set $loaded = 1
set arch i386:x86-64
file obj/kernel.full
add-symbol-file obj/bootsector.full 0x7c00
target remote localhost:1234
source build/functions.gdb
