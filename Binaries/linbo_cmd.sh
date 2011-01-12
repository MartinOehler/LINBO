#!/bin/sh
# linbo_cmd - Backend worker script for LINBO
# (C) Klaus Knopper 2007-2010
# License: GPL V2

CLOOP_BLOCKSIZE="131072"
RSYNC_PERMISSIONS="--chmod=ug=rw,o=r"

trap bailout 1 2 3 10 12 13 15

umask 002

PID="$$"

TMP="/tmp/linbo_cmd.$$.tmp"
rm -f "$TMP"

# Sprache und Zeichensatz
# export LANG=de_DE.UTF-8
# export LANGUAGE=de_DE.UTF-8

# Nur zum Debuggen
# echo "»linbo_cmd«" "»$@«"
ps w | grep linbo_cmd | grep -v grep >"$TMP"
if [ $(cat "$TMP" | wc -l) -gt 1 ]; then
 echo "Possible Bug detected: linbo_cmd already running." >&2
 cat "$TMP" >&2
fi
rm -f "$TMP"
# EOF Debugging

printargs(){
 local arg
 local count=1
 for arg in "$@"; do
  echo -n "$((count++)):»$arg« "
 done
 echo ""
}

# File patterns for exclusion when creating/restoring rsync-archives
# These patterns should match through the entire filesystem tree.
# When cleaning up before compression, however, they will be prefixed
# by "/", in order to get applied only for the root file system.
#
# Should match common Windows/Linux garbage
RSYNC_EXCLUDE='[Pp][Aa][Gg][Ee][Ff][Ii][Ll][Ee].[Ss][Yy][Ss]
[Hh][Ii][Bb][Ee][Rr][Ff][Ii][Ll].[Ss][Yy][Ss]
[Hh][Ii][Bb][Ee][Rr][Nn][Aa][Tt][Ee].[Ss][Yy][Ss]
[Ww][Ii][Nn]386.[Ss][Ww][Pp]
Papierkorb/*
[Rr][Ee][Cc][Yy][Cc][Ll][Ee][DdRr]/*
\$[Rr][Ee][Cc][Yy][Cc][Ll][Ee].[Bb][Ii][Nn]/*
[Ll][Ii][Nn][Bb][Oo].[Ll][Ss][Tt]
tmp/*
var/tmp/*'

bailout(){
 echo "DEBUG: bailout() called, linbo_cmd=$PID, my_pid=$$" >&2
 echo ""
 # Kill all processes that have our PID as PPID.
 local processes=""
 local names=""
 local pid=""
 local cmd=""
 local stat=""
 local relax=""
 local statfile=""
 for statfile in /proc/[1-9]*/stat; do
  while read pid cmd stat ppid relax; do
   if [ "$ppid" = "$PID" ]; then
    processes="$processes $pid"
    names="$names $cmd"
   fi
  done <"$statfile"
 done
 if [ -n "$processes" ]; then
   echo "Beende Prozesse: $processes $names" >&2
   kill $processes
   sleep 1
   echo ""
 fi
 cd /
 sync; sync; sleep 1
 umount /mnt >/dev/null 2>&1 || umount -l /mnt >/dev/null 2>&1
 umount /cache >/dev/null 2>&1 || umount -l /cache >/dev/null 2>&1
 umount /cloop >/dev/null 2>&1 || umount -l /cloop >/dev/null 2>&1
 rmmod cloop >/dev/null 2>&1
 rm -f "$TMP" /tmp/rsync.exclude
 echo "Abgebrochen." >&2
 echo "" >&2
 exit $?
}

load_cloop(){
 if [ ! -r "$1" ]; then
  echo "CLOOP-Image $1 nicht vohanden!" >&2
  return 1
 fi
 local cloop_module="$(find /lib/modules/*/ -name cloop.ko | tail -1)"
 if [ ! -r "$cloop_module" ]; then
  echo "CLOOP-Modul nicht gefunden!" >&2
  return 2
 fi
 local RC
 insmod "$cloop_module" file="$1"; RC="$?"
 if [ "$RC" != 0 ]; then
  dmesg | grep cloop | tail -2 >&2
 fi
 return "$RC"
}

interruptible(){
 local RC=0
 # "$@" >"$TMP" 2>&1 &
 "$@" &
 local newpid="$!"
 wait
 RC="$?"
 case "$RC" in
  0) true ;;
  2) kill "$newpid"; cd /; bailout 0 ;;
#  *) [ -s "$TMP" ] && cat "$TMP" >&2 ;;
 esac
 return "$RC"
}

help(){
echo "
 Invalid LINBO command: »$@«

 Syntax: linbo_cmd command option1 option2 ...

 Examples:
 start bootdev rootdev kernel initrd append
               - boot OS
 syncr server  cachedev baseimage image bootdev rootdev kernel initrd
               - sync cache from server AND partitions from cache
 syncl cachedev baseimage image bootdev rootdev kernel initrd
               - sync partitions from cache

 Image types: 
 .cloop - full block device (partition) image, cloop-compressed
          accompanied by a .list file for quicksync
 .rsync - incremental rsync batch, cloop-compressed
 " 1>&2
}

# Check for "nonetwork" boot option
localmode(){
 case "$(cat /proc/cmdline)" in *\ nonetwork*|*\ localmode*) return 0;; esac
 [ -n "$1" ] || return 1
 ping -c1 "$1" 1>/dev/null || return 0
 return 1
}

cmd="$1"
[ -n "$cmd" ] && shift # Command args are now $@

# fstype partition
fstype(){
 local phead="$(dd if="$1" bs=128k count=2 2>/dev/null)"
 local RC="$?"
 [ "$RC" = "0" ] || return "$RC"
 case "$phead" in
  *NTFS*) echo "ntfs" ;;
  # tschmitt: we also need to know fat
  *[Mm][Kk][Dd][Oo][Ss][Fs][Ss]*|*[Ff][Aa][Tt]*) echo "vfat" ;;
  *[Rr][Ee][Ii][Ss][Ee][Rr]*) echo "reiserfs" ;;
  *) echo "auto" ;;
 esac
 return 0
}

# tschmitt
# get DownloadType from start.conf
downloadtype(){
 local RET=""
 if [ -s /start.conf ]; then
  RET="$(grep -i ^downloadtype /start.conf | tail -1 | awk -F= '{ print $2 }' | awk '{ print $1 }' | tr A-Z a-z)"
  # get old option for compatibility issue
  if [ -z "$RET" ]; then
   RET="$(grep -i ^usemulticast /start.conf | tail -1 | awk -F= '{ print $2 }' | awk '{ print $1 }' | tr A-Z a-z)"
   [ "$RET" = "yes" ] && RET="multicast"
  fi
 fi
 echo "$RET"
}

# tschmitt
# fetch hostgroup from start.conf
hostgroup(){
 local hostgroup=""
 [ -s /start.conf ] || return 1
 hostgroup=`grep -i ^group /start.conf | tail -1 | awk -F= '{ print $2 }' | awk '{ print $1 }'`
 echo "$hostgroup"
}

# fetch fstype from start.conf
# fstype_startconf dev
fstype_startconf(){
 local dev="$1"
 local type=""
 local section=""
 local param=""
 local tdev=""
 local pfound=0
 local dfound=0
 local line=""
 if [ ! -e /start.conf ]; then
   echo "Fatal! start.conf nicht gefunden!"
   return 1
 fi
 while read line; do
  section=`echo $line | awk '{ print $1 }' | tr A-Z a-z`
  [ "$section" = "[partition]" ] && pfound=1
  if [ "$pfound" = 1 ]; then
    param=`echo $line | awk '{ print $1 }' | tr A-Z a-z`
    if [ "$param" = "dev" ]; then
     tdev=`echo $line | awk -F= '{ print $2 }' | awk '{ print $1 }'`
     if [ "$tdev" = "$dev" ]; then
      dfound=1
     else
      pfound=0; dfound=0; type=""
     fi
    fi
    if [ "$param" = "fstype" ]; then
     type=`echo $line | awk -F= '{ print $2 }' | awk '{ print $1 }' | tr A-Z a-z`
    fi
    if [ "$dfound" = 1 -a -n "$type" ]; then
     echo "$type"
     return 0
    fi
  fi
 done </start.conf
 return 1
}

# mountpart partition destdir [options]
mountpart(){
 local RC=0
 local type=""
 local i=0
 # "noatime" is required for later remount, otherwise kernel will default to "relatime",
 # which busybox mount does not know
 local OPTS="noatime"
 if [ "$3" = "-r" ]; then OPTS="$OPTS,ro"; else OPTS="$OPTS,rw"; fi
 # fix vanished cloop symlink
 if [ "$1" = "/dev/cloop" ]; then
  [ -e "/dev/cloop" ] || ln -sf /dev/cloop0 /dev/cloop
 fi
 for i in 1 2 3 4 5; do
  type="$(fstype $1)"
  RC="$?"
  [ "$RC" = "0" ] && break
  [ "$i" = "5" ] && break
  echo "Partition $1 ist noch nicht verfügbar, versuche erneut..."
  sleep 2
 done
 [ "$RC" = "0" ] || { echo "Partition $1 ist nicht verfügbar, wurde die Platte schon partitioniert?" 1>&2; return "$RC"; }
 case "$type" in
  ntfs)
   OPTS="$OPTS,force,silent,umask=0,no_def_opts,allow_other,streams_interface=xattr"
   ntfs-3g "$1" "$2" -o "$OPTS" 2>/dev/null; RC="$?"
   ;;
  vfat)
   OPTS="$OPTS,umask=000,shortname=winnt,utf8"
   mount -o "$OPTS" "$1" "$2" ; RC="$?"
   ;;
  *)
   # Should we really enable acls and user_xattr?
   # ext2/ext3/reiserfs support them
   OPTS="$OPTS,acl,user_xattr"
   mount -o "$OPTS" "$1" "$2" ; RC="$?"
   # mount $3 "$1" "$2" ; RC="$?"
   ;;
 esac
 return "$RC"
}

