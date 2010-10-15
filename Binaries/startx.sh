#!/bin/sh
# This is a busybox 1.1.3 startx script
# (C) Klaus Knopper 2006
# License: GPL V2

OPTIONS="-nolisten tcp -noreset -br"

echo "Starting X."

ifconfig lo 127.0.0.1
Xorg $OPTIONS :0 &

export DISPLAY=":0"

for i in 1 2 3 4 5 6 7 8 9 10; do
 xsetroot -cursor_name left_ptr  && break
 sleep 1
done

while rxvt -geometry 166x10+0-1 -display :0 ; do sleep 1; done &
