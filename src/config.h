/*
pcboot - bootable PC demo/game kernel
Copyright (C) 2018  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY, without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef PCBOOT_CONFIG_H_
#define PCBOOT_CONFIG_H_

/* frequency of generated timer ticks in hertz */
#define TICK_FREQ_HZ		250

#define CON_TEXTMODE
#define CON_SERIAL

#define AUTOSTART_GUI

/* allocate 64k for stack */
#define STACK_PAGES		16

#endif	/* PCBOOT_CONFIG_H_ */