# Is /cache writable?
# Displayed mount permissions may not be correct, do a write-test.
cache_writable(){
local testfile="/cache/.write_test"
 [ -f "$testfile" ] && rm -f "$testfile" 2>/dev/null
 echo > "$testfile" 2>/dev/null
 local RC="$?"
 [ -f "$testfile" ] && rm -f "$testfile" 2>/dev/null
 return "$RC"
}

# Return true if cache is NFS- or SAMBA-Share
remote_cache(){
 case "$1" in *:*|*//*|*\\\\*) return 0 ;; esac
 return 1
}

# format dev fs
format(){
 echo -n "format " ;  printargs "$@"
# local dev="${1%%[0-9]*}"
# local part="${1#$dev}"
 case "$2" in
  swap) mkswap "$1" ;;
  reiserfs) mkreiserfs -f -f  "$1" | egrep '(^Init|^Sync|^ReiserFS)' ;;
  ext2) mke2fs -F -b 4096 -m 0 "$1" ;;
  ext*) mke2fs -F -b 4096 -m 0 -j "$1" ;;
  [Nn][Tt][Ff][Ss]*) mkfs.ntfs -Q "$1" ;;
  *[Ff][Aa][Tt]*) mkdosfs -F 32 "$1" ;;
  *) return 1 ;;
 esac
 return $?
}

# mountcache partition [options]
mountcache(){
 local RC=1
 [ -n "$1" ] || return 1
 export CACHE_PARTITION="$1"
 # Avoid duplicate mounts by just preparing read/write mode
 if grep -q "^$1 " /proc/mounts; then
  local RW
  grep -q "^$1 .*rw.*" /proc/mounts && RW="true" || RW=""
  case "$2" in
   -r|-o\ *ro*) [ -n "$RW" ] && mount -o remount,ro /cache; RC=0 ;; 
   *) [ -n "$RW" ] || mount -o remount,rw /cache; RC="$?" ;; 
  esac
  return "$RC"
 fi
 case "$1" in
  *:*) # NFS
   local server="${1%%:*}"
   local dir="${1##*:}"
   echo "Mounte /cache per NFS von $1..."
   # -o nolock is EXTREMELY important here, otherwise mount.nfs will timeout waiting for
   # local portmap
   mount $2 -t nfs -o nolock,rsize=8192,wsize=8192,hard,intr "$1" /cache
   RC="$?"
   ;;
  \\\\*\\*|//*/*) # CIFS/SAMBA
   local server="${1%\\*}";  server="${server%/*}"; server="${server#\\\\}"; server="${server#//}"
   echo "Mounte /cache per SAMBA/CIFS von $1..."
   # unix extensions have to be disabled
   echo 0 > /proc/fs/cifs/LinuxExtensionsEnabled 2>/dev/null
   # mount.cifs (3) pays attention to $PASSWD
   # this does not work: $RSYNC_PASSWORD is not available and mount.cifs does not pay attention to $PASSWD
   #export PASSWD="$RSYNC_PASSWORD"
   #mount $2 -t cifs -o username=linbo,nolock "$1" /cache 2>/dev/null
   # temporary workaround for password
   [ -s /tmp/linbo.passwd ] && PASSWD="$(cat /tmp/linbo.passwd 2>/dev/null)"
   [ -z "$PASSWD" -a -s /tmp/rsyncd.secrets ] && PASSWD="$(grep ^linbo /tmp/rsyncd.secrets | awk -F\: '{ print $2 }' 2>/dev/null)"
   mount $2 -t cifs -o user=linbo,pass="$PASSWD",nolock "$1" /cache 2>/dev/null
   RC="$?"
   if [ "$RC" != "0" ]; then
    echo "Zugriff auf $1 als User \"linbo\" mit Authentifizierung klappt nicht."
    mount $2 -t cifs -o nolock,guest,sec=none "$1" /cache
    RC="$?"
    if [ "$RC" != "0" ]; then
     echo "Zugriff als \"Gast\" klappt auch nicht."
    fi
   fi
   ;;
  /dev/*) # local cache
   # Check if cache partition exists
   if grep -q "${1##*/}" /proc/partitions; then
    if cat /proc/mounts | grep -q /cache; then
     echo "Cache ist bereits gemountet."
     RC=0
    else
     mountpart "$1" /cache $2 ; RC="$?"
    fi
    if [ "$RC" != "0" ]; then
     # Cache partition has not been formatted yet?
     local cachefs="$(fstype_startconf "$1")"
     if [ -n "$cachefs" ]; then
      echo "Formatiere Cache-Partition..."
      format "$1" "$cachefs"
     fi
     # Retry.
     mountpart "$1" /cache $2 ; RC="$?"
    fi
   else
    echo "Cache-Partition existiert nicht."
   fi
   ;;
   *) # Yet unknown
   echo "Unbekannte Quelle für LINBO-Cache: $1" >&2
   ;;
  esac
 [ "$RC" = "0" ] || echo "Kann $1 nicht als /cache einbinden." >&2
 return "$RC"
}

killalltorrents(){
 local WAIT=5
 # check for running torrents and kill them if any
 if [ -n "`ps w | grep ctorrent | grep -v grep`" ]; then
  echo "Killing torrents ..."
  killall -9 ctorrent 2>/dev/null
  sleep "$WAIT"
  [ -n "`ps w | grep ctorrent | grep -v grep`" ] && sleep "$WAIT"
 fi
}

# partition dev1 size1 id1 bootable1 filesystem dev2 ...
# When "$NOFORMAT" is set, format is skipped, otherwise all
# partitions with known fstypes are formatted.
partition(){
 echo -n "partition " ;  printargs "$@"
 # umount /cache if mounted
 killalltorrents
 if cat /proc/mounts | grep -q /cache; then
  cd /
  if ! umount /cache &>/dev/null; then
   umount -l /cache &>/dev/null
   sleep "$WAIT"
   if cat /proc/mounts | grep -q /cache; then
    echo "Kann /cache nicht unmounten." >&2
    return 1
   fi
  fi
 fi
 local table=""
 local formats=""
 local disk="${1%%[1-9]*}"
 local cylinders=""
 local disksize=""
 local dummy=""
 local relax=""
 local pcount=0
 local fstype=""
 local bootable=""
 read d cylinders relax <<.
$(sfdisk -g "$disk")
.
 read disksize relax <<.
$(sfdisk -s "$disk")
.
 [ -n "$cylinders" -a "$cylinders" -gt 0 -a -n "$disksize" -a "$disksize" -gt 0 ] >/dev/null 2>&1 || { echo "Festplatten-Geometrie von $disk lässt sich nicht lesen, cylinders=$cylinders, disksize=$disksize, Abbruch." >&2; return 1; }
 while [ "$#" -ge "5" ]; do
  local dev="$1"
  [ -n "$dev" ] || continue
  local csize=""
  if [ "$2" -gt 0 ] 2>/dev/null; then
   # Cylinders = kilobytes * totalcylinders / totalkilobytes
   csize="$(($2 * $cylinders / $disksize))"
   [ "$(($csize * $disksize / $cylinders))" -lt "$2" ] && let csize++
  fi
  if [ -n "$table" ]; then
   table="$table
"
  fi
  let pcount++
  # Is this a primary partition?
  local partno="${dev##?d?}"
  if [ "$partno" -gt 4 ] >/dev/null 2>&1; then
   # Fill up unused partitions
   while [ "$pcount" < 5 ]; do
    table="$table;
"
    let pcount++
   done
  fi
  # Insert table entry.
  bootable="$4"
  [ "$bootable" = "-" ] && bootable=""
  fstype="$5"
  [ "$fstype" = "-" ] && fstype=""
  table="$table,$csize,$3${bootable:+,*}"
  [ -n "$fstype" ] && formats="$formats $dev,$fstype"
  shift 5
 done
 # tschmitt: This causes windows to recognize a new harddisk after each partitioning, which leeds
 # further to a rather annoying "System settings changed, do you want to restart" dialog box,
 # therefore deactivated
 #dd if=/dev/zero of="$disk" bs=512 count=1
 sfdisk -D -f "$disk" 2>&1 <<EOT
$table
EOT
 if [ "$?" = "0" -a -z "$NOFORMAT" ]; then
  sleep 2
  local i=""
  for i in $formats; do
   format "${i%%,*}" "${i##*,}"
  done
 fi
}

