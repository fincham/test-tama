#!/usr/bin/python3

import sys
import serial
ser = serial.Serial(sys.argv[1], 9600, timeout=20)

#payload = "test"
#frame = ('\x01%s%s\x04\r' % (chr(len(payload)), payload)).encode('ascii')
#ser.write(frame)
#print(frame)

pkt_buffer = b""
pkt_good = 0

page = 0

while True:
    pkt_buffer = pkt_buffer + ser.read(ser.in_waiting)
    for i in range(0, len(pkt_buffer)):
        if pkt_buffer[i] == 0x01: # /* 0x01 is "start of header" */
            try:
                payload_length = pkt_buffer[i+4]
                end_marker = i+payload_length+5
                if pkt_buffer[end_marker] == 0x04: # /* 0x04 is "end of transmission" */
                    pkt_good = pkt_good + 1
                    print("pkt_good=%i" % pkt_good)
                    print(pkt_buffer)

                    if pkt_buffer[i+1] == 0x01: # 0x01 is "hello"
                        print("hello from %s to %s" % (hex(pkt_buffer[i+2]), hex(pkt_buffer[i+3])))
                    elif pkt_buffer[i+1] == 0x09: # 0x03 is "log"
                        print("log from %s to %s: %s" % (hex(pkt_buffer[i+2]), hex(pkt_buffer[i+3]), repr(pkt_buffer[i+5:i+5+payload_length].decode('ascii'))))
                    elif pkt_buffer[i+1] == 0x03: # 0x03 is "log"
                        print("log from %s to %s: %s" % (hex(pkt_buffer[i+2]), hex(pkt_buffer[i+3]), repr(pkt_buffer[i+5:i+5+payload_length].decode('ascii'))))
                    elif pkt_buffer[i+1] == 0x03: # 0x05 is "sector erased"
                        print("sector erased OK from %s to %s: %s" % (hex(pkt_buffer[i+2]), hex(pkt_buffer[i+3]), hex(pkt_buffer[i+5])))
                    elif pkt_buffer[i+1] == 0x06: # 0x06 is "reprogram my external ROM"
                        print("reprogram_me from %s to %s" % (hex(pkt_buffer[i+2]), hex(pkt_buffer[i+3])))
                        # erase next sector
                        ser.write(b"\x01\x04\xff\xff\x01" + chr(page).encode('ascii') + b"\x04" + b"\x0d")
                        page = page + 1
                    else:
                        print("unknown from %s to %s: %s" % (hex(pkt_buffer[i+2]), hex(pkt_buffer[i+3]), pkt_buffer[i:i+payload_length+1+5].hex()))

                    pkt_buffer = b""
                    break
            except IndexError:
                continue
ser.close()
