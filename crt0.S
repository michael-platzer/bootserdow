# Copyright Michael Platzer
# Licensed under the ISC license, see LICENSE.txt for details
# SPDX-License-Identifier: ISC


###############################################################################
# REGULAR CODE SECTION (only used for dynamically loaded programs)

    .section .text

# entry point for the dynamic loader:
    .global _start
    .type   _start, @function
_start:
    # clear bss
    la t1, _bss_start
    la t2, _bss_end

    bge t1, t2, bss_clear_end

bss_clear_loop:
    sw x0, 0(t1)
    addi t1, t1, 4
    ble t1, t2, bss_clear_loop

bss_clear_end:

    # jump to main (argc = 0, argv = 0, returns to the caller of _start)
    li a0, 0
    li a1, 0
    j main


###############################################################################
# BOOT SECTION (only used for programs that are written to RAM before reset)

    .section .boot, "ax"

# entry point for the reset handler:
_boot_start:
    # set all registers to zero
    mv  x1, x0
    mv  x2, x0
    mv  x3, x0
    mv  x4, x0
    mv  x5, x0
    mv  x6, x0
    mv  x7, x0
    mv  x8, x0
    mv  x9, x0
    mv x10, x0
    mv x11, x0
    mv x12, x0
    mv x13, x0
    mv x14, x0
    mv x15, x0
    mv x16, x0
    mv x17, x0
    mv x18, x0
    mv x19, x0
    mv x20, x0
    mv x21, x0
    mv x22, x0
    mv x23, x0
    mv x24, x0
    mv x25, x0
    mv x26, x0
    mv x27, x0
    mv x28, x0
    mv x29, x0
    mv x30, x0
    mv x31, x0

    # initialize stack pointer
    la sp, _stack

    # clear bss
    la t1, _bss_start
    la t2, _bss_end

    bge t1, t2, boot_bss_clear_end

boot_bss_clear_loop:
    sw x0, 0(t1)
    addi t1, t1, 4
    ble t1, t2, boot_bss_clear_loop

boot_bss_clear_end:

    # jump to main (argc = 0, argv = 0, returns to _boot_start)
    li a0, 0
    li a1, 0
    la ra, _boot_start
    j main


# default exception handler (infinite loop)
    .weak exception_handler
    .type exception_handler, @function
exception_handler:
    j exception_handler


# wrapper for exception handlers:
exc_wrapper:
    # save registers:
    addi sp, sp, -80
    sw ra, 76(sp)
    sw s0, 72(sp)
    addi s0, sp, 80
    sw t0, 68(sp)
    sw t1, 64(sp)
    sw t2, 60(sp)
    sw t3, 56(sp)
    sw t4, 52(sp)
    sw t5, 48(sp)
    sw t6, 44(sp)
    sw a0, 40(sp)
    sw a1, 36(sp)
    sw a2, 32(sp)
    sw a3, 28(sp)
    sw a4, 24(sp)
    sw a5, 20(sp)
    sw a6, 16(sp)
    sw a7, 12(sp)

    # save mpec, mie and mstatus:
    csrr t0, mie
    csrr t1, mstatus
    csrr a1, mepc
    sw t0, 0(sp)
    sw t1, 4(sp)
    sw a1, 8(sp)

    # call exception handler (a0 = mcause, a1 = mpec, a2 = mtval, a3 = fp)
    csrr a0, mcause
    csrr a2, mtval
    lw a3, 72(sp)
    call exception_handler

    # restore mpec, mie and mstatus (restoring mstatus disables interrupts):
    lw t0, 0(sp)
    lw t1, 4(sp)
    lw t2, 8(sp)
    csrw mstatus, t1
    csrw mepc, t2
    csrw mie, t0

    # restore registers:
    lw a7, 12(sp)
    lw a6, 16(sp)
    lw a5, 20(sp)
    lw a4, 24(sp)
    lw a3, 28(sp)
    lw a2, 32(sp)
    lw a1, 36(sp)
    lw a0, 40(sp)
    lw t6, 44(sp)
    lw t5, 48(sp)
    lw t4, 52(sp)
    lw t3, 56(sp)
    lw t2, 60(sp)
    lw t1, 64(sp)
    lw t0, 68(sp)
    lw s0, 72(sp)
    lw ra, 76(sp)
    addi sp, sp, 80
    mret


###############################################################################
# VECTORS

    .section .vectors, "ax"
    .option norvc;

    .org 0x00
    .rept 32
    j exc_wrapper
    .endr

    # reset vector
    .org 0x80
    j _boot_start