# mkgrubmenu partition [kernel initrd server append]
# Creates/updates menu.lst with given partition
# /cache is already mounted when this is called.
mkgrubmenu(){
 local menu="/cache/boot/grub/menu.lst"
 local grubdisk="hd0"
 case "$1" in
  *[hs]da) grubdisk=hd0 ;;
  *[hs]db) grubdisk=hd1 ;;
  *[hs]dc) grubdisk=hd2 ;;
  *[hs]dd) grubdisk=hd3 ;;
 esac
 local grubpart="${1##*[hs]d[a-z]}"
 grubpart="$((grubpart - 1))"
 local root="root ($grubdisk,$grubpart)"
 echo
 case "$(cat $menu 2>/dev/null)" in
  *$root*) true ;; # Entry for this partition is already present
  *)
    if [ -n "$2" ]; then
     echo "default saved"
     echo "timeout 0"
     echo ""
     echo "title LINBO ($1)"
     echo "$root"
     echo "kernel /$2 server=$4 cache=$1 $5"
     echo "initrd /$3"
    else
     echo ""
     echo "title WINDOWS ($1)"
     echo "$root"
     echo "chainloader +1"
     echo "savedefault 0"
    fi >>"$menu"
    ;;
 esac
}

# tschmitt: mkmenulst bootpart bootfile
# Creates menu.lst with given partition
# /cache and /mnt is already mounted when this is called.
mkgrldr(){
 local menu="/mnt/menu.lst"
 local grubdisk="hd0"
 local bootfile="$2"
 local driveid="0x80"
 case "$1" in
  *[hsv]da) grubdisk=hd0; driveid="0x80" ;;
  *[hsv]db) grubdisk=hd1; driveid="0x81" ;;
  *[hsv]dc) grubdisk=hd2; driveid="0x82" ;;
  *[hsv]dd) grubdisk=hd3; driveid="0x83" ;;
 esac
 local grubpart="${1##*[hsv]d[a-z]}"
 grubpart="$((grubpart - 1))"
 bootlace.com --"$(fstype_startconf "$1")" --floppy="$driveid" "$1"
 echo -e "default 0\ntimeout 0\nhiddenmenu\n\ntitle Windows\nroot ($grubdisk,$grubpart)\nchainloader ($grubdisk,$grubpart)/$bootfile" > $menu
 cp /usr/lib/grub/grldr /mnt
}

# tschmitt
# patch fstab with root partition
patch_fstab(){
 echo -n "patch_fstab " ;  printargs "$@"
 local appendstr="$1"
 local line=""
 local rootpart=""
 local found=""
 local item=""
 for item in $appendstr; do
  echo $item | grep -q ^root && rootpart=`echo $item | awk -F\= '{ print $2 }'`
 done
 [ -z "$rootpart" ] && return 1
 [ -e /tmp/fstab ] && rm -f /tmp/fstab
 while read line; do
  mntpnt=`echo $line | awk '{ print $2 }'`
  if [ "$mntpnt" = "/" -a "$found" = "" ] && ! echo "$line" | grep ^#; then
   echo "$line" | sed -e 's,.* /,'"$rootpart"' /,' - >> /tmp/fstab
   found=yes
  else
    echo "$line" >> /tmp/fstab
  fi
 done </mnt/etc/fstab
 if [ -n "$found" ]; then
  echo "Setze Rootpartition in fstab -> $rootpart."
  mv -f /mnt/etc/fstab /mnt/etc/fstab.bak
  mv -f /tmp/fstab /mnt/etc
 fi
}

# start boot root kernel initrd append cache
start(){
 echo -n "start " ;  printargs "$@"
 local WINDOWS_PXE=""
 local i
 local cpunum=1
 local disk="${1%%[1-9]*}"
 if mountpart "$1" /mnt -w ; then
  LOADED=""
  KERNEL="/mnt/$3"
  INITRD=""
  [ -n "$4" -a -r /mnt/"$4" ] && INITRD="--initrd=/mnt/$4"
  APPEND="$5"
  # tschmitt: repairing grub mbr on every start
  if mountcache "$6" && cache_writable ; then
   [ -e /cache/boot/grub ] || mkdir -p /cache/boot/grub
   grub-install --root-directory=/cache $disk
  fi
  case "$3" in
   *[Gg][Rr][Uu][Bb].[Ee][Xx][Ee]*)
    # Load grub.exe preferably from cache partition, if present
    if [ -r "/cache/$3" ]; then
     KERNEL="/cache/$3"
     LOADED="true"
    fi
    [ -r "$KERNEL" ] || KERNEL="/usr/lib/grub.exe" # Use builtin
    [ -z "$APPEND" ] && APPEND="--config-file=map(rd) (hd0,0); map --hook; chainloader (hd0,0)/ntldr; rootnoverify(hd0,0) --device-map=(hd0) $disk"
    ;;
   *[Pp][Xx][Ee][Gg][Rr][Uu][Bb]*)
     # tschmitt: if kernel is pxegrub assume that it is a real windows, which has to be rebootet
     WINDOWS_PXE="yes"
     LOADED="true"
     # tschmitt: needed for local boot here
     if [ -e /cache/boot/grub ] && cache_writable; then
      mkgrubmenu "$1"
      grub-set-default --root-directory=/cache 1
     fi
     # oehler: set windows boot flag
     flag="$(printf '\%o' 1)"
     echo -n -e "$flag" | dd seek=432 bs=1 count=1 of=$disk conv=notrunc 2>/dev/null
     ;;
   *)
    if [ -n "$2" ]; then
     APPEND="root=$2 $APPEND"
    fi
    ;;
  esac
  # provide a menu.lst for grldr on win2k/xp
# Should not be necessary anymore with new grub4dos local boot. -KK
# if [ -e /mnt/[Nn][Tt][Ll][Dd][Rr] ]; then
#   mkgrldr "$1" ntldr
#  elif [ -e /mnt/[Ii][Oo].[Ss][Yy][Ss] ]; then
  if [ -e /mnt/[Nn][Tt][Ll][Dd][Rr] ]; then
   true
  elif [ -e /mnt/[Ii][Oo].[Ss][Yy][Ss] ]; then
   # tschmitt: patch autoexec.bat (win98),
   if ! grep ^'if exist C:\\linbo.reg' /mnt/AUTOEXEC.BAT; then
    echo "if exist C:\linbo.reg regedit C:\linbo.reg" >> /mnt/AUTOEXEC.BAT
    unix2dos /mnt/AUTOEXEC.BAT
   fi
   # provide a menu.lst for grldr on win98
   mkgrldr "$1" io.sys
   APPEND="$(echo $APPEND | sed -e 's/ntldr/io.sys/' | sed -e 's/bootmgr/io.sys/')"
  elif [ -e /mnt/etc/fstab ]; then
   # tschmitt: patch fstab with root device
   patch_fstab "$APPEND"
  fi
 else
  echo "Konnte Betriebssystem-Partition $1 nicht mounten." >&2
  umount /mnt 2>/dev/null
  mountcache "$6" -r
  return 1
 fi
 # kill torrents if any
 killalltorrents
 # No more timer interrupts
 [ -f /proc/sys/dev/rtc/max-user-freq ] && echo "1024" >/proc/sys/dev/rtc/max-user-freq 2>/dev/null
 [ -f /proc/sys/dev/hpet/max-user-freq ] && echo "1024" >/proc/sys/dev/hpet/max-user-freq 2>/dev/null
 if [ -z "$WINDOWS_PXE" ]; then
  echo "kexec -l $INITRD --append=\"$APPEND\" $KERNEL"
  kexec -l $INITRD --append="$APPEND" $KERNEL && LOADED="true"
  sleep 3
 fi
 umount /mnt 2>/dev/null
 umount /cache || umount -l /cache 2>/dev/null
 if [ -n "$LOADED" ]; then
  # Workaround for missing speedstep-capability of Windows
  local i=""
  for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
   if [ -f "$i" ]; then
    echo "Setze CPU #$((cpunum++)) auf maximale Leistung."
    echo "performance" > "$i"
   fi
  done
  [ "$cpunum" -gt "1" ] && sleep 4
  # We basically do a quick shutdown here.
  killall5 -15
  [ -z "$WINDOWS_PXE" ] && sleep 2
  echo -n "c" >/dev/console
  if [ -z "$WINDOWS_PXE" ]; then
   kexec -e --reset-vga
   # exec kexec -e
   sleep 10
  else
   #sleep 2
   reboot -f
   sleep 10
  fi
 else
  echo "Betriebssystem konnte nicht geladen werden." >&2
  return 1
 fi
}

# return partition size in kilobytes
# arg: partition
get_partition_size(){
 # fix vanished cloop symlink
 if [ "$1" = "/dev/cloop" ]; then
  [ -e "/dev/cloop" ] || ln -sf /dev/cloop0 /dev/cloop
 fi
 sfdisk -s "$1"
 return $?
}

# echo file size in bytes
get_filesize(){
 ls -l "$1" 2>/dev/null | awk '{print $5}' 2>/dev/null
 return $?
}

