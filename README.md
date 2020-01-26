256boss - a bootable launcher for 256byte intros
================================================

About
-----
256boss is a standalone USB-bootable operating system for running 256byte intros
without having to install DOS. It's based on the pcboot project:
https://github.com/jtsiomb/pcboot

256byte intro compos rapidly devolve into an emulated category on recent
demoparties, making dosbox into its own platform, due to the disproportionate
hassle of setting up a computer with MS-DOS or FreeDOS, just for the task of
running 256byte intros.

As a result of this emulationization of 256byte compos, signs are appearing of
entries requiring specific dosbox "cycles" to run properly, or using a wide
variety of sound hardware emulated by dosbox, without regard for what a
realistic compo PC would have, and having to contend with that like every other
category.

We at mindlapse demos feel this to be a travesty, and cannot allow this slippery
slope to continue. For this reason we decided to create 256boss: a simple and
painless way to run 256byte intros on real computers, without any hassle other
than inserting a USB stick, rebooting, and possibly selecting the USB stick from
a boot menu.

Use it to watch 256byte releases at home, or use it to run a 256byte compo at a
demoparty. Just install 256boss on a USB stick, by following our simple
installation instructions for each platform, copy every 256byte intro you
want into it, and reboot.

Project website: http://nuclear.mutantstargoat.com/sw/256boss 
Source code repository: https://github.com/MindlapseDemos/256boss

License
-------
Copyright (C) 2019-2020 John Tsiombikas <nuclear@member.fsf.org>

This program is free software, feel free to use, modify and/or redistribute it
under the terms of the GNU General Public License version 3, or at your option
any later version published by the Free Software Foundation. See COPYING for
details.

Download
--------
Unless you intend to get involved in the development of 256boss, just grab the
latest "official" release, which includes everything you need to get up and
running.

The official releases are available in gzipped-tar, or zip archives. You just
need one or the other; they are identical. If you don't know which one to get,
you should probably get the zip file.

256boss latest release (v0.1):
  - http://nuclear.mutantstargoat.com/sw/256boss/256boss-0.1.zip
  - http://nuclear.mutantstargoat.com/sw/256boss/256boss-0.1.tar.gz

Although the official releases do contain the full source code, if you intend to
contribute to 256boss, you should get the latest code directly from our git
repository on github:
  - https://github.com/MindlapseDemos/256boss

Setup instructions
------------------
The 256boss release comes with a pre-generated bootable disk image, ready to be
written onto a USB stick (if you got the source code from github instead of an
"official" release, follow the build instructions in the next section first).
This image contains both the boot loader and operating system itself, as well as
a small FAT partition where you can write your 256byte intros to access them
when you boot 256boss.

TODO cont.

Build instructions
------------------
Just type make.
