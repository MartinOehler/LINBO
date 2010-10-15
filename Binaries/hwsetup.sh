#!/bin/sh
# This is a busybox 1.1.3 boot script
# (C) Klaus Knopper 2007
# License: GPL V2

echo "Detecting Network devices..."

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