# mkexclude
# Create /tmp/rsync.exclude
mkexclude(){
rm -f /tmp/rsync.exclude
cat > /tmp/rsync.exclude <<EOT
${RSYNC_EXCLUDE}
EOT
}

# cleanup_fs directory
# Removes all files from ${RSYNC_EXCLUDE}
# in the root directory only.
cleanup_fs(){
 ( 
  local i=""
  cd "$1" || return 1
  for i in ${RSYNC_EXCLUDE}; do # Expand patterns
   if [ -e "$i" ]; then
    echo "Lösche $i."
    rm -rf "$i"
   fi
  done
 )
}

# mk_cloop type inputdev imagename baseimage [timestamp]
mk_cloop(){
 echo "## $(date) : Starte Erstellung von $1."
 #echo -n "mk_cloop " ;  printargs "$@"
 # kill torrent process for this image
 local pid="$(ps w | grep ctorrent | grep "$3.torrent" | grep -v grep | awk '{ print $1 }')"
 [ -n "$pid" ] && kill "$pid"
 # remove torrent files
 [ -e "/cache/$3.torrent" ] && rm "/cache/$3.torrent"
 [ -e "/cache/$3.complete" ] && rm "/cache/$3.complete"
 local RC=1
 local size="$(get_partition_size $2)"
 local imgsize=0
 case "$1" in
  partition) # full partition dump
   if mountpart "$2" /mnt -w ; then
    echo "Bereite Partition $2 (Größe=${size}K) für Komprimierung vor..."
    cleanup_fs /mnt
    echo "Leeren Platz auffüllen mit 0en..."
    # Create nulled files of size 1GB, should work on any FS.
    local count=0
    while true; do
     interruptible dd if=/dev/zero of="/mnt/zero$count.tmp" bs=1024k count=1000 2>/dev/null || break
     [ -s "/mnt/zero$count.tmp" ] || break
     let count++
     echo "$(du -ch /mnt/zero*.tmp | tail -1 | awk '{ print $1 }') genullt... "
    done
    # Sync is asynchronous, unless started twice at least.
    sync ; sync ; sync 
    rm -f /mnt/zero*.tmp
    echo "Dateiliste erzeugen..."
    ( cd /mnt/ ; find . | sed 's,^\.,,' ) > "$3".list
    umount /mnt || umount -l /mnt
   fi
   echo "Starte Kompression von $2 -> $3 (ganze Partition, ${size}K)."
   echo "create_compressed_fs -B $CLOOP_BLOCKSIZE -L 1 -t 2 -s ${size}K $2 $3"
   interruptible create_compressed_fs -B "$CLOOP_BLOCKSIZE" -L 1 -t 2 -s "${size}K" "$2" "$3" 2>&1
   RC="$?"
   if [ "$RC" = "0" ]; then
    # create status file
    if mountpart "$2" /mnt -w ; then
     echo "${3%.cloop}" > /mnt/.linbo
     umount /mnt || umount -l /mnt
    fi
    # create info file
    imgsize="$(get_filesize $3)"
    # Adjust uncompressed image size with one additional cloop block
    size="$(($CLOOP_BLOCKSIZE / 1024 + $size))"
    mk_info "$3" "$2" "$size" "$imgsize" >"$3".info
    echo "Fertig."
    ls -l "$3"
   else
    echo "Das Komprimieren ist fehlgeschlagen." >&2
   fi
  ;;
  incremental)
   if mountpart "$2" /mnt -r ; then
    rmmod cloop >/dev/null 2>&1
    if test -s /cache/"$4" && load_cloop /cache/"$4"; then
     mkdir -p /cloop
     if mountpart /dev/cloop /cloop -r ; then
      echo "Starte Kompression von $2 -> $3 (differentiell)."
      cleanup_fs /mnt
      mkexclude
      # determine rsync opts due to fstype
      local type="$(fstype "$2")"
      case $type in
       ntfs) ROPTS="-HazAX" ;;
       vfat) ROPTS="-rtz" ;;
       *) ROPTS="-Haz" ;;
      esac
      interruptible rsync "$ROPTS" --exclude="/.linbo" --exclude-from="/tmp/rsync.exclude" --delete --delete-excluded --only-write-batch="$3" /mnt/ /cloop 2>&1
      RC="$?"
      umount /cloop
      if [ "$RC" = "0" ]; then
        imgsize="$(get_filesize $3)"
        mk_info "$3" "$2" "" "$imgsize" >"$3".info
        echo "Fertig."
        ls -l "$3"
      else
       echo "Das differentielle Imagen ist fehlgeschlagen, rsync Fehler-Code: $RC." >&2
       sleep 5
      fi
     else
      RC="$?"
      # DEBUG, REMOVEME
       dmesg | tail -5 >&2
     fi
    else
     echo "Image $4 kann nicht eingebunden werden," >&2
     echo "ist aber für die differentielle Sicherung notwendig." >&2
     RC="$?"
    fi
    rmmod cloop >/dev/null 2>&1
    umount /mnt || umount -l /mnt
   else
    RC="$?"
   fi
  ;; 
 esac
 # create torrent file
 if [ "$RC" = "0" ]; then
  echo "Erstelle torrent Dateien ..."
  touch "$3".complete
  local serverip="$(grep -i ^server /start.conf | awk -F\= '{ print $2 }' | awk '{ print $1 }')"
  ctorrent -t -u http://"$serverip":6969/announce -s "$3".torrent "$3"
 fi
 echo "## $(date) : Beende Erstellung von $1."
 return "$RC"
}

# check_status partition imagefile:
# returns true if mountable & contains a version of the archive.
check_status(){
 local RC=1
 local base="${2##*/}"
 base="${base%.[Cc][Ll][Oo]*}"
 base="${base%.[Rr][Ss][Yy]*}"
 mountpart "$1" /mnt -r 2>/dev/null || return $?
 [ -s /mnt/.linbo ] && case "$(cat /mnt/.linbo 2>/dev/null)" in *$base*) RC=0 ;; esac
 umount /mnt || umount -l /mnt
# [ "$RC" = "0" ] && echo "Enthält schon eine Version von $2."
 return "$RC"
}

# update_status partition imagefile:
# Add information about installed archives to partition
update_status(){
 local base="${2##*/}"
 base="${base%.[Cc][Ll][Oo]*}"
 base="${base%.[Rr][Ss][Yy]*}"
 mountpart "$1" /mnt -w 2>/dev/null || return $?
 case "$2" in *.[Cc][Ll][Oo]*) rm -f /mnt/.linbo ;; esac
 echo "$base" >> /mnt/.linbo
 sync; sync; sleep 1
 umount /mnt || umount -l /mnt
 return 0
}

# INITIAL copy
# cp_cloop imagefile targetdev
cp_cloop(){
 echo "## $(date) : Starte Komplettrestore von $1."
 # echo -n "cp_cloop " ;  printargs "$@"
 local RC=1
 rmmod cloop >/dev/null 2>&1
 # repair cloop link if vanished
 [ -e /dev/cloop ] || ln -s /dev/cloop0 /dev/cloop
# echo "modprobe cloop file=/cache/$1"
 if test -s "$1" && load_cloop /cache/"$1"; then
  local s1="$(get_partition_size /dev/cloop)"
  local s2="$(get_partition_size $2)"
  local block="$(($CLOOP_BLOCKSIZE / 1024))"
  if [ "$(($s1 - $block))" -gt "$s2" ] 2>/dev/null; then
   echo "FEHLER: Cloop Image $1 (${s1}K) ist größer als Partition $2 (${s2}K)" >&2
   echo 'FEHLER: Das passt nicht!' >&2
   rmmod cloop >/dev/null 2>&1
   return 1
  fi
  # Userspace program MAY be faster than kernel module (no kernel lock necessary)
  # Forking an additional dd makes use of a second CPU and speeds up writing
  ( interruptible extract_compressed_fs /cache/"$1" - | dd of="$2" bs=1M ) 2>&1
  # interruptible dd if=/dev/cloop of="$2" bs=1024k
  RC="$?"
 else
  RC="$?"
  # DEBUG, REMOVEME
  dmesg | tail -5
  echo "Fehler: Archiv \"$1\" nicht vorhanden oder defekt." >&2
 fi
 rmmod cloop >/dev/null 2>&1
 if [ "$(fstype $2)" = "ntfs" ]; then
  # Fix number of heads in NTFS, Windows boot insists that this
  # is >= the number reported by BIOS
  local heads=255
#  local disk="${2%%[1-9]*}"
#  local d
#  local cylinders
#  local c
#  read d cylinders c heads relax <<.
#$(sfdisk -g "$disk")
#.
  if [ "$heads" -gt 0 -a "$heads" -le 255 ] 2>/dev/null; then
   heads="$(printf '\%o' $heads)"
   # Number of heads at NTFS offset 0x1a (26)
   echo -n -e "$heads" | dd seek=26 bs=1 count=1 of="$2" conv=notrunc 2>/dev/null
  fi
 fi
 [ "$RC" = "0" ] && update_status "$2" "$1"
 echo "## $(date) : Beende Komplettrestore von $1."
 return "$RC"
}

