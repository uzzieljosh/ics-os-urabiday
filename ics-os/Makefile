# ics-os Makefile
#
# written by Joseph Anthony C. Hermocilla
#

vmdex:
	make -C kernel/

.PHONY: clean
clean:
	rm -f vmdex
	rm -f ics-os-livecd.iso
	rm -fr tmp/*
	make -C kernel/ clean

update_kernel_only: vmdex
	sudo rm -fr mnt
	sudo mkdir mnt
	sudo mount ics-os-floppy.img mnt -tmsdos -oloop
	sudo cp -r vmdex mnt
	sudo umount mnt
	sudo chmod 666 ics-os-floppy.img

prep_image: vmdex
	rm -fr tmp
	mkdir tmp
	cp -r vmdex tmp
	scripts/gen-help.sh
	cp  base/* tmp
	mkdir -p tmp/apps
	mkdir -p tmp/tcc1
	mkdir -p tmp/lib1
	cp apps/* tmp/apps/
	cp sdk/* tmp/tcc1/
	cp lib/* tmp/lib1/

floppy: prep_image
	cp grub.img ics-os-floppy.img                #copy an image with grub
	sudo rm -fr mnt
	sudo mkdir mnt
	sudo mount ics-os-floppy.img mnt -tmsdos -oloop
	sudo cp -r tmp/* mnt/
	sudo umount mnt
	sudo chmod 666 ics-os-floppy.img
	rm -fr tmp/

livecd: prep_image
	mkdir -p tmp/boot/grub
	cp boot/grub/grub.cfg tmp/boot/grub/.
	grub-mkrescue -o ics-os-livecd.iso tmp/
	rm -fr tmp/

boot-floppy:
	qemu-system-i386 -net nic,model=rtl8139 -soundhw pcspk -fda ics-os-floppy.img -boot a -m 64M 

boot-floppy-gdb:
	qemu-system-i386 -net nic,model=rtl8139 -soundhw pcspk -fda ics-os-floppy.img -boot a -m 64M -s -S

boot-floppy-vnc:
	qemu-system-i386 -net nic,model=rtl8139 -soundhw pcspk -fda ics-os-floppy.img -boot a -m 64M -vnc :1 

boot-debug:
	cd kernel ;	qemu-system-i386 -s -S -kernel Kernel32.bin -fda ../ics-os-floppy.img -m 64M -append "root (hdp0p0)" 


boot-livecd:
	qemu-system-i386 -cdrom ics-os-livecd.iso -boot cd -m 64M
