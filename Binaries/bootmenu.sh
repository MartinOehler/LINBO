#!/bin/sh
# This is a busybox 1.1.3 bootmenu script
# (C) Klaus Knopper 2006
# License: GPL V2

PHASE="1"

echo "Displaying bootmenu."

DIALOG="Xdialog --wrap --cr-wrap --left --rc-file /etc/xdialog.rc --icon /usr/lib/linbo.xpm --stdout"
DIALOG_IFS="Xdialog|--wrap|--cr-wrap|--left|--rc-file|/etc/xdialog.rc|--icon|/usr/lib/linbo.xpm|--stdout"

export XDIALOG_NO_GMSGS=1
export GDK_USE_XFT=1
export DISPLAY=":0"

MENU='LINBO Boot Menü'

HELP='Willkommen zu LINBO, dem GNU/Linux-basierten Netzwerk Boot System.

In diesem Dialog können Sie von einer der in der Auswahl gezeigten Festplatteninstallationen booten.

Wählen Sie bitte eine Partition aus und klicken Sie auf "OK", um mit dem - ggf. durch frühere Sessions modifizierten - System auf dieser Partition weiterzuarbeiten.

Mit der Option "Reparieren" setzen Sie das installierte System in den Ursprungszustand zurück, wofür der lokale Cache oder der Bootserver als Master verwendet wird.
'

BOOT='OK'

CANCEL='Abbruch'

REINSTALL='Partition oder System aus dem Cache oder vom Server reparieren'

TMP="/tmp/bootmenu.$$.tmp"
BOOTLIST="/tmp/bootmenu.bootlist.$$.tmp"
rm -f "$TMP" "$BOOTLIST"

# TODO: Check if X is running

[ ! -e /proc/partitions ] && { echo "$0: /proc not mounted, exiting" >&2; exit 1; }

PROGRESSDONE="/tmp/progress.$$.done"
rm -f "$PROGRESSDONE"

# progress "text" expected_time (seconds)
# or
# progress (without params, to kill gauge)
progress(){
if [ -n "$1" ]; then
 # Show progressbar
 rm -f "$PROGRESSDONE"
 expected="$2"
 [ -n "$expected" ] && ratio="$((expected/10))+2" || ratio=4
 status=0
 ( while [ ! -e "$PROGRESSDONE" ]; do echo "$(((status=(10000-status)/ratio+status)/100))"; sleep 1; done | $DIALOG --gauge "$1" 8 75 0 ) &
else
 # Kill previous
 touch "$PROGRESSDONE" ; wait ; rm -f "$PROGRESSDONE"
fi
}

# Returns partition size in MB
getsize(){
while read major minor s p relax; do
 if [ "$p" = "$1" -o "$p" = "/dev/$1" ]; then
  echo "$(($s / 1024))" 2>/dev/null
  break
 fi
done </proc/partitions
}

notyet(){
 $DIALOG --msgbox "Sorry, diese Funktion ${1:+($1)} ist in Phase $PHASE noch nicht implementiert." 0 0
}

bootfrom(){
 while read p k i relax; do
  [ "$p" = "$1" ] && break
 done < "$BOOTLIST"

 $DIALOG --yesno "Boote Kernel ${k}${i:+, mit initrd=$i} von $p (dieser Dialog erscheint nur in der Debug-Version von LINBO)." 0 0 || return 0

 if mount -r "$p" /mnt; then
  LOADED=""
  if [ -r /mnt/"$k" ]; then
   KERNEL="/mnt/$k"
   [ -n "$i" -a -r /mnt/"$i" ] && INITRD="--initrd=/mnt/$i"
   kexec -l $INITRD --append="root=$p" $KERNEL && LOADED="true"
  fi
  umount /mnt
 fi
 if [ -n "$LOADED" ]; then
  # We basically do a quick shutdown here.
  echo -n "c" >/dev/console
  killall5 -15
  sleep 2
  exec kexec -e
  sleep 10
 fi
}