# INCREMENTAL/Synced
# sync_cloop imagefile targetdev
sync_cloop(){
 echo "## $(date) : Starte Synchronisation von $1."
 # echo -n "sync_cloop " ;  printargs "$@"
 local RC=1
 local ROPTS="-HaAX"
 #local ROPTS="-a"
 [ "$(fstype "$2")" = "vfat" ] && ROPTS="-rt"
 if mountpart "$2" /mnt -w ; then
  case "$1" in
   *.[Rr][Ss][Yy]*)
    rm -f "$TMP"
    interruptible rsync "$ROPTS" --compress --delete --read-batch="$1" /mnt >"$TMP" 2>&1 ; RC="$?"
    if [ "$RC" != "0" ]; then
     cat "$TMP" >&2
     echo "Fehler beim Synchronisieren des differentiellen Images \"$1\" nach $2, rsync-Fehlercode: $RC." >&2
     sleep 2
    fi
    rm -f "$TMP"
   ;;
   *.[Cc][Ll][Oo]*)
    rmmod cloop >/dev/null 2>&1
    if test -s "$1" && load_cloop /cache/"$1"; then
     mkdir -p /cloop
     if mountpart /dev/cloop /cloop -r ; then
      list="$1".list
      FROMLIST=""
      [ -r "$list" ] && FROMLIST="--files-from=$list"
      mkexclude
      # tschmitt: with $FROMLIST only files which are in the list were synced and
      # new files, that are not in the image, where not deleted.
      # Therefore we try it without $FROMLIST, which gives the expected result.
      #interruptible rsync -Ha --partial $FROMLIST --exclude="/.linbo" --exclude-from="/tmp/rsync.exclude" --delete --delete-excluded /cloop/ /mnt ; RC="$?"
      rm -f "$TMP"
      interruptible rsync "$ROPTS" --exclude="/.linbo" --exclude-from="/tmp/rsync.exclude" --delete --delete-excluded /cloop/ /mnt >"$TMP" 2>&1 ; RC="$?"
      umount /cloop
      if [ "$RC" != "0" ]; then
       cat "$TMP" >&2
       echo "Fehler beim Restaurieren des Images \"$1\" nach $2, rsync-Fehlercode: $RC." >&2
       sleep 2
      fi
      rm -f "$TMP"
     else
      RC="$?"
      # DEBUG/REMOVEME
      dmesg | tail -5
      echo "Fehler: /cloop kann nicht vom Image \"$1\" gemountet werden." >&2
     fi
    else
     RC="$?"
     echo "Fehler: Image \"$1\" fehlt oder ist defekt." >&2
    fi
    rmmod cloop >/dev/null 2>&1
   ;;
  esac
  sync; sync; sleep 1
  umount /mnt || umount -l /mnt
 else
  RC="$?"
 fi
 [ "$RC" = "0" ] && update_status "$2" "$1"
 echo "## $(date) : Beende Synchronisation von $1."
 return "$RC"
}

# restore imagefile targetdev [force]
restore(){
 #echo -n "restore " ;  printargs "$@"
 local RC=1
 local disk="${2%%[1-9]*}"
 local force="$3"
 local fstype="$(fstype_startconf "$2")"
 echo -n "Entpacke: $1 -> $2 "
 case "$1" in
  *.[Cc][Ll][Oo]*)
   if [ "$force" != "force" ]; then
    check_status "$2" "$1" || force="force"
   fi
   if [ "$force" = "force" ]; then
    echo "[Komplette Partition]..."
    cp_cloop "$1" "$2" ; RC="$?"
   else
    echo "[Datei-Sync]..."
    sync_cloop "$1" "$2" ; RC="$?"
   fi
   ;;
  *.[Rr][Ss][Yy]*)
   echo "[Datei-Sync]..."
   sync_cloop "$1" "$2" ; RC="$?"
   ;;
 esac
 if [ "$RC" = "0" ]; then
  echo "Fertig."
 else
  return "$RC"
 fi
 return "$RC"
}

# syncl cachedev baseimage image bootdev rootdev kernel initrd append [force]
syncl(){
 local RC=1
 local patchfile=""
 local postsync=""
 local rootdev="$5"
 # don't sync in that case
 if [ "$1" = "$rootdev" ]; then
  echo "Ueberspringe lokale Synchronisation. Image $2 wird direkt aus Cache gestartet."
  return 0
 fi
 echo -n "syncl " ; printargs "$@"
 mountcache "$1" || return "$?"
 cd /cache
 local image=""
 for image in "$2" "$3"; do
  [ -n "$image" ] || continue
  if [ -f "$image" ]; then
   restore "$image" "$5" $9 ; RC="$?"
   [ "$RC" = "0" ] || break
   patchfile="$image.reg"
   postsync="$image.postsync"
  else
   echo "$image ist nicht vorhanden." >&2
   RC=1
   break
  fi
 done
 if [ "$RC" = "0" ]; then
  # Apply patches
  if mountpart "$5" /mnt -w ; then
   # hostname
   local HOSTNAME
   if localmode; then
    if [ -s /cache/hostname -a -e /tmp/.offline ]; then
     # add -w to hostname for wlan clients if client is really offline
     HOSTNAME="$(cat /cache/hostname)-w"
    else
     HOSTNAME="$(cat /cache/hostname)"
    fi
   else
    HOSTNAME="$(hostname)"
    echo $HOSTNAME > /cache/hostname
   fi
   if [ -r "$patchfile" ]; then
    echo "Patche System mit $patchfile"
    rm -f "$TMP"
    sed 's|{\$HostName\$}|'"$HOSTNAME"'|g;y//\n/' "$patchfile" > "$TMP"
    # tschmitt: different registry patching for Win98, WinXP, Win7
    if [ -e /mnt/[Nn][Tt][Ll][Dd][Rr] -o -e /mnt/[Bb][Oo][Oo][Tt][Mm][Gg][Rr] ]; then
     # tschmitt: logging
     echo -n "Patche System mit $patchfile ... " >/tmp/patch.log
     cat "$TMP" >>/tmp/patch.log
     patch_registry "$TMP" /mnt 2>&1 >>/tmp/patch.log
     [ -e /tmp/output ] && cat /tmp/output >>/tmp/patch.log
     echo "Fertig."
     # patch newdev.dll (xp/2000 only)
     if [ -e /mnt/[Nn][Tt][Ll][Dd][Rr] ]; then
      local newdevdll="$(ls /mnt/[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/[Ss][Yy][Ss][Tt][Ee][Mm]32/[Nn][Ee][Ww][Dd][Ee][Vv].[Dd][Ll][Ll])"
      [ -z "$newdevdll" ] && newdevdll="$(ls /mnt/[Ww][Ii][Nn][NN][Tt]/[Ss][Yy][Ss][Tt][Ee][Mm]32/[Nn][Ee][Ww][Dd][Ee][Vv].[Dd][Ll][Ll])"
      # patch newdev.dll only if it has not yet patched
      if [ -n "$newdevdll" -a ! -s "$newdevdll.linbo-orig" ]; then
       echo "Patche $newdevdll ..."
       [ -e "$newdevdll.linbo-orig" ] || cp "$newdevdll" "$newdevdll.linbo-orig"
       grep ^: /etc/newdev-patch.bvi | bvi "$newdevdll" >>/tmp/patch.log 2>&1
      fi
     fi
     # write partition bootsector
     [ "$(fstype "$5")" = "vfat" ] && ms-sys -2 "$5"
    elif [ -e /mnt/[Ii][Oo].[Ss][Yy][Ss] ]; then
     cp -f "$TMP" /mnt/linbo.reg
     unix2dos /mnt/linbo.reg
     ms-sys -3 "$5"
    fi
    rm -f "$TMP"
   fi
   if [ -f /mnt/etc/hostname ]; then
    if [ -n "$HOSTNAME" ]; then
     echo "Setze Hostname -> $HOSTNAME."
     echo "$HOSTNAME" > /mnt/etc/hostname
    fi
   fi
   # fstab
   [ -f /mnt/etc/fstab ] && patch_fstab "$rootdev"
   sync; sync; sleep 1
   umount /mnt || umount -l /mnt
  fi
 fi
 cd / ; mountcache "$1" -r
 return "$RC"
}

# create cachedev imagefile baseimagefile bootdev rootdev kernel initrd
create(){
 echo -n "create " ;  printargs "$@"
 [ -n "$2" -a -n "$1" -a -n "$5" ] || return 1
 mountcache "$1" || return "$?"
 if ! cache_writable; then
  echo "Cache-Partition ist nicht schreibbar, Abbruch." >&2
  mountcache "$1" -r
  return 1
 fi
 cd /cache
 local RC="1"
 local type="$(fstype "$5")"
 echo "Erzeuge Image '$2' von Partition '$5'..."
 case "$2" in
  *.[Cc][Ll][Oo]*)
    mk_cloop partition "$5" "$2" "$3" ; RC="$?"
   ;;
  *.[Rr][Ss][Yy]*)
    mk_cloop incremental "$5" "$2" "$3" ; RC="$?"
   ;;
 esac
 [ "$RC" = "0" ] && echo "Fertig." || echo "Fehler." >&2
 cd / ; mountcache "$1" -r
 return "$RC"
}

