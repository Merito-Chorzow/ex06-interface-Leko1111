/* boards/qemu-mps2/linker.ld */
ENTRY(Reset_Handler)

/* QEMU mps2-an385: FLASH at 0x0000_0000, SRAM at 0x2000_0000 */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1M
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Top of stack (used by vector table) */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  /* Vector table in FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  /* Text and read-only data in FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame*)
    . = ALIGN(4);
  } > FLASH

  /* Initialized data in RAM, with load image in FLASH */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  _sidata = LOADADDR(.data);

  /* Uninitialized data in RAM */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
	end = .;
  } > RAM

  _end = .;
}
