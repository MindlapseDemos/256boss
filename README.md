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

256boss latest release (v0.1) github mirror:
  - https://github.com/MindlapseDemos/releases/download/v0.1/256boss-0.1.zip
  - https://github.com/MindlapseDemos/releases/download/v0.1/256boss-0.1.tar.gz

Although the official releases do contain the full source code, if you intend to
contribute to 256boss, you should get the latest code directly from our git
repository on github:
  - https://github.com/MindlapseDemos/256boss

Setup instructions
------------------
The 256boss release comes with a pre-generated bootable disk image, ready to be
written onto a USB stick (if you got the source code from github instead of an
"official" release, follow the build instructions in the next section first).
The disk image contains both the boot loader and operating system itself, as well as
a small FAT partition where you can store your 256-byte intros to access them
when you boot 256boss.

To install from Windows, insert a USB stick, and run the `install.bat` batch
file. This will start the bundled "rufus" program with the 256boss disk image
already loaded. Just make sure the correct USB device is selected and click
"start".

To install from GNU/Linux, insert a USB stick, and identify which device file
corresponds to it by running `lsblk`, or monitoring `dmesg`.  For the purposes
of this example we'll assume the USB stick device file is `/dev/sdc`; make sure
you replace the correct one. Run `dd if=disk.img of=/dev/sdc bs=1M` to install.

Reboot to start 256boss, and make sure to boot from the USB stick. Some
computers will boot from the USB stick automatically, while others require you
to press a key (usually `ESC`, `F8`, or `F12`) to pop up a boot selection menu.
Watch the screen during POST, where a message will most likely appear
instructing you which key to press. If you're still having trouble booting from
the USB stick, make sure *legacy boot* is enabled in your BIOS settings.

Build instructions
------------------
Just run `make`.

To test the build in an emulator, make sure qemu for i386 is installed, and
invoke `make run`.

If you're trying to build the source code from git, make sure to first copy over
the `data` directory from an official release.

Tools compiled as part of the build process require `libpng` and `zlib` to be
installed.

BUGS
----
You will most likely encounter two forms of bugs:
  - 256boss will fail to boot on some computers.
  - 256boss will fail to run certain 256-byte intros correctly.

Ideally I would like to eliminate both of these problems. And while I can't
promise that 256boss will ever be 100% compatible with every computer and every
intro out there, I would appreciate reports of any such bugs you encounter,
either by email, or even better by opening an issue in the project issue tracker
on github: https://github.com/MindlapseDemos/256boss/issues

For computer-compatibility bugs make sure to include as much information as
possible about the nature of the problem, and about your computer. Most relevant
are going to be computer manufacturer and model (if applicable), CPU,
motherboard, and BIOS version.

For intro-compatibility bugs, first make sure the problem persists even when the
intro in question is executed directly after booting into 256boss, without
having ran other intros previously. This is important, because prior intro
executions might lead to subtle corruption that will make a subsequent intro (or
the operating system itself) fail. If possible, try to run the intro in MS-DOS
or FreeDOS on the same computer, to make sure the bug is in 256boss and not in
the intro itself.

NOT BUGS
--------
If an intro fails to run correctly under MS-DOS or FreeDOS in a certain
computer, it will probably also fail to run under 256boss.

If an intro needs an adlib card to play music, and your computer doesn't have an
adlib card, 256boss will not emulate one. You need the hardware the intro is
written for; 256boss is not an emulator like dosbox.

If a computer is UEFI-only and cannot boot in BIOS (or "legacy" mode), 256boss
will not boot. It's unlikely for this limitation to be lifted any time soon.
