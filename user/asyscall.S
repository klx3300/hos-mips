.set noreorder
.text
.globl _u_syscall_intr
.type _u_syscall_intr, @function
.ent _u_syscall_intr
_u_syscall_intr:
# sorry i use register k0, but it shouldn't matter..
move $k0, $a0
move $k1, $a1
# save
# v0 shall not save.
sw $a0, 4($k1)
sw $a1, 8($k1)
sw $a2, 12($k1)
sw $a3, 16($k1)
sw $t0, 20($k1)
# the sp trick
sw $sp, 24($k1)
move $sp, $k1
lw $v0, 0($k0)
lw $a0, 4($k0)
lw $a1, 8($k0)
lw $a2, 12($k0)
lw $a3, 16($k0)
lw $t0, 20($k0)
syscall
# restore
move $k1, $sp
lw $sp, 24($k1)
lw $t0, 20($k1)
lw $a3, 16($k1)
lw $a2, 12($k1)
lw $a1, 8($k1)
lw $a0, 4($k1)
# v0 shall not restore.
jr $ra
nop

.end _u_syscall_intr