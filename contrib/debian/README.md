
Debian
====================
This directory contains files used to package phantomriderd/phantomrider-qt
for Debian-based Linux systems. If you compile phantomriderd/phantomrider-qt yourself, there are some useful files here.

## dash: URI support ##


phantomrider-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install phantomrider-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your phantomrider-qt binary to `/usr/bin`
and the `../../share/pixmaps/dash128.png` to `/usr/share/pixmaps`

phantomrider-qt.protocol (KDE)

