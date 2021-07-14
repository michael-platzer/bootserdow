#!/usr/bin/env python3

# Copyright Michael Platzer
# Licensed under the ISC license, see LICENSE.txt for details
# SPDX-License-Identifier: ISC


MAGIC = 0x55AA55AA
MAX_PACKET_LEN = 1024
MAX_SEND_TRIES = 10

import sys
import time
import zlib
import struct
import serial
from elftools.elf.elffile import ELFFile

if len(sys.argv) < 3:
    print("Usage: %s <ELF-file> <serial-device>" % sys.argv[0])
    sys.exit(1)

path = sys.argv[1]
tty = sys.argv[2]

# open ELF file
with open(path, 'rb') as binfile:
    # open serial device
    with serial.Serial(tty, 115200, timeout=.1) as ser:
        # helper function to send a packet
        def send_packet(data):
            for i in range(MAX_SEND_TRIES):
                # remove trash from serial input queue
                ser.reset_input_buffer()

                # send data and calculate checksum
                ser.write(data)
                chksum = zlib.crc32(data)

                # receive checksum
                reply = ser.read(4)
                if len(reply) == 4 and struct.unpack('<I', reply)[0] == chksum:
                    return

                if len(reply) == 4:
                    print("Received wrong checksum (0x%08X instead of 0x%08X)"
                          % (struct.unpack('<I', reply)[0], chksum))
                else:
                    print("Target sent %d bytes instead of 4." % len(reply))

            raise RuntimeError("Target failed to reply with the correct "
                               "checksum %d times" % MAX_SEND_TRIES)

        # print ELF file info
        elf = ELFFile(binfile)
        print(f"ELF object is {elf['e_type']} for machine {elf['e_machine']}, "
              f"{elf.elfclass} bits, "
              f"{'little' if elf.little_endian else 'big'} endian")

        entrypt = elf['e_entry']

        # SEGMENT PACKETS
        for seg in elf.iter_segments():
            seg_off = seg['p_offset']
            seg_paddr = seg['p_paddr']
            seg_filesz = seg['p_filesz']
            seg_memsz = seg['p_memsz']

            desc = f"{seg['p_type']} @{seg_paddr:08X}"

            if seg_paddr == 0:
                print("skipping boot section " + desc)
                continue

            if seg_filesz == 0:
                print("skipping empty section " + desc)
                continue

            for pack_off in range(0, seg_filesz, MAX_PACKET_LEN):
                # calculate the length of the packet data (which is the maximum
                # packet length, or the length of the remaining data)
                pack_len = seg_filesz - pack_off
                if pack_len > MAX_PACKET_LEN:
                    pack_len = MAX_PACKET_LEN

                # calculate the memory address for the data of this packet
                pack_paddr = seg_paddr + pack_off

                # PACKET HEADER
                # pack the magic number, the length of the packet data and the
                # memory address for the packet data
                # (all values are packed as 4-byte integers, little endian)
                pack_head = struct.pack('<III', MAGIC, pack_len, pack_paddr)

                # PACKET DATA
                # seek to the offset of the data in the ELF file and read it
                seg.stream.seek(seg_off + pack_off)
                pack_data = seg.stream.read(pack_len)

                send_packet(pack_head + pack_data)

                # print status output and a nice progress bar
                percent = ((pack_off + pack_len) * 100) // seg_filesz
                progress = "[{:33}]".format('#' * (percent // 3))
                kib = (pack_off + pack_len) / 1024
                stat = "{:8.1f} KiB {} {:>3}%".format(kib, progress, percent)
                print(" {:24}{:>55}".format(desc, stat), end='\r', flush=True)

            # print a newline when done (avoid overwriting status output)
            print('')

        # ENTRYPOINT PACKET
        # pack the magic number, special length 0 and the entrypoint address
        # (all values are packed as 4-byte integers, little endian)
        entry_pack = struct.pack('<III', MAGIC, 0, entrypt)
        send_packet(entry_pack)
        print("Transmission complete; entry point address: 0x%08x" % entrypt)
        print('-' * 80)

        while True:
            data = ser.readline()
            if data:
                try:
                    sys.stdout.write(data.decode('utf-8'))
                    sys.stdout.flush()
                except UnicodeDecodeError as err:
                    print(f"\x1B[7m{str(err)}\x1B[0m")
