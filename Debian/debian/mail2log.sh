#!/bin/bash
#
# read log mail from stdin
#
# 19.07.2008
# Thomas Schmitt
# <schmitt@lmz-bw.de>

sessiondate="$(date)"

while read line; do

	if echo "$line" | grep ^"Subject: LOG"; then
		logname="$(echo "$line" | awk '{ print $3 }')"
		if [ -z "$logname" ]; then
			echo "Cannot determine hostname!"
			exit 0
		fi
	fi

	if [ -z "$line" ]; then
		header_read=yes
		continue
	fi

	[ -z "$header_read" -o -z "$logname" ] && continue

	if [ -z "$date_printed" ]; then
		echo >> /var/log/linuxmuster/linbo/$logname.log
		echo "### New Session started at $sessiondate ###" >> /var/log/linuxmuster/linbo/$logname.log
		date_printed=yes
	fi

	echo "$line" >> /var/log/linuxmuster/linbo/$logname.log

done