# download server file [important]
download(){
 local RC=1
 [ -n "$3" ] && echo "RSYNC Download $1 -> $2..."
 rm -f "$TMP"
 interruptible rsync -HaLz --partial "$1::linbo/$2" "$2" 2>"$TMP"; RC="$?"
 if [ "$RC" != "0" ]; then
  # Delete incomplete/defective/non-existent file (maybe we should check for returncde=23 first?)
  rm -f "$2" 2>/dev/null
  if [ -n "$3" ]; then
   # Verbose error message if file was important
   cat "$TMP" >&2
   echo "Datei $2 konnte nicht heruntergeladen werden." >&2
  fi
 fi
 rm -f "$TMP"
 return "$RC"
}

# getinfo file key
getinfo(){
 [ -f "$1" ] || return 1
 while read line; do
  local key="${line%%=*}"
  if [ "$key" = "$2" ]; then
   echo "${line#*=}"
   return 0
  fi
 done <"$1"
 return 1
}

# mk_info imagename baseimagename device_size image_size - creates timestamp file
mk_info(){
 echo "[$1 Info File]
timestamp=$(date +%Y%m%d%H%M)
image=$1
baseimage=$2
partitionsize=$3
imagesize=$4"
}

#check_torrent_complete image
check_torrent_complete(){
 local image="$1"
 local complete="$image".complete
 local logfile=/tmp/"$image".log
 local RC=1
 [ -e "$logfile" ] || return "$RC"
 if [ -e "$complete" ]; then
  RC=0
 else
  grep "$image" "$logfile" | grep -q "(100%)" && { touch "$complete"; RC=0; }
 fi
 return "$RC"
}

# torrent_watchdog image timeout
torrent_watchdog(){
 local image="$1"
 local torrent="$image".torrent
 local logfile=/tmp/"$image".log
 local timeout="$2"
 local line=""
 local line_old=""
 local pid=""
 local int=10
 local RC=1
 local c=0
 while [ $c -lt $timeout ]; do
  sleep $int
  # check if torrent is complete
  check_torrent_complete "$image" && { RC=0; break; }
  line="$(tail -1 "$logfile" | awk '{ print $3 }')"
  [ -z "$line_old" ] && line_old="$line"
  if [ "$line_old" = "$line" ]; then
   [ $c -eq 0 ] || echo "Torrent-Überwachung: Download von $image hängt seit $c Sekunden." >&2
   c=$(($c+$int))
  else
   line_old="$line"
   c=0
  fi
 done
 if [ "$RC" = "1" ]; then
  echo "Download von $image wurde wegen Zeitüberschreitung abgebrochen." >&2
  pid="$(ps w | grep ctorrent | grep "$torrent" | grep -v grep | awk '{ print $1 }')"
  [ -n "$pid" ] && kill "$pid"
 fi
 pid="$(ps w | grep "tail -f $logfile" | grep -v grep | awk '{ print $1 }')"
 [ -n "$pid" ] && kill "$pid"
 echo
 if [ "$RC" = "0" ]; then
  echo "Image $image wurde komplett heruntergeladen. :-)"
 fi
 return "$RC"
}

# download_torrent_check image opts
download_torrent_check() {
 local image="$1"
 local OPTS="$2"
 local complete="$image".complete
 local torrent="$image".torrent
 local logfile=/tmp/"$image".log
 local RC=1
 ctorrent -dd -X "touch $complete" $OPTS "$torrent" > "$logfile"
 tail -f "$logfile"
 [ -e "$complete" ] && RC=0
 rm -f "$logfile"
 return "$RC"
}

# download_torrent image
download_torrent(){
 local image="$1"
 local torrent="$image".torrent
 local complete="$image".complete
 local RC=1
 [ -e "$torrent" ] || return "$RC"
 local ip="$(ip)"
 [ -z "$ip" -o "$ip" = "OFFLINE" ] && return "$RC"
 # default values
 local MAX_INITIATE=40
 local MAX_UPLOAD_RATE=0
 local SLICE_SIZE=128
 local TIMEOUT=300
 local pid=""
 [ -e /torrent-client.conf ] && . /torrent-client.conf
 [ -n "$DOWNLOAD_SLICE_SIZE" ] && SLICE_SIZE=$(($DOWNLOAD_SLICE_SIZE/1024))
 local pid="$(ps w | grep ctorrent | grep "$torrent" | grep -v grep | awk '{ print $1 }')"
 [ -n "$pid" ] && kill "$pid"
 local OPTS="-e 10000 -I $ip -M $MAX_INITIATE -z $SLICE_SIZE"
 [ $MAX_UPLOAD_RATE -gt 0 ] && OPTS="$OPTS -U $MAX_UPLOAD_RATE"
 echo "Torrent-Optionen: $OPTS"
 echo "Starte Torrent-Dienst für $image."
 if [ -e "$complete" ]; then
  ctorrent -f -d $OPTS "$torrent"
 else
  rm -f "$image" "$torrent".bf
  torrent_watchdog "$image" "$TIMEOUT" &
  interruptible download_torrent_check "$image" "$OPTS"
 fi
 [ -e "$complete" ] && RC=0
 return "$RC"
}


# Download main file and supplementary files
# download_all server mainfile additional_files...
download_all(){
 local RC=0
 local server="$1"
 download "$server" "$2" important; RC="$?"
 if [ "$RC" != "0" ]; then
  rm -f "$2"
  return "$RC"
 fi
 shift; shift;
 local file=""
 for file in "$@"; do
  download "$server" "$file"
 done
 return "$RC"
}

# Download info files, compare timestamps
# download_if_newer server file downloadtype
download_if_newer(){
 # do not execute in localmode
 localmode && return 0
 local DLTYPE="$3"
 [ -z "$DLTYPE" ] && DLTYPE="$(downloadtype)"
 [ -z "$DLTYPE" ] && DLTYPE="rsync"
 local RC=0
 local DOWNLOAD_ALL=""
 local IMAGE=""
 case "$2" in *.[Cc][Ll][Oo][Oo][Pp]|*.[Rr][Ss][Yy][Nn][Cc]) IMAGE="true" ;; esac
 if [ ! -s "$2" -o ! -s "$2".info ]; then # File not there, download all
  DOWNLOAD_ALL="true"
 else
  mv -f "$2".info "$2".info.old 2>/dev/null
  download "$1" "$2".info
  if [ -s "$2".info ]; then
   local ts1="$(getinfo "$2".info.old timestamp)"
   local ts2="$(getinfo "$2".info timestamp)"
   local fs1="$(getinfo "$2".info imagesize)"
   local fs2="$(get_filesize "$2")"
   if [ -n "$ts1" -a -n "$ts2" -a "$ts1" -gt "$ts2" ] >/dev/null 2>&1; then
    DOWNLOAD_ALL="true"
    echo "Server enthält eine neuere ($ts2) Version von $2 ($ts1)."
   elif  [ -n "$fs1" -a -n "$fs2" -a ! "$fs1" -eq "$fs2" ] >/dev/null 2>&1; then
    DOWNLOAD_ALL="true"
    echo "Dateigröße von $2 ($fs1) im Cache ($fs2) stimmt nicht."
   fi
   rm -f "$2".info.old
  else
   DOWNLOAD_ALL="true"
   mv -f "$2".info.old "$2".info
  fi
 fi
 # check for complete flag
 [ -z "$DOWNLOAD_ALL" -a -n "$IMAGE" -a ! -e "$2.complete" ] && DOWNLOAD_ALL="true"
 # supplemental torrent check
 if [ -n "$IMAGE" ]; then
  # save local torrent file
  [ -e "$2.torrent" -a -z "$DOWNLOAD_ALL" ] && mv "$2".torrent "$2".torrent.old
  # download torrent file from server
  download_all "$1" "$2".torrent ; RC="$?"
  if [ "$RC" != "0" ]; then
   echo "Download von $2.torrent fehlgeschlagen." >&2
   return "$RC"
  fi
  # check for updated torrent file
  if [ -e "$2.torrent.old" ]; then
   cmp "$2".torrent "$2".torrent.old || DOWNLOAD_ALL="true"
   rm "$2".torrent.old
  fi
  # update regpatch and postsync script
  rm -rf "$2".reg "$2".postsync
  download_all "$1" "$2".reg "$2".postsync >/dev/null 2>&1
 fi
 # start torrent service for others if there is no image to download
 [ "$DLTYPE" = "torrent" -a -n "$IMAGE" -a -z "$DOWNLOAD_ALL" ] && download_torrent "$2"
 # download because newer file exists on server
 if [ -n "$DOWNLOAD_ALL" ]; then
  if [ -n "$IMAGE" ]; then
   # remove complete flag
   rm -f "$2".complete
   # download images according to downloadtype torrent or multicast
   case "$DLTYPE" in
    torrent)
     # remove old image and torrents before download starts
     rm -f "$2" "$2".torrent.bf
     download_torrent "$2" ; RC="$?"
     [ "$RC" = "0" ] ||  echo "Download von $2 per torrent fehlgeschlagen!" >&2
    ;;
    multicast)
     if [ -s /multicast.list ]; then
      local MPORT="$(get_multicast_port "$2")"
      if [ -n "$MPORT" ]; then
       download_multicast "$1" "$MPORT" "$2" ; RC="$?"
      else
       echo "Konnte Multicast-Port nicht bestimmen, kein Multicast-Download möglich." >&2
       RC=1
      fi
     else
      echo "Datei multicast.list nicht gefunden, kein Multicast-Download möglich." >&2
      RC=1
     fi
     [ "$RC" = "0" ] || echo "Download von $2 per multicast fehlgeschlagen!" >&2
    ;;
   esac
   # download per rsync also as a fallback if other download types failed
   if [ "$RC" != "0" -o "$DLTYPE" = "rsync" ]; then
    [ "$RC" = "0" ] || echo "Versuche Download per rsync." >&2
    download_all "$1" "$2" ; RC="$?"
    [ "$RC" = "0" ] || echo "Download von $2 per rsync fehlgeschlagen!" >&2
   fi
   # download supplemental files and set complete flag if image download was successful
   if [ "$RC" = "0" ]; then
    # reg und postsync were downloaded already above
