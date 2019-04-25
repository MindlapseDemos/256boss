# pcboot - bootable PC demo/game kernel
# Copyright (C) 2018  John Tsiombikas <nuclear@member.fsf.org>
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

	.code32
	.section .startup,"ax"

	.extern _bss_start
	.extern _bss_end
	.extern pcboot_main
	.extern wait_vsync
	.extern kb_getkey

	# move the stack to the top of the conventional memory
	cli
	movl $0x80000, %esp

	# zero the BSS section
	xor %eax, %eax
	mov $_bss_start, %edi
	mov $_bss_size, %ecx
	cmp $0, %ecx
	jz skip_bss_zero
	shr $4, %ecx
	rep stosl
skip_bss_zero:

	call pcboot_main
	# pcboot_main never returns
0:	cli
	hlt
	jmp 0b
