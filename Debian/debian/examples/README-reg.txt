Windows(tm) Registry-Patches fuer LINBO
---------------------------------------

* In der Patch-Datei f�r Windows 2000/XP (win2k-xp.reg)
  muss der Domaenenname angepasst werden.

* Ggf. k�nnen Patch-Dateien mit eigenen Registry-Eintr�gen
  erg�nzt werden.

* F�r jedes Windows-Image muss eine Patch-Datei nach diesen Mustern
  bereitgestellt werden:
  <ImageName>.cloop.reg
  <ImageName>.rsync.reg

* Die angepassten Patch-Dateien m�ssen unter /var/linbo abgelegt
  werden, damit sie von LINBO gefunden werden.


18.12.2007
Thomas Schmitt
<schmitt@lmz-bw.de>
