// Copyright Michael Platzer
// Licensed under the ISC license, see LICENSE.txt for details
// SPDX-License-Identifier: ISC


/*
 * Boot program for the serial downloader
 */


// all UART functions need to be in the boot section too:
__attribute__ ((section (".boot"))) void uart_putc(char c);
__attribute__ ((section (".boot"))) char uart_getc(void);
__attribute__ ((section (".boot"))) void uart_write(int n, const char *buf);
__attribute__ ((section (".boot"))) void uart_read(int n, char *buf);
__attribute__ ((section (".boot"))) void uart_puts(const char *str);
__attribute__ ((section (".boot"))) void uart_gets(char *buf, int size);
__attribute__ ((section (".boot.rodata"))) volatile long *const uart_data;
__attribute__ ((section (".boot.rodata"))) volatile long *const uart_status;
#include "uart.c"


#define WORD_SIZE   sizeof(unsigned long)
#define WORD_MASK   (WORD_SIZE - 1)

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define ADD_BYTE(BYTE, DEST, POS)                                             \
    DEST |= (((unsigned long)(BYTE)) & 0xFF) << ((POS) * 8);

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define ADD_BYTE(BYTE, DEST, POS)                                             \
    DEST |= (((unsigned long)(BYTE)) & 0xFF) << ((WORD_SIZE - 1 - (POS)) * 8);

#endif


__attribute__ ((section (".boot.rodata")))
static const unsigned int poly = 0xEDB88320; // reversed polynomial

__attribute__ ((section (".boot")))
static void crc32_add_byte(unsigned int *checksum, unsigned int data)
{
    *checksum = (*checksum) ^ (data & 0xFF);

    int i;
    for (i = 0; i < 8; i++) {
        if (((*checksum) & 1) != 0)
            *checksum = ((*checksum) >> 1) ^ poly;
        else
            *checksum = ((*checksum) >> 1);
    }
}


#define MAGIC 0x55AA55AA

__attribute__ ((section (".boot.rodata")))
static unsigned long *const MEM = (unsigned long *const) 0x00000000;

__attribute__ ((section (".boot")))
unsigned long download(void)
{
    for (;;) {
        unsigned int data = 0;

        // search for magic number
        while (data != MAGIC) {
            int byte = uart_getc();
            data >>= 8;
            data |= ((unsigned long)byte) << 24;
        }

        do {
            unsigned int checksum = 0xFFFFFFFF;

            // add magic number to checksum
            crc32_add_byte(&checksum, MAGIC & 0xFF);
            crc32_add_byte(&checksum, (MAGIC >> 8) & 0xFF);
            crc32_add_byte(&checksum, (MAGIC >> 16) & 0xFF);
            crc32_add_byte(&checksum, (MAGIC >> 24) & 0xFF);

            unsigned long pos, len, addr;

            // read packet length
            len = 0;
            for (pos = 0; pos < WORD_SIZE; pos++) {
                int byte = uart_getc();
                ADD_BYTE(byte, len, pos)
                crc32_add_byte(&checksum, byte);
            }

            // read packet address
            addr = 0;
            for (pos = 0; pos < WORD_SIZE; pos++) {
                int byte = uart_getc();
                ADD_BYTE(byte, addr, pos)
                crc32_add_byte(&checksum, byte);
            }

            // read packet data
            data = 0;
            for (pos = 0; pos < len; pos++) {
                int byte = uart_getc();
                ADD_BYTE(byte, data, pos & WORD_MASK)
                crc32_add_byte(&checksum, byte);

                if ((pos & WORD_MASK) == WORD_MASK) {
                    *(MEM + (addr + pos) / WORD_SIZE) = data;
                    data = 0;
                }
            }
            if ((pos & WORD_MASK) != 0)
                *(MEM + (addr + pos) / WORD_SIZE) = data;

            checksum ^= 0xFFFFFFFF;

            // send checksum
            uart_putc(checksum & 0xFF);
            uart_putc((checksum >> 8) & 0xFF);
            uart_putc((checksum >> 16) & 0xFF);
            uart_putc((checksum >> 24) & 0xFF);

            // if this was the entrypoint packet, return the entrypoint
            if (len == 0)
                return addr;

            // (hopefully) read the magic number of the next packet
            for (pos = 0; pos < 4; pos++) {
                int byte = uart_getc();
                data >>= 8;
                data |= ((unsigned long)byte) << 24;
            }
        } while (data == MAGIC);

        // sync lost
    }
}

__attribute__ ((section (".boot.rodata")))
static const char MSG_SUCCESS[] = "Program exited normally\n";
__attribute__ ((section (".boot.rodata")))
static const char MSG_FAILURE[] = "Program exited with error status\n";

__attribute__ ((section (".boot.data"))) unsigned long sp = 0, fp = 0;

__attribute__ ((section (".boot")))
int main(void)
{
    while (1) {
        // download application
        volatile int (*entrypoint)() = (volatile int (*)())download();

        // save stack pointer and frame pointer
        asm volatile ("mv %0, sp" : "=r" (sp));
        asm volatile ("mv %0, fp" : "=r" (fp));

        // execute application
        int retval = (*entrypoint)();

        // clobber all registers except stack pointer and frame pointer
        asm volatile ("" ::: "x1",  "x3",  "x4",  "x5",  "x6",  "x7",
                             "x9",  "x10", "x11", "x12", "x13", "x14",
                             "x15", "x16", "x17", "x18", "x19", "x20",
                             "x21", "x22", "x23", "x24", "x25", "x26",
                             "x27", "x28", "x29", "x30", "x31");

        // restore stack pointer and frame pointer
        asm volatile ("mv sp, %0" :: "r" (sp));
        asm volatile ("mv fp, %0" :: "r" (fp));

        if (retval == 0)
            uart_puts(MSG_SUCCESS);
        else
            uart_puts(MSG_FAILURE);
    }
}
