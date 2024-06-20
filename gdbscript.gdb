set disassembly-flavor intel
target remote | qemu-system-i386 -drive index=0,if=floppy,format=raw,file=build/main_floppy.img -boot order=a -gdb stdio