#    download_all "$1" "$2".info "$2".desc "$2".reg "$2".postsync >/dev/null 2>&1
    download_all "$1" "$2".info "$2".desc >/dev/null 2>&1
    touch "$2".complete
   fi
  else # download other files than images
   download_all "$1" "$2" "$2".info ; RC="$?"
   [ "$RC" = "0" ] || echo "Download von $2 fehlgeschlagen!" >&2
  fi
 else # download nothing, no newer file on server
  echo "Keine neuere Version vorhanden, überspringe $2."
 fi
 return "$RC"
}

# Authenticate server user password share
authenticate(){
 local RC=1
 localmode "$1"; RC="$?"
 if [ "$RC" = "1" ]; then
  export RSYNC_PASSWORD="$3"
  echo "Logge $2 ein auf $1..."
  rm -f "$TMP"
  rsync "$2@$1::linbo-upload" >/dev/null 2>"$TMP" ; RC="$?"
 elif [ -e /etc/linbo_passwd ]; then
  echo "Authentifiziere offline ..."
  md5passwd="$(echo -n "$3" | md5sum | awk '{ print $1 }')"
  linbo_md5passwd="$(cat /etc/linbo_passwd)"
  if [ "$md5passwd" = "$linbo_md5passwd" ]; then
   RC=0
  else
   echo 'Passt nicht!' >"$TMP" ; RC=1
  fi
 else
  echo 'Kann nicht lokal authentifizieren!' >"$TMP" ; RC=1
 fi
 if [ "$RC" != "0" ]; then
  echo "Fehler: $(cat "$TMP")" >&2
  echo "Falsches Passwort oder fehlende Passwortdatei?" >&2
 else
  echo "Passwort OK."
  # temporary workaround for password
  echo -n "$RSYNC_PASSWORD" > /tmp/linbo.passwd
 fi
 rm -f "$TMP"
 return "$RC"
}

# upload server user password cache file
upload(){
 # do not execute in localmode
 localmode && return 0
 local RC=0
 local file
 local ext
 if remote_cache "$4"; then
  echo "Cache $4 ist nicht lokal, die Datei $5 befindet sich" >&2
  echo "höchstwahrscheinlich bereits auf dem Server, daher kein Upload." >&2
  return 1
 fi
 # We may need this password for mountcache as well!
 export RSYNC_PASSWORD="$3"
 mountcache "$4" || return "$?"
 cd /cache
 if [ -s "$5" ]; then
  local FILES="$5"
  for ext in info list reg desc torrent; do
   [ -s "${5}.${ext}" ] && FILES="$FILES ${5}.${ext}"
  done
  echo "Uploade $FILES auf $1..."
  for file in $FILES; do
   rm -f "$TMP"
   interruptible rsync -Ha $RSYNC_PERMISSIONS "$file" "$2@$1::linbo-upload/$file" 2>"$TMP" ; RC="$?"
   if [ "$RC" != "0" ]; then
    cat "$TMP" >&2
    rm -f "$TMP"
    break
   else
    # start torrent service for image
    case "$file" in
     *.torrent) [ -n "`ps w | grep ctorrent | grep "$file" | grep -v grep`" ] || download_torrent "${file%.torrent}" ;;
    esac
   fi
   rm -f "$TMP"
  done
 else
  RC=1
  echo "Die Datei $5 existiert nicht, und kann daher nicht hochgeladen werden." >&2
 fi
 if [ "$RC" = "0" ]; then
  echo "Upload von $FILES nach $1 erfolgreich." | tee -a /tmp/linbo.log
 else
  echo "Upload von $FILES nach $1 ist fehlgeschlagen." | tee -a /tmp/linbo.log
 fi
 cd / ; mountcache "$4" -r
 [ "$RC" = "0" ] && echo "Upload von $FILES nach $1 erfolgreich." || echo "Upload von $FILES nach $1 ist fehlgeschlagen." >&2
 return "$RC"
}

# Sync from server
# syncr server cachedev baseimage image bootdev rootdev kernel initrd append [force]
syncr(){
 echo -n "syncr " ; printargs "$@"
 if remote_cache "$2"; then
  echo "Cache $2 ist nicht lokal, überspringe Aktualisierung der Images."
 else
  mountcache "$2" || return "$?"
  cd /cache
  local i
  for i in "$3" "$4"; do
   [ -n "$i" ] && download_if_newer "$1" "$i"
#   rm -f "$i".reg 2>/dev/null ; download "$1" "$i".reg >/dev/null 2>&1
  done
  cd / ; mountcache "$2" -r
  # Also update LINBO, while we are here.
  update "$1" "$2"
 fi
 shift 
 syncl "$@"
}

# get_multicast_server file
get_multicast_server(){
 local file=""
 local serverport=""
 local relax=""
 while read file serverport relax; do
  if [ "$file" = "$1" ]; then
   echo "${serverport%%:*}"
   return 0
  fi
 done <multicast.list
 return 1
}

# get_multicast_port file
get_multicast_port(){
 local file=""
 local serverport=""
 local relax=""
 while read file serverport relax; do
  if [ "$file" = "$1" ]; then
   echo "${serverport##*:}"
   return 0
  fi
 done <multicast.list
 return 1
}

# download_multicast server port file
download_multicast(){
 local interface="$(route -n | tail -1 | awk '/^0.0.0.0/{print $NF}')"
 echo "MULTICAST Download $interface($1):$2 -> $3"
 echo "udp-receiver --nosync --nokbd --interface $interface --rcvbuf 4194304 --portbase $2 --file $3"
 interruptible udp-receiver --nosync --nokbd --interface "$interface" --rcvbuf 4194304 --portbase "$2" --file "$3"
 return "$?"
}

# tschmitt
# fetch hostgroup from start.conf
hostgroup(){
 local hostgroup=""
 [ -s /start.conf ] || return 1
 hostgroup=`grep -m1 ^Group /start.conf | awk -F= '{ print $2 }' | awk '{ print $1 }'`
 echo "$hostgroup"
}

# update server cachedev
update(){
 echo -n "update " ;  printargs "$@"
 local RC=0
 local group="$(hostgroup)"
 local server="$1"
 local cachedev="$2"
 local disk="${cachedev%%[1-9]*}"
 mountcache "$cachedev" || return "$?"
 cd /cache
 # For local startup, copy /start.conf to cache, if no newer version exists.
 [ ! -r start.conf -o /start.conf -nt start.conf ] 2>/dev/null && cp -f /start.conf .
 echo "Suche nach LINBO-Updates auf $1."
 download_if_newer "$server" grub.exe
 local linbo_ts1="$(getinfo linbo.info timestamp)"
 local linbo_fs1="$(get_filesize linbo)"
 download_if_newer "$server" linbo
 local linbo_ts2="$(getinfo linbo.info timestamp)"
 local linbo_fs2="$(get_filesize linbo)"
 local linbofs_ts1="$(getinfo linbofs.gz.info timestamp)"
 local linbofs_fs1="$(get_filesize linbofs.gz)"
 # tschmitt: download group specific linbofs
 [ -n "$group" ] && download_if_newer "$server" linbofs.$group.gz
 if [ -e "linbofs.$group.gz" ]; then
  rm linbofs.gz; ln linbofs.$group.gz linbofs.gz
  rm linbofs.gz.info; ln linbofs.$group.gz.info linbofs.gz.info
 else
  download_if_newer "$server" linbofs.gz
 fi
 local linbofs_ts2="$(getinfo linbofs.gz.info timestamp)"
 local linbofs_fs2="$(get_filesize linbofs.gz)"
 # tschmitt: update grub on every synced start not only if newer linbo is available
 # if [ "$disk" -a -n "$cachedev" -a -s "linbo" -a -s "linbofs.gz" ] && \
 #    [ "$linbo_ts1" != "$linbo_ts2" -o "$linbo_fs1" != "$linbo_fs2" -o \
 #      "$linbofs_ts1" != "$linbofs_ts2" -o "$linbofs_fs1" != "$linbofs_fs2" ]; then
 if [ -b "$disk" -a -n "$cachedev" -a -s "linbo" -a -s "linbofs.gz" ]; then
  echo "Update Master-Bootrecord von $disk."
  local append=""
  local vga="vga=785"
  local i
  for i in $(cat /proc/cmdline); do
   case "$i" in
    vga=*) vga="$i" ;; 
    BOOT_IMAGE=*|server=*|cache=*) true ;;
    *) append="$append $i" ;;
   esac
  done
  mkdir -p /cache/boot/grub
  # tschmitt: provide custom local menu.lst
  download "$server" "menu.lst.$group"
  if [ -e "/cache/menu.lst.$group" ]; then
   mv "/cache/menu.lst.$group" /cache/boot/grub/menu.lst
  else
   [ -e /cache/boot/grub/menu.lst ] && rm /cache/boot/grub/menu.lst
   mkgrubmenu "$cachedev" "linbo" "linbofs.gz" "$server" "$append"
  fi
  # tschmitt: grub is installed on every update
  grub-install --root-directory=/cache "$disk"
 fi
 RC="$?"
 cd / ; mountcache "$cachedev" -r
 [ "$RC" = "0" ] && echo "LINBO update fertig." || echo "Lokale Installation von LINBO hat nicht geklappt." >&2
 return "$RC"
}

