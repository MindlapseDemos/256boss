# 256boss - bootable launcher for 256byte intros
# Copyright (C) 2018-2019  John Tsiombikas <nuclear@member.fsf.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

	.section .lowtext,"ax"
	.code16
	.global dos_int21h_entry
dos_int21h_entry:
	cmp $0x4c, %ah
	jz exit
	iret

exit:	ljmp $0,$run_com_return
