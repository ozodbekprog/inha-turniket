#!/usr/bin/env python3
# ESP32-S3 USB CDC o'quvchi (DTR/RTS ko'tarilgan holda)
# Ishlatish: python3 read_serial.py [sekund]
import os, sys, time, select, termios, fcntl, struct

import glob
cands = sorted(glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyUSB*'))
PORT = cands[0] if cands else '/dev/ttyACM0'
DUR = int(sys.argv[1]) if len(sys.argv) > 1 else 40

fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0] = 0
attrs[1] = 0
attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
attrs[3] = 0
attrs[4] = termios.B115200
attrs[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, attrs)
fcntl.ioctl(fd, 0x5416, struct.pack('I', 0x002 | 0x004))  # TIOCMBIS DTR|RTS

t0 = time.time()
while time.time() - t0 < DUR:
    r, _, _ = select.select([fd], [], [], 0.5)
    if fd in r:
        try:
            data = os.read(fd, 4096)
        except OSError:
            break
        if data:
            sys.stdout.write(data.decode('utf-8', 'replace'))
            sys.stdout.flush()
os.close(fd)
