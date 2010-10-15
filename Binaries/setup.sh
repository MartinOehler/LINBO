#!/bin/sh
# This is a busybox 1.1.3 init script
# (C) Klaus Knopper 2007
# License: GPL V2

# Ignore signals
trap "" 1 2 11 15

CMDLINE=""
DEBUG=""

# Utilities

# findmodules dir
# Returns module names for modprobe
findmodules(){
 for m in `find "$@" -name \*.ko 2>/dev/null`; do
  m="${m##*/}"
  m="${m%%.ko}"
  echo "$m"
 done
}

# HW Detection
hwsetup(){
 UNAME="$(uname -r)"
 NETMODULES="$(findmodules /lib/modules/$UNAME/kernel/drivers/net)"
 HDDMODULES="$(findmodules /lib/modules/$UNAME/kernel/drivers/ide /lib/modules/$UNAME/kernel/drivers/ata /lib/modules/$UNAME/kernel/drivers/usb /lib/modules/$UNAME/kernel/drivers/scsi)"
 FSMODULES="$(findmodules /lib/modules/$UNAME/kernel/fs)"
 # Silence
 echo 0 >/proc/sys/kernel/printk
 for m in $NETMODULES $HDDMODULES $FSMODULES; do modprobe "$m"; done
 echo 6 >/proc/sys/kernel/printk
}

# Main

echo "Hardware-Erkennung..."
# FG process
hwsetup >/dev/null 2>&1

exit 0
