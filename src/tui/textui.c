/*
256boss - bootable launcher for 256byte intros
Copyright (C) 2018-2019  John Tsiombikas <nuclear@member.fsf.org>

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
#include "textui.h"
#include "video.h"
#include "contty.h"
#include "keyb.h"
#include "asmops.h"

int textui(void)
{
	set_vga_mode(3);
	con_show_cursor(0);
	con_clear();

	con_bgcolor(CON_BLUE);
	con_printf(0, 0, "%80s", "256boss");

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			switch(c) {
			case KB_F8:
				goto end;
			}
		}
	}

end:
	return 0;
}
