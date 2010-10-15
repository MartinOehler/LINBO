#!/bin/bash

if [ ! -s "multicast.list" ]; then
 echo 'Keine multicast.list vorhanden, bitte erstellen Sie eine!' >&2
 echo 'Format:' >&2
 echo 'dateiname1 [server-adresse]:portbase1' >&2
 echo 'dateiname2 [server-adresse]:portbase2' >&2
 echo 'portbase muss eine gerade Zahl sein, z.B. 9000, 9002, ...' >&2
 return 0
fi

# 
bailout(){
 
 exit $1
}

MINCLIENTS="100"
read -e -p "Ab wie vielen Client-Anfragen soll die Übertragung [7msofort[0m starten? [$MINCLIENTS] " MINCLIENTS

MINSECONDS="60"
read -e -p "Wieviele Sekunden Wartezeit ab der [7mersten[0m Anfrage vergehen? [$MINSECONDS] " MINSECONDS

INTERFACE="$(route -n | tail -1 | awk '{print $NF}')"
[ -n "$INTERFACE" ] || INTERFACE="eth0"
read -e -p "Netzwerk-Interface für Multicast? [$INTERFACE] " INTERFACE

if [ -n "$MINCLIENTS" -a "$MINCLIENTS" -gt "0" ] >/dev/null 2>&1; then
 MINCLIENTS="--min-clients $MINCLIENTS"
else
 MINCLIENTS=""
fi

if [ -n "$MINSECONDS" -a "$MINSECONDS" -gt "0" ] >/dev/null 2>&1; then
 MINSECONDS="--max-wait $MINSECONDS"
else
 MINSECONDS=""
fi

echo "UDP-Server Parameter:" >&2
echo "	MinClients (--min-clients):	${MINCLIENTS:-nicht angegeben}" >&2
echo "	MaxWait (--max-wait):	${MINSECONDS:-nicht angegeben}" >&2
echo "	Interface (--interface):	$INTERFACE" >&2

read -e -p "EINGABE zum Starten ^C für Abbruch. "

while read file serverport relax; do
 port="${serverport##*:}"
 if [ -s "$file" ]; then
  echo "Starte udp-sender $file -> $INTERFACE:$port" >&2
  while true; do
   udp-sender --interface "$INTERFACE" --portbase $port $MINCLIENTS $MINSECONDS --file "$file" --nokbd || exit 1
  done &
 fi
done < multicast.list

