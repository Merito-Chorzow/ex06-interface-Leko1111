/*
* Poprawny plik startowy dla Cortex-M3 (QEMU MPS2)
* Inicjalizuje .data i .bss przed wywołaniem main()
*/
.syntax unified
.thumb

.section .isr_vector, "a", %progbits
.align 2
.globl g_pfnVectors
g_pfnVectors:
    .word   _estack             /* 0x00 Initial Stack Pointer (z linker.ld) */
    .word   Reset_Handler       /* 0x04 Reset Handler */
    .word   0                   /* 0x08 NMI Handler */
    .word   HardFault_Handler   /* 0x0C HardFault Handler */
    .word   0                   /* 0x10 MemManage */
    .word   0                   /* 0x14 BusFault */
    .word   0                   /* 0x18 UsageFault */
    .word   0                   /* 0x1C Reserved */
    .word   0                   /* 0x20 Reserved */
    .word   0                   /* 0x24 Reserved */
    .word   0                   /* 0x28 Reserved */
    .word   0                   /* 0x2C SVCall */
    .word   0                   /* 0x30 Debug Monitor */
    .word   0                   /* 0x34 Reserved */
    .word   0                   /* 0x38 PendSV */
    .word   0                   /* 0x3C SysTick */

.section .text
.thumb
.globl Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    /* 1. Skopiuj sekcję .data z FLASH do RAM */
    /* Załaduj adresy z linker.ld */
    ldr     r0, =_sidata    /* r0 = LMA (adres źródłowy) .data w FLASH */
    ldr     r1, =_sdata     /* r1 = VMA (adres docelowy) .data w RAM */
    ldr     r2, =_edata     /* r2 = VMA końca .data w RAM */

.L_copy_data_loop:
    cmp     r1, r2          /* Sprawdź, czy (RAM_dest >= RAM_end) */
    bhs     .L_copy_data_end
    ldr     r3, [r0], #4    /* Załaduj 4 bajty ze źródła (FLASH), inkrementuj wskaźnik r0 */
    str     r3, [r1], #4    /* Zapisz 4 bajty w docelu (RAM), inkrementuj wskaźnik r1 */
    b       .L_copy_data_loop
.L_copy_data_end:

    /* 2. Wyzeruj sekcję .bss w RAM */
    /* Załaduj adresy z linker.ld */
    ldr     r0, =_sbss      /* r0 = VMA początku .bss w RAM */
    ldr     r1, =_ebss      /* r1 = VMA końca .bss w RAM */
    movs    r2, #0          /* r2 = 0 */

.L_zero_bss_loop:
    cmp     r0, r1          /* Sprawdź, czy (bss_start >= bss_end) */
    bhs     .L_zero_bss_end
    str     r2, [r0], #4    /* Zapisz 0 w RAM, inkrementuj wskaźnik r0 */
    b       .L_zero_bss_loop
.L_zero_bss_end:

    /* 3. Wywołaj main() */
    bl      main

    /* Pętla nieskończona, jeśli main() kiedykolwiek wróci */
.L_inf_loop:
    b       .L_inf_loop

.size Reset_Handler, . - Reset_Handler

/* Minimalny handler błędu, aby procesor się nie zawiesił */
.section .text.HardFault_Handler
.thumb
.globl HardFault_Handler
.type HardFault_Handler, %function
HardFault_Handler:
.L_fault_loop:
    b       .L_fault_loop /* Po prostu zapetl się w miejscu */

.size HardFault_Handler, . - HardFault_Handler