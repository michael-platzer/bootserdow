# Copyright Michael Platzer
# Licensed under the ISC license, see LICENSE.txt for details
# SPDX-License-Identifier: ISC


# Generate a boot program

RISCV_CC   := riscv32-unknown-elf-gcc
RISCV_DUMP := riscv32-unknown-elf-objdump
RISCV_OBCP := riscv32-unknown-elf-objcopy

LD_SCRIPT := link.ld

CFLAGS := -march=rv32imv -mabi=ilp32 -static -mcmodel=medany                  \
          -fvisibility=hidden -nostdlib -nostartfiles -Wall -Iuart/

PROG := bootserdow
OBJ  := bootserdow.o exceptions.o crt0.o

all: $(PROG).vmem

dump: $(PROG).elf
	$(RISCV_DUMP) -D $<

$(PROG).elf: $(OBJ) $(LD_SCRIPT)
	$(RISCV_CC) $(CFLAGS) -T $(LD_SCRIPT) $(OBJ) -o $@

%.o: %.c
	$(RISCV_CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(RISCV_CC) $(CFLAGS) -c -o $@ $<

# currently unusable due to problems with byte order
# (see https://github.com/riscv/riscv-tools/issues/168#issuecomment-554973539)
#%.vmem: %.elf
#	$(OBJCOPY) -O verilog --verilog-data-width 4 $^ $@

# workaround (requires srecord):
# note: start address must be reset manually because it is lost in bin file
%.vmem: %.bin
	srec_cat $^ -binary -offset 0x0000 -byte-swap 4 -o $@ -vmem
%.bin: %.elf
	$(RISCV_OBCP) -O binary $^ $@

clean:
	rm -f *.o *.elf *.bin *.vmem
