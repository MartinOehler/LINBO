# Boot commandline for testing!
# debug            -  run a shell prior to linbo_gui
# nonetwork        -  run locally with builtin start.conf
# loglevel=4       -  Linux level of verbosity in boot messages
# CMDLINE  = server=10.0.2.2 config=start.conf-test ip=10.0.2.15 ipconf=10.0.2.15 loglevel=4 test=\"this is a test\" nmi_watchdog=0 debug highres=off noapic nolapic acpi=off pci=bios
CMDLINE  = server=10.0.2.2 config=start.conf-test ip=10.0.2.15 nosmp ipconf=10.0.2.15 quiet loglevel=0 test=\"this is a test\" nmi_watchdog=0 apm=power-off
# CMDLINE  = server=10.0.2.2 config=start.conf ip=10.0.2.15 ipconf=10.0.2.15 loglevel=0 test=\"this is a test\" nmi_watchdog=0 noapic nolapic apm=power-off

# VESAMODE = 771 # 800x600, 256 colors
VESAMODE = 785 # 640x480, 64k colors
#  VESAMODE = 0 # VGA16
# VESAMODE = 792 # 1024x768, 64k colors # Currently not working

KERNEL   = $(shell ls -1d Kernel/linux-*/. | tail -1)
CLOOP    = $(shell ls -1d Kernel/cloop-*/. | tail -1)
KVERS    = $(shell awk '{if($$1~/^VERSION$$/){version=$$NF}if($$1~/^PATCHLEVEL$$/){patchlevel=$$NF}if($$1~/^SUBLEVEL$$/){sublevel=$$NF}if($$1~/^EXTRAVERSION$$/){if($$NF != "="){extraversion=$$NF}}}END{print version"."patchlevel"."sublevel extraversion}' $(KERNEL)/Makefile)

help:
	@echo "[1mWELCOME TO THE LINBO BUILD SYSTEM (Kernel version: $(KVERS))"
	@echo ""
	@echo "make linbo	(Re-)Build both LINBO-Kernel and LINBO-FS"
	@echo "make kernel	(Re-)Build Kernel and Modules (recommended before \"make linbo\")"
	@echo "make linbofs	(Re-)Build only LINBO-FS"
	@echo "make config	Configure LINBO kernel and edit LINBO filesystem."
	@echo "make clean	Cleanup LINBO kernel source for recompilation."
	@echo "make debian	Create debian package."
	@echo "make linbo-cd	Create bootable ISO image for non-PXE boot."
	@echo "make test	Run LINBO kernel in kvm"
	@echo "make cdtest	Run LINBO from linbo-cd.iso (see make linbo-cd)"
	@echo "make hdtest	Run LINBO from a harddisk-installed LINBO session"
	@echo "make pxetest	Run LINBO in a kvm simulated PXE network boot"
	@echo "            	(kvm 0.9.0+cvs version required that supports '-bootp')"
	@echo
	@echo "Don't worry about the sequence of build commands, this Makefile will tell you"
	@echo "what to do first, in case anything is missing."
	@echo
	@echo "Have a lot of fun. ;-)"
	@echo "[0m"

linbofs Images/linbofs: $(KERNEL) Binaries/Patch_Registry/patch_registry.sh Binaries/linbo_gui/linbo_gui
	@echo "[1mRebuilding LINBOFS...[0m"
	cd $(KERNEL) ; \
	ln -snf ../initramfs.conf . ; \
	ln -snf ../../Binaries initramfs ; \
	:> ../initramfs.conf ; \
	( cd initramfs/modules && find . -type d -printf "dir %p %m 0 0\n" && find . -type f \( -path \*/ide/\* -o -path \*/ata/\* -o -path \*/net/\* -o -path \*/usb/\* -o -path \*/fs/\* -o -name cloop.ko -o -name modules.\* \) -printf "file %p initramfs/modules/%p %m 0 0\n" ) | sed -e 's|\./|/|g' >../initramfs.d/modules.conf ; \
	cat ../initramfs.d/* >>../initramfs.conf ; \
	rm -f initramfs.gz ; ./usr/gen_init_cpio ../initramfs.conf | gzip -9cv > initramfs.gz
	@echo "[1mLinking LINBOFS to Images/linbofs.gz[0m" ; \
	ln -nf $(KERNEL)/initramfs.gz Images/linbofs.gz
	echo -e "[LINBOFS]\ntimestamp=`date +%Y\%m\%d\%H\%M`\nimagesize=`ls -l Images/linbofs.gz | awk '{print $$5}'`" >Images/linbofs.gz.info

linbo Images/linbo:
	@echo "[1mRebuilding LINBO...[0m" ; \
	$(MAKE) kernel
	$(MAKE) linbofs
	@echo "[1mLINBO done. You may run \"make test\" now.[0m"

cloop: $(KERNEL)
	@echo "[1mBuilding cloop module...[0m" ; \
	rm -f Binaries/modules/lib/modules/*/kernel/drivers/block/cloop.ko
	cd $(CLOOP) && \
	( make ARCH=i386 KERNEL_DIR=`pwd`/../../$(KERNEL) clean ; make ARCH=i386 KERNEL_DIR=`pwd`/../../$(KERNEL) cloop.ko ) && \
	mkdir -p `pwd`/../../$(KERNEL)/initramfs/modules/lib/modules/$(KVERS)/kernel/drivers/block && \
	install cloop.ko `pwd`/../../$(KERNEL)/initramfs/modules/lib/modules/$(KVERS)/kernel/drivers/block/

