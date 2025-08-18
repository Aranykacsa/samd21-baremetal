/* ATSAMD21G18A memory map */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

MEMORY {
  FLASH (rx) : ORIGIN = 0x00002000, LENGTH = 0x00040000 - 0x2000   /* 256KB minus 8KB */
  RAM   (rwx): ORIGIN = 0x20000000, LENGTH = 0x00008000            /* 32KB */
}

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*) *(.rodata*) *(.glue_7) *(.glue_7t) *(.eh_frame*)
    KEEP(*(.init)) KEEP(*(.fini))
  } > FLASH

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  .data : AT (ADDR(.text) + SIZEOF(.text))
  {
    _sidata = LOADADDR(.data);
    _sdata  = .;
    *(.data*)
    _edata  = .;
  } > RAM

  .bss (NOLOAD) :
  {
    _sbss = .;
    *(.bss*) *(COMMON)
    _ebss = .;
  } > RAM

  . = ALIGN(4);
  _end = .;
}
