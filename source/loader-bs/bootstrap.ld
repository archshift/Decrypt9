OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

__heap_size = 0x3F00000;
__stack_size = 0x100000;

MEMORY
{
    LOAD : ORIGIN = 0x23F00000, LENGTH = 128K
    RAM : ORIGIN = 0x24000000, LENGTH = 64M
}

SECTIONS
{
    . = 0x23F00000;

    .init ALIGN(4) : {
        __text_start = .;
        KEEP (*(.init))
    } >LOAD = 0xff

    .text ALIGN(4) : {
        *(.text .stub .text.* .gnu.linkonce.t.*)

        KEEP (*(.text.*personality*))

        *(.gnu.warning)
        *(.glue_7t) *(.glue_7)
    } >LOAD = 0xff

    .fini ALIGN(4) : {
        KEEP (*(.fini))
    } >LOAD =0xff

    __text_end = . ;

    .rodata ALIGN(4) : {
        *(.rodata)
        *all.rodata*(*)
        *(.roda)
        *(.rodata.*)
        *(.gnu.linkonce.r*)
    } >LOAD = 0xff

    . = ALIGN(4);
    .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >LOAD
    __exidx_start = .;
    .ARM.exidx   : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } >LOAD
    __exidx_end = .;

    . = ALIGN(4);
    PROVIDE (__preinit_array_start = .);
    .preinit_array  : { KEEP (*(.preinit_array)) } >LOAD = 0xff
    PROVIDE (__preinit_array_end = .);

    PROVIDE (__init_array_start = .);
    .init_array     : { KEEP (*(.init_array)) } >LOAD = 0xff
    PROVIDE (__init_array_end = .);

    PROVIDE (__fini_array_start = .);
    .fini_array     : { KEEP (*(.fini_array)) } >LOAD = 0xff
    PROVIDE (__fini_array_end = .);

    .ctors ALIGN(4) : {
        KEEP (*crtbegin.o(.ctors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
    } >LOAD = 0xff

    .dtors ALIGN(4) : {
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
    } >LOAD = 0xff

    .eh_frame ALIGN(4) : {
        KEEP (*(.eh_frame))
    } >LOAD = 0xff

    .gcc_except_table ALIGN(4) : {
        *(.gcc_except_table)
    } >LOAD = 0xff

    .data ALIGN(4) : {
        __data_start = ABSOLUTE(.);

        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d*)

        __data_end = ABSOLUTE(.) ;
    } >LOAD = 0xff

    .bss ALIGN(4) : {
        __bss_start = ABSOLUTE(.);
        __bss_start__ = ABSOLUTE(.);

        *(.dynbss)
        *(.gnu.linkonce.b*)
        *(.bss*)
        *(COMMON)

        __bss_end__ = ABSOLUTE(.);
    } >LOAD

    __end__ = ABSOLUTE(.);

    . = 0x24000000;
    __heap_start = .;
    . += __heap_size;

    . += __stack_size;
    __stack_top = .;
}
