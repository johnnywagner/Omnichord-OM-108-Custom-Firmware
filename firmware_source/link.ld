SECTIONS {
  . = 0x6003A000;
  .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) }
  .bss : { *(.bss*) *(.data*) }
}
