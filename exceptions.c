// Copyright Michael Platzer
// Licensed under the ISC license, see LICENSE.txt for details
// SPDX-License-Identifier: ISC


#include "uart.h"

__attribute__ ((section (".boot")))
char *ultoa(unsigned long value, char *str, int base, int zero)
{
    int len = 0;
    if (1 < base && base < 36) {
        // write string in reverse order:
        do {
            unsigned long digit = value % base;
            str[len++] = (digit < 10) ? '0' + digit : 'A' - 10 + digit;
            value = value / base;
        }
        while (value);
        // pad with zeroes:
        while (len < zero)
            str[len++] = '0';
        // reverse characters:
        int pos1, pos2;
        for (pos1 = 0, pos2 = len - 1; pos1 < pos2; pos1++, pos2--) {
            char temp = str[pos1];
            str[pos1] = str[pos2];
            str[pos2] = temp;
        }
    }
    str[len] = 0;
    return str;
}

__attribute__ ((section (".boot")))
char *sltoa(long value, char *str, int base, int zero)
{
    if (value < 0) {
        *(str++) = '-';
        value -= value;
    }
    return ultoa(value, str, base, zero);
}

#define BOOTCHAR                                                              \
    __attribute__ ((section (".boot.rodata"))) static const char

BOOTCHAR MSG_EXC1[]             = "EXCEPTION @";
BOOTCHAR MSG_EXC2[]             = ": ";
BOOTCHAR MSG_INSTR_ACCESS[]     = "instruction access fault\n";
BOOTCHAR MSG_INSTR_ILLEGAL[]    = "illegal instruction ";
BOOTCHAR MSG_BREAKPOINT[]       = "breakpoint\n";
BOOTCHAR MSG_LOAD_ACCESS[]      = "load access fault @";
BOOTCHAR MSG_STORE_ACCESS[]     = "store access fault @";
BOOTCHAR MSG_ENV_U[]            = "environment call from U mode\n";
BOOTCHAR MSG_ENV_M[]            = "environment call from M mode\n";
BOOTCHAR MSG_EXC_UNKNOWN[]      = "unknown exception code ";
BOOTCHAR MSG_BACKTRACE[]        = "Backtrace:\n";
BOOTCHAR MSG_BACKTRACE_FRAME[]  = "Stack frame @";
BOOTCHAR MSG_BACKTRACE_RETADDR[]= ", return address: ";
BOOTCHAR MSG_IRQ1[]             = "INTERRUPT @";
BOOTCHAR MSG_IRQ2[]             = ", code: ";
BOOTCHAR MSG_NEWLINE[]          = "\n";

#define WORD_SIZE   sizeof(long)

BOOTCHAR EXC_HANDLER_WELCOME[] = "EXCEPTION HANDLER\n";

//extern void *const _stack;
//__attribute__ ((section (".boot.rodata")))
//void *const stack_offset = (void *)0x100000;

__attribute__ ((section (".boot")))
void exception_handler(long mcause, void *mpec, void *mtval, void *frame_ptr)
{
    uart_puts(EXC_HANDLER_WELCOME);

    // handle exceptions:
    if (mcause >= 0) {
        char buf[16];

        uart_puts(MSG_EXC1);
        uart_puts(ultoa((unsigned long)mpec, buf, 16, WORD_SIZE * 2));
        uart_puts(MSG_EXC2);

        int backtrace = 0, resume = 0;

        switch (mcause) {
            case 1: // instruction access fault
                uart_puts(MSG_INSTR_ACCESS);
                backtrace = 1;
                break;

            case 2: // illegal instruction
                uart_puts(MSG_INSTR_ILLEGAL);
                uart_puts(ultoa((unsigned long)mtval, buf, 16, WORD_SIZE * 2));
                uart_puts(MSG_NEWLINE);
                backtrace = 1;
                break;

            case 3: // breakpoint
                uart_puts(MSG_BREAKPOINT);
                resume = 1;
                break;

            case 5: // load access fault
                uart_puts(MSG_LOAD_ACCESS);
                uart_puts(ultoa((unsigned long)mtval, buf, 16, WORD_SIZE * 2));
                uart_puts(MSG_NEWLINE);
                backtrace = 1;
                break;

            case 7: // store access fault
                uart_puts(MSG_STORE_ACCESS);
                uart_puts(ultoa((unsigned long)mtval, buf, 16, WORD_SIZE * 2));
                uart_puts(MSG_NEWLINE);
                backtrace = 1;
                break;

            case 8: // environment call from U mode
                uart_puts(MSG_ENV_U);
                break;

            case 11: // environment call from M mode
                uart_puts(MSG_ENV_M);
                break;

            default:
                uart_puts(MSG_EXC_UNKNOWN);
                uart_puts(ultoa((unsigned long)mcause, buf, 10, 0));
                uart_puts(MSG_NEWLINE);
        }

        // print a backtrace if required:
        if (backtrace) {
            uart_puts(MSG_BACKTRACE);

            void *curr_frame = frame_ptr;
            while (frame_ptr <= curr_frame && curr_frame <= (void *)0x100000) {
                uart_puts(MSG_BACKTRACE_FRAME);
                uart_puts(ultoa((unsigned long)curr_frame, buf, 16,
                                WORD_SIZE * 2));

                void *ret_addr = *((void **)(curr_frame - sizeof(void *)));
                uart_puts(MSG_BACKTRACE_RETADDR);
                uart_puts(ultoa((unsigned long)ret_addr, buf, 16,
                                WORD_SIZE * 2));
                uart_puts(MSG_NEWLINE);

                curr_frame = *((void **)(curr_frame - 2 * sizeof(void *)));
            }
        }

        // loop if the exception can not be resolved:
        if (!resume)
            while (1)
                ;
    }

    // handle interrupts:
    else {
        char buf[16];
        uart_puts(MSG_IRQ1);
        uart_puts(ultoa((unsigned long)mpec, buf, 16, WORD_SIZE * 2));
        uart_puts(MSG_IRQ2);
        uart_puts(ultoa((unsigned long)(mcause & 0x7FFFFFFFL), buf, 10, 0));
        uart_puts(MSG_NEWLINE);

        // loop after an unhandled interrupt (the interrupt line is likely
        // still high, which would immediately trigger the interrupt again):
        while (1)
            ;
    }
}
