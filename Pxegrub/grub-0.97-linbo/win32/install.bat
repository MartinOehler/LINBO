ntfsinstall -d '(hd0,0)' -1 c:/tlinux3/stage1 -2 c:/tlinux3/stage2 -m '/tlinux3/menu.lst'

find "Topologilinux" C:\boot.ini

if ERRORLEVEL 1 goto install
goto skip_boot_ini

:install_boot_ini
echo "installing in boot.ini"
pause


attrib -r -h -s C:\boot.ini
copy C:\boot.ini C:\boot_ini.bak

echo C:\tlinux3\stage1="Topologilinux" >> C:\boot.ini
attrib +r +h +s C:\boot.ini

:skip_boot_ini

