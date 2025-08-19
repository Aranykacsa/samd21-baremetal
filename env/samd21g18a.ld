/* ATSAMD21G18A — NO BOOTLOADER OFFSET */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

MEMORY {
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00040000  /* 256 KB */
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00008000  /* 32 KB  */
}

SECTIONS
{
  /* --- code + const in FLASH (vectors at 0x00000000) --- */
  .text :
  {
    _stext = .;
    KEEP(*(.isr_vector))               /* vector table */
    *(.text .text.* .gnu.linkonce.t.*)
    *(.glue_7t) *(.glue_7)
    KEEP(*(.init)) KEEP(*(.fini))
    *(.rodata .rodata.* .gnu.linkonce.r.*)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  /* unwind tables in FLASH BEFORE .data’s LMA */
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
    . = ALIGN(4);
  } > FLASH

  /* .data runs in RAM, loads from FLASH right after exidx */
  .data : AT (ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx))
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
  } > RAM
  _sidata = LOADADDR(.data);

  /* zero-initialized */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  _end = .;
}
