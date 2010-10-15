#!/bin/bash

TMP="linbo-debian.$$"

PXEGRUB="../Pxegrub/grub-0.97_ntfs_findloop/stage2/pxegrub"

trap bailout 1 2 10 15

clean(){
	echo "Cleaning up..."
	rm -rf "$TMP"
	fakeroot debian/rules clean
	rm -f debian/install debian/dirs
}

bailout(){
 clean
 exit $1
}


# clean
clean

# fetch version from changelog
version=`head -n 1 debian/changelog | awk -F\( '{ print $2 }' | awk -F\) '{ print $1 }'`
[ -z "$version" ] && { echo "Cannot determine LINBO Version!"; bailout 1; }

# Install dirs and progs into $TMP

# create necessary dirs
for dir in /var/linbo /var/linbo/examples /var/linbo/pxelinux.cfg /etc/default /etc/init.d /etc/linuxmuster/linbo /usr/sbin /var/log/linuxmuster /usr/share/doc/linuxmuster-linbo /usr/share/linuxmuster-linbo /usr/share/linuxmuster-linbo/cd/isolinux; do
 srcdir="$TMP$dir"
 mkdir -m 755 -p "$srcdir"
 echo "$dir" >> debian/dirs 
 echo "${srcdir}/* $dir" >> debian/install 
done

chmod 1777 "$TMP/var/log/linuxmuster"

# check for linbo kernel and initrd
for file in ../Images/{linbo,linbo.info,linbofs.gz,linbofs.gz.info}; do
    [ -f "$file" ] || { echo "$file does not exist yet, please build LINBO first."; bailout 1; }
    cp "$file" "$TMP/var/linbo/"
done

# example start.conf files
install -m 644 debian/examples/* "$TMP/var/linbo/examples/"

# doc
cp ../Documentation/LINBO.pdf "$TMP/usr/share/doc/linuxmuster-linbo/"

# pxe boot stuff
sed -e "s/@@version@@/$version/
	s/@@seconds@@/3/" debian/boot.msg.template > "$TMP/var/linbo/boot.msg"
install -m 644 ../Binaries/german.kbd "$TMP/var/linbo/"
install -m 644 $PXEGRUB "$TMP/var/linbo/"
install -m 644 ../Graphics/linbo-pxe-bootscreen.16 "$TMP/var/linbo/logo.16"

# default config files
install -m 644 debian/start.conf "$TMP/etc/linuxmuster/linbo/start.conf.default"
ln -s /etc/linuxmuster/linbo/start.conf.default "$TMP/var/linbo/start.conf"
install -m 644 ../Images/pxelinux.cfg/default "$TMP/etc/linuxmuster/linbo/pxelinux.cfg.default"
ln -s /etc/linuxmuster/linbo/pxelinux.cfg.default "$TMP/var/linbo/pxelinux.cfg/default"
install -m 644 debian/pxegrub.lst "$TMP/etc/linuxmuster/linbo/pxegrub.lst.default"
ln -s /etc/linuxmuster/linbo/pxegrub.lst.default "$TMP/var/linbo/pxegrub.lst"

# debian init stuff
install -m 644 ../Binaries/Etc/default/* "$TMP/etc/default/"
install -m 755 ../Binaries/Etc/init.d/* "$TMP/etc/init.d/"
install -m 755 ../Binaries/linbo-multicast-server.sh "$TMP/usr/sbin/linbo-multicast-server"

# cd boot stuff
sed -e "s/@@version@@/$version/
	s/@@seconds@@/30/" debian/boot.msg.template > "$TMP/usr/share/linuxmuster-linbo/cd/isolinux/boot.msg"
install -m 644 ../cd/boot/isolinux/german.kbd "$TMP/usr/share/linuxmuster-linbo/cd/isolinux/"
install -m 644 ../cd/boot/isolinux/isolinux.cfg "$TMP/usr/share/linuxmuster-linbo/cd/isolinux/"
install -m 644 ../Graphics/linbo-cd-bootscreen.16 "$TMP/usr/share/linuxmuster-linbo/cd/isolinux/logo.16"

# linbo scripts
install -m 755 debian/rsync-p*.sh "$TMP/usr/share/linuxmuster-linbo/"
install -m 755 debian/make-linbo-iso.sh "$TMP/usr/share/linuxmuster-linbo/"

# rewrite Debian files
# (
#    DEBFILES="control dirs install postinst"
#    cd debmod/debian
#    for d in $DEBFILES; do
#	sed "s|#KVERS#|$KVERS|g" $d.in > $d
#    done
#)

# create debian package
fakeroot debian/rules binary

# clean
bailout 0
