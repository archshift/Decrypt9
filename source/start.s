.section .data
.balign 4
run_once: .word 1

.section .text.start
.arm
.global _start
.type _start, %function

_start:
   //DO NOT EDIT THIS!
   .word 0xE51FF004 
   .word loop 
   .word 0xE51FF004 
   .word loop
   .word 0xE51FF004 
   .word loop
   .word 0xE51FF004 
   .word loop
   .word 0xE51FF004 
   .word loop
   .word 0xE51FF004 
   .word loop
   //!!!	
   
	ldr sp, =0x24000000
				
    bl main
	
loop:
    b loop