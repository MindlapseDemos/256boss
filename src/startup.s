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
	.extern kmain
	.extern wait_vsync
	.extern kb_getkey

	.equ STACKTOP,0x80000

	# move the stack to the top of the conventional memory
	cli
	movl $STACKTOP, %esp
	# push a 0 ret-addr to terminate gdb backtraces
	pushl $0

	# zero the BSS section
	xor %eax, %eax
	mov $_bss_start, %edi
	mov $_bss_size, %ecx
	cmp $0, %ecx
	jz skip_bss_zero
	shr $2, %ecx
	rep stosl
skip_bss_zero:

	call kmain
	# kmain never returns
0:	cli
	hlt
	jmp 0b

	# this is called once after memory init, to move the protected mode
	# stack to the top of usable memory, to avoid interference from 16bit
	# programs (as much as possible)
	.global move_stack
move_stack:
	# calculate the currently used lowest address of the stack (rounded
	# down to 4-byte alignment), to see where to start copying
	mov %esp, %esi
	and $0xfffffffc, %esi
	# calculate the size we need to copy
	mov $STACKTOP, %ecx
	sub %esi, %ecx
	# load the destination address to edi
	mov 4(%esp), %edi
	sub %ecx, %edi
	# size in longwords
	shr $2, %ecx

	# change esp to the new stack
	mov $STACKTOP, %ecx
	sub %esp, %ecx
	mov 4(%esp), %eax
	mov %eax, %esp
	sub %ecx, %esp

	rep movsd
	ret