# Creates BOOTLIST
scanpartitions(){
 rm -f "$BOOTLIST"

 partitions=""
 disks=""
 disksize=0
 blocksum=0
 pold="none"
 foundboot=0
 
 while read major minor blocks partition relax; do
  partition="${partition#/dev/}"
  [ -z "$partition" -o ! -e "/dev/$partition" ] && continue
  [ "$blocks" -lt 2 ] 2>/dev/null && continue
  case "$partition" in
   ram*|cloop*|loop*) continue;; # Kernel 2.6 bug?
   ?d?|ataraid/d?) disks="$disks /dev/$partition"; disksize="$blocks"; blocksum=0;;
   *) blocksum="$(($blocksum + $blocks))"; [ "$blocksum" -gt "$disksize" ] >/dev/null 2>&1 || partitions="$partitions /dev/$partition";;
  esac
 done </proc/partitions
 
 # Add MBRs and disks without partition table
 partitions="$partitions $disks"
 
 bootpart=""
 bootnum=1
 for p in $partitions; do
  # We need the busybox builtin "hexdump" utility for this!
  # sig="$(hexdump -e '"%04x"' -s 510 -n 2 $partition 2>/dev/null)"
  KERNEL=""
  INITRD=""
  ROOT="$p"
  if mount -r "$p" /mnt; then
   for k in vmlinuz boot/vmlinuz $(cd /mnt ; ls -td *linuz* boot/*linuz* grub4dos.exe 2>/dev/null); do
    if [ -r /mnt/"$k" ]; then
     KERNEL="$k"
     break
    fi
   done
   for i in $(cd /mnt ; ls -td *initrd* boot/*initrd* 2>/dev/null); do
    if [ -r /mnt/"$i" ]; then
     INITRD="$i"
     break
    fi
   done
   umount /mnt
  fi
  if [ -n "$KERNEL" ]; then
   foundboot="$((foundboot + 1))"
   echo "$p $KERNEL $INITRD"
  fi
 done >"$BOOTLIST"
#  if [ "$sig" = "aa55" -o -z "$sig" ]; then
#   # Found a bootable partition.
#   bootpart="$bootpart $p"
#   bootnum="$((bootnum + 1))"
#  fi
 return $foundboot
}

# MAIN

if [ -d /proc/scsi/usb-storage ]; then
  # Allowing slow USB-devices some more time to register...
 progress "Scanne USB/Firewire..." 5
 sleep 7
 progress ""
fi


while true; do
 progress "Scanne Partitionen..." 5
 scanpartitions
 NPARTS="$?"

 NOBOOTPARTS=""
 DIALOGITEMS=""
 if [ "$NPARTS" -lt 1 ]; then
  # No bootable partitions have been found, all we can do is reinstall.
  NOBOOTPARTS='Bitte verwenden Sie die "Reparieren"-Funktion, um den Rechner vom Server aus neu zu installieren.'
  DIALOGITEMS=" | "
 else
  while read p k i relax; do
   disk="${p#/dev/}"
   disk="${disk%%[0-9]*}"
   model="$(cat /proc/ide/$disk/model 2>/dev/null)"
   [ -n "$model" ] || model="SCSI/USB Harddisk"
   capacity="$(getsize $p 2>/dev/null)"
   DETAILS="${model:+$model}${capacity:+, $capacity MB}${k:+, KERNEL=$k}${i:+, INITRD=$i}"
   DIALOGITEMS="${DIALOGITEMS:+$DIALOGITEMS|}${p#/dev/}|$DETAILS"
  done <"$BOOTLIST"
 fi

 MENUTEXT="$MENU

$NPARTS bootbare Betriebssysteme gefunden.

$NOBOOTPARTS"
 progress ""

 IFS='|'
 $DIALOG_IFS --help "$HELP" --check "$REINSTALL" --ok-label "$BOOT" --cancel-label "$CANCEL" --menubox "$MENUTEXT" 20 75 12 $DIALOGITEMS >"$TMP"
 RC="$?"
 unset IFS
 RESULT="$(cat $TMP)"
 rm -f "$TMP"
 [ "$RC" = "0" ] || continue
 case "$RESULT" in *unchecked*) ;; *) notyet "Partitions-Restaurierung";; esac
 for b in $RESULT; do
  case "$b" in [hs]d*) bootfrom "/dev/$b"; break ;; esac
 done
 
done
exit 0
