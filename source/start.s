.section ".init"
.global _start
.extern main
.align 4
.arm


_start:
    bl main

.pool

loop:
    b loop