# initcache server cachedev downloadtype images...
initcache(){
 # do not execute in localmode
 localmode && return 0
 echo -n "initcache " ;  printargs "$@"
 local RC=0
 local server="$1"
 local cachedev="$2"
 local download_type="$3"
 local i
 local ext
 local u
 local used_images
 local group
 local found
 if remote_cache "$cachedev"; then
  echo "Cache $cachedev ist nicht lokal, und muss daher nicht aktualisiert werden."
  return 1
 fi
 if [ -n "$FORCE_FORMAT" ]; then
  local cachefs="$(fstype_startconf "$cachedev")"
  if [ -n "$cachefs" ]; then
   echo "Formatiere Cache-Partition $cachedev..."
   format "$cachedev" "$cachefs"
  fi
 fi
 mountcache "$cachedev" || return "$?"
 cd /cache
 shift; shift; shift

 # clean up obsolete linbofs files
 rm -f linbofs[.a-zA-Z0-9_-]*.gz*

 # clean up obsolete image files
 used_images="$(grep -i ^baseimage /start.conf | awk -F\= '{ print $2 }' | awk '{ print $1 }')"
 used_images="$used_images $(grep -i ^image /start.conf | awk -F\= '{ print $2 }' | awk '{ print $1 }')"
 for i in *.cloop *.rsync; do
  [ -e "$i" ] || continue
  found=0
  for u in $used_images; do
   if [ "$i" = "$u" ]; then
    found=1
    break
   fi
  done
  if [ "$found" = "0" ]; then
   echo "Entferne nicht mehr benötigte Imagedatei $i."
   rm -f "$i" "$i".*
  fi
 done

 for i in "$@"; do
  if [ -n "$i" ]; then
   download_if_newer "$server" "$i" "$download_type"
  fi
 done
 cd / ; mountcache "$cachedev" -r
 update "$server" "$cachedev"
}

### Main ###
# DEBUG linbo_gui:
# echo -n "Running: $cmd "
# count=1
# for i in "$@"; do
#  echo -n "$((count++))=$i,"
# done
# echo ""
# sleep 1

# readfile cachepartition filename [destinationfile]
readfile(){
 local RC=0
 mountcache "$1" -r || return "$?"
 if [ -n "$3" ]; then
  cp -a /cache/"$2" "$3"
 else
  cat /cache/"$2"
 fi
 RC="$?"
 # umount /cache
 # Leave cache mounted read-only
 return "$RC"
}

# writefile cachepartition filename [sourcefile]
writefile(){
 local RC=0
 mountcache "$1" -w || return "$?"
 if cache_writable; then
  if [ -n "$3" ]; then
   cp -a "$3" /cache/"$2"
  else
   cat > /cache/"$2"
  fi
  RC="$?"
 else
  echo "Cache ist nicht schreibbar, Datei $2 nicht gespeichert." >&2
  RC=1
 fi
 mountcache "$1" -r
 return "$RC"
}

# ready - check if LINBO is ready (timeout 120 seconds)
ready(){
 # Files /tmp/linbo-network.done and /tmp/linbo-cache.done created by init.sh
 local count=0
 while [ ! -e /tmp/linbo-network.done -o ! -e /tmp/linbo-cache.done -o ! -s start.conf ]; do
  sleep 1
#  echo -n "."
  count=`expr $count + 1`
  if [ "$count" -gt 120 ]; then
   echo "Timeout, LINBO ist nicht bereit. :-(" >&2
   return 1
  fi
 done
 localmode || echo "Netzwerk OK."
 echo "Lokale Disk(s) OK."
 return 0
}

# register server user password variables...
register(){
 local RC=1
 local room="$4"
 local client="$5"
 local ip="$6"
 local group="$7"
 local device="$(route -n | awk '/^0.0.0.0/{print $NF; exit}')"
 [ -n "$device" ] && device="eth0"
 local mac="$(cat /sys/class/net/$device/address)"
 local info="$room;$client;$group;$mac;$ip;255.240.0.0;1;1;1;1;22"
 # Plausibility check
 if echo "$client" | grep -qi '[^a-z0-9-]'; then
  echo "Falscher Rechnername: '$client'," >&2
  echo "Rechnernamen dürfen nur Buchstaben [a-z0-9-] enthalten." >&2
  return 1
 fi
 if echo "$group" | grep -qi '[^a-z0-9_]'; then
  echo "Falscher Gruppenname: '$group'," >&2
  echo "Rechnergruppen dürfen nur Buchstaben [a-z0-9_] enthalten." >&2
  return 1
 fi
 cd /tmp
 echo "$info" '>' "$client.new"
 echo "$info" >"$client.new"
 echo "Uploade $client.new auf $1..."
 export RSYNC_PASSWORD="$3"
 interruptible rsync -HaP "$client.new" "$2@$1::linbo-upload/$client.new" ; RC="$?"
 cd /
 return "$RC"
}

ip(){
 ifconfig "$(grep eth /proc/net/route | sort | head -n1 | awk '{print $1}')" | grep 'inet\ addr' | awk '{print $2}' | awk 'BEGIN { FS = ":" }; {print $2}'
}

clientname(){
 if localmode; then
  local cachedev="$(grep ^Cache /start.conf | awk -F\= '{ print $2 }' | awk '{ print $1 }')"
  if [ -n "$cachedev" ]; then
   if mountcache $cachedev; then
    if [ -s /cache/hostname ]; then
     cat /cache/hostname
     mountcache "$cachedev" -r
     return 0
    fi
    mountcache "$cachedev" -r
   fi
  fi
 fi
 echo `hostname`
}

mac(){
 ifconfig "$(grep eth /proc/net/route | sort | head -n1 | awk '{print $1}')" | grep HWaddr | awk '{print $5}'
}

cpu(){
 cat /proc/cpuinfo | grep name | sed 's,model.*:\ ,,'
}

memory(){
 free | grep Mem | awk '{printf "%d MB\n",$2 / 1024}'
}

size(){
 if mountpart "$1" /mnt -r 2>/dev/null; then
  df -k /mnt 2>/dev/null | tail -1 | \
   awk '{printf "%.1f/%.1fGB\n", $4 / 1048576, $2 / 1048576}' 2>/dev/null
  umount /mnt
 else
  local d=$(sfdisk -s $1 2>/dev/null)
  if [ "$?" = "0" -a "$d" -ge 0 ] 2>/dev/null; then
   echo "$d" | awk '{printf "%.1fGB\n",$1 / 1048576}' 2>/dev/null
  else
   echo " -- "
  fi
 fi
 return 0
}

version(){
 local versionfile="/etc/linbo-version"
 if [ -s "$versionfile" ]; then
  cat "$versionfile"
 else
  echo "LINBO 2.00"
 fi
}

# Main

case "$cmd" in
 ip) ip ;;
 hostname) clientname ;;
 cpu) cpu ;;
 memory) memory ;;
 mac) mac ;;
 size) size "$@" ;;
 authenticate) authenticate "$@" ;;
 create) create "$@" ;;
 start) start "$@" ;;
 partition_noformat) export NOFORMAT=1; partition "$@" ;;
 partition) partition "$@" ;;
 initcache) initcache "$@" ;;
 initcache_format) echo "initcache_format gestartet."; export FORCE_FORMAT=1; initcache "$@" ;;
 mountcache) mountcache "$@" ;;
 readfile) readfile "$@" ;;
 ready) ready "$@" ;;
 register) register "$@" ;;
 sync) syncl "$@" && { cache="$1"; shift 3; start "$1" "$2" "$3" "$4" "$5" "$cache"; } ;;
 syncstart) syncr "$@" && { cache="$2"; shift 4; start "$1" "$2" "$3" "$4" "$5" "$cache"; } ;;
 syncr) syncr "$@" && { cache="$2"; shift 4; start "$1" "$2" "$3" "$4" "$5" "$cache"; } ;;
 synconly) syncr "$@" ;;
 update) update "$@" ;;
 upload) upload "$@" ;;
 version) version ;;
 writefile) writefile "$@" ;;
 *) help "$cmd" "$@" ;;
esac

# Return returncode
exit $?