kernel: $(KERNEL)
	@echo "[1mBuilding LINBO kernel...[0m" ; \
	rm -rf Binaries/modules/* || true
	test -r $(KERNEL)/.config || cp Kernel/kernel.conf $(KERNEL)/.config
	cd $(KERNEL) ; \
	ln -snf ../initramfs_kernel.conf . ; \
	ln -snf ../../Binaries initramfs ; \
	:> ../initramfs_kernel.conf ; \
	cat ../initramfs_kernel.d/* >>../initramfs_kernel.conf; \
	rm -f usr/initramfs_data.cpio.gz ; \
	make -j 3 ARCH=i386 bzImage modules ; \
	mkdir -p initramfs/modules ; make ARCH=i386 INSTALL_MOD_PATH=initramfs/modules modules_install
	@echo "[1mLinking LINBO kernel to Images/linbo.[0m" ; \
	rm -f Images/linbo; cp $(KERNEL)/arch/i386/boot/bzImage Images/linbo ; \
	/usr/sbin/rdev -v Images/linbo $(VESAMODE) ; \
	echo -e "[LINBO]\ntimestamp=`date +%Y\%m\%d\%H\%M`\nimagesize=`ls -l Images/linbo | awk '{print $$5}'`" >Images/linbo.info
	$(MAKE) cloop
	-/sbin/depmod -a -b Binaries/modules $(KVERS)

Binaries/Patch_Registry/patch_registry.sh: Patch_Registry/patch_registry.sh
	install -m 755 $< $@

Binaries/linbo_gui/linbo_gui: GUI2/linbo_gui
	ln -nf $< $@

# Don't check this dependency, rather compile manually
#GUI2/linbo_gui: GUI2/qt-embedded-linux-opensource-src-4.5.2/lib/libQtGui.a
#	cd GUI2 && ./build_gui

GUI2/qt-embedded-linux-opensource-src-4.5.2/lib/libQtGui.a:
	cd GUI2 && ./build_qt

qemu:
	cd Qemu/qemu && CC=gcc-3.4 fakeroot dpkg-buildpackage -us -uc 
#	cd Qemu/qemu && fakeroot dpkg-buildpackage -us -uc 

config: $(KERNEL)
	@echo "[1mConfiguring LINBO kernel...[0m" ; \
	cd $(KERNEL) ; make ARCH=i386 menuconfig
	@echo "[1mLINBO configuration complete. You may now run \"make linbo\".[0m"
	
clean: $(KERNEL)
	@echo "[1mCleaning up LINBO kernel sources...[0m" ; \
	cd $(KERNEL) ; \
	make ARCH=i386 clean

debian: Images/linbo Images/linbofs.gz
	cd Debian && ./build.sh

linbo-cd linbo-cd.iso: cd/boot/isolinux/isolinux.cfg Images/linbo Images/linbofs.gz
	install Images/linbo Images/linbofs.gz cd/boot/isolinux/
	mkisofs -r -no-emul-boot -boot-load-size 4 -boot-info-table \
	  -b boot/isolinux/isolinux.bin -c boot/isolinux/boot.cat \
	  -m .svn -input-charset ISO-8859-1 -o linbo-cd.iso cd
	@rm -f cd/boot/isolinux/linbo* cd/boot/isolinux/boot.cat

cdtest: linbo-cd.iso kvm-modules
	@echo "[1mStarting LINBO-CD in kvm...[0m"
	kvm -m 256 -monitor stdio -hda Images/hda.img -boot d -cdrom linbo-cd.iso 

Images/hda.img:
	qemu-img create -f qcow2 $@ 40G

test: Images/linbo Images/hda.img kvm-modules
	@echo "[1mStarting LINBO in kvm...[0m"
	kvm -m 256 -monitor stdio -hda Images/hda.img -kernel Images/linbo -initrd Images/linbofs.gz -append "$(CMDLINE)"

testlocal: Images/linbo Images/hda.img kvm-modules
	@echo "[1mStarting LINBO in kvm in \"nonetwork\" mode...[0m"
	kvm -m 256 -monitor stdio -hda Images/hda.img -kernel Images/linbo -initrd Images/linbofs.gz -append "$(CMDLINE) nonetwork"

pxetest: Images/linbo Images/hda.img kvm-modules
	@echo "[1mStarting LINBO in kvm via PXE...[0m"
	kvm -m 256 -monitor stdio -boot n -hda Images/hda.img -bootp /pxelinux.0 -tftp Images

hdtest: Images/linbo Images/hda.img kvm-modules
	@echo "[1mStarting LINBO from a previous harddisk installation...[0m"
	kvm -m 256 -monitor stdio -boot c -hda Images/hda.img -tftp Images -append "$(CMDLINE)"

kvm-modules:
	[ -d /sys/modules/kvm_intel ] || [ -d /sys/modules/kvm_amd ] || sudo modprobe kvm_intel || sudo modprobe kvm_amd
