#!/usr/bin/env python3
"""Minimal host-side speaker of the burnd frame protocol, for tests.

Talks to a tty (or any file descriptor pair) without pyserial so it runs on
a bare CI runner. Exit code 0 when the board answered OK to everything.

    client.py PORT upload FILE [--bad-crc] [--no-delay]
    client.py PORT cmd NAME            NAME in NONE CONNECT STOP START
    client.py PORT raw HEX             send arbitrary bytes, print the reply
"""
import os
import struct
import sys
import termios
import time
import zlib

CMDS = {"NONE": 0, "CONNECT": 1, "STOP": 2, "START": 3, "UPDATE": 4}


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY)
    attrs = termios.tcgetattr(fd)
    attrs[0] = attrs[1] = attrs[3] = 0  # iflag, oflag, lflag: raw
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 1
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def read_reply(fd, timeout=10.0):
    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = os.read(fd, 8 - len(buf))
        if chunk:
            buf += chunk
            # resync on the magic
            while buf and not buf.startswith(b"\xaa"):
                buf = buf[1:]
            if len(buf) == 8:
                cmd, status, err = struct.unpack("<HHH", buf[2:])
                return cmd, status, err
    raise SystemExit("no reply")


def send_cmd(fd, cmd, length=0):
    os.write(fd, b"\xaa\x55" + struct.pack("<HI", cmd, length))
    return read_reply(fd)


def expect(reply, cmd, status=0):
    if reply[0] != cmd or reply[1] != status:
        raise SystemExit(f"reply {reply}, want cmd {cmd} status {status}")
    print(f"ok: cmd {reply[0]} status {reply[1]}")


def main():
    port, action = sys.argv[1], sys.argv[2]
    fd = open_port(port)
    if action == "cmd":
        cmd = CMDS[sys.argv[3]]
        expect(send_cmd(fd, cmd), cmd)
    elif action == "raw":
        os.write(fd, bytes.fromhex(sys.argv[3]))
        print("reply", read_reply(fd))
    elif action == "upload":
        data = open(sys.argv[3], "rb").read()
        bad = "--bad-crc" in sys.argv
        delay = "--no-delay" not in sys.argv
        expect(send_cmd(fd, CMDS["NONE"]), 0)
        expect(send_cmd(fd, CMDS["CONNECT"]), 1)
        send_cmd(fd, CMDS["STOP"])  # may fail when nothing runs
        expect(send_cmd(fd, CMDS["UPDATE"], len(data)), 4)
        for i in range(0, len(data), 1024):
            os.write(fd, data[i:i + 1024])
        if delay:
            time.sleep(0.3)
        crc = zlib.crc32(data) ^ (0xFFFFFFFF if bad else 0)
        os.write(fd, struct.pack("<II", crc, 0xFFFFFFFF))
        reply = read_reply(fd, timeout=30)
        if bad:
            expect(reply, 4, 7)
            return
        expect(reply, 4, 0)
        expect(send_cmd(fd, CMDS["START"]), 3)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
