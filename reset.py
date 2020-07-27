#!/usr/bin/python3

import ftdi1
f = ftdi1.new()
ftdi1.usb_open(f, 0x0403, 0x6001)
ftdi1.set_bitmode(f, 1, 0)
ftdi1.write_data(f, bytes([0]))
ftdi1.write_data(f, bytes([1]))
ftdi1.usb_close(f)
