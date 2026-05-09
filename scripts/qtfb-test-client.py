#!/opt/bin/python3
"""
qtfb test client. Stands in for the (not-yet-forked) vnsee, so we can
exercise vncast's qtfb server end-to-end before touching vnsee's source.

Connects to /run/vncast.sock, completes the v1 handshake, mmaps the shm
the server hands back, and paints either:
  - a solid frame (--solid 255,0,255)
  - a moving checkerboard (--checker)
  - a frame that animates a counter so we can see frames updating

Then sends FRAME messages at the requested fps until interrupted.

Run on the rMPP (since the socket is local to that host):
    /opt/bin/python3 /home/root/qtfb-test-client.py --checker --fps 4
"""

import argparse
import ctypes
import mmap
import os
import socket
import struct
import sys
import time

SOCKET_PATH = "/run/vncast.sock"
MAGIC       = 0x56434B31   # 'VCK1'

TAG_HELLO_C2S    = 0x01
TAG_HELLO_ACK    = 0x02
TAG_FRAME        = 0x03
TAG_BYE          = 0xFF

# struct Header { u32 magic; u32 tag; }                — 8 bytes
# struct HelloC2S { Header; u32 ver; u32 reqW; u32 reqH; }   — 8+12 = 20
# struct HelloAckS2C { Header; u32 accepted; u32 w; u32 h; u32 stride;
#                      u32 bpp; u32 format; u32 shm_name_len;
#                      char shm_name[64]; }                  — 8 + 7*4 + 64 = 100
# struct Frame { Header; u32 seq; u32 x,y,w,h; }             — 8 + 5*4 = 28
# struct Bye { Header; u32 reason; }                         — 8+4 = 12

FMT_HEADER       = "<II"
FMT_HELLO_C2S    = "<II III"
FMT_HELLO_ACK    = "<II IIIIIII 64s"
FMT_FRAME        = "<II IIIII"
FMT_BYE          = "<II I"

def hello(sock):
    sock.sendall(struct.pack(FMT_HELLO_C2S, MAGIC, TAG_HELLO_C2S, 1, 0, 0))
    raw = recvall(sock, struct.calcsize(FMT_HELLO_ACK))
    (magic, tag, accepted, w, h, stride, bpp, fmt, name_len, name) = \
        struct.unpack(FMT_HELLO_ACK, raw)
    if magic != MAGIC or tag != TAG_HELLO_ACK:
        raise RuntimeError(f"bad hello-ack: magic={magic:#x} tag={tag}")
    if not accepted:
        raise RuntimeError("server rejected our hello")
    shm_name = name[:name_len].decode("latin-1").rstrip("\x00")
    return w, h, stride, bpp, fmt, shm_name

def recvall(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise RuntimeError("server closed during recv")
        buf += chunk
    return buf

def map_shm(name, size):
    fd = os.open("/dev/shm" + name, os.O_RDWR)
    mm = mmap.mmap(fd, size, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
    os.close(fd)
    return mm

# ----- pixel patterns -----

def fill_solid(mm, w, h, stride, bpp, rgba):
    r, g, b, a = rgba
    if bpp == 32:
        # RGBA8888 little-endian: R G B A in memory
        row = bytes((r, g, b, a)) * w
        for y in range(h):
            mm[y * stride : y * stride + len(row)] = row
    else:
        gray = (r * 30 + g * 59 + b * 11) // 100
        v16  = struct.pack("<H", gray * 257)
        row  = v16 * w
        for y in range(h):
            mm[y * stride : y * stride + len(row)] = row

def fill_checker(mm, w, h, stride, bpp, frame_idx, cell=64):
    offset = (frame_idx * cell // 4) % (cell * 2)
    if bpp == 32:
        for y in range(h):
            yy = (y + offset) // cell
            row = bytearray(stride)
            for x in range(w):
                xx = (x + offset) // cell
                if (xx + yy) & 1:
                    row[x*4:x*4+4] = b"\x00\x00\x00\xff"   # black
                else:
                    row[x*4:x*4+4] = b"\xff\xff\xff\xff"   # white
            mm[y*stride : y*stride + stride] = bytes(row)
    else:
        for y in range(h):
            yy = (y + offset) // cell
            row = bytearray(stride)
            for x in range(w):
                xx = (x + offset) // cell
                v = 0x0000 if (xx + yy) & 1 else 0xFFFF
                row[x*2:x*2+2] = struct.pack("<H", v)
            mm[y*stride : y*stride + stride] = bytes(row)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--solid", help="R,G,B,A — fill once and tick frames")
    ap.add_argument("--checker", action="store_true", help="moving checkerboard")
    ap.add_argument("--fps", type=int, default=4)
    ap.add_argument("--socket", default=SOCKET_PATH)
    args = ap.parse_args()

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(args.socket)
    w, h, stride, bpp, fmt, shm_name = hello(sock)
    print(f"hello-ack: {w}x{h} stride={stride} bpp={bpp} fmt={fmt} shm={shm_name}",
          flush=True)

    mm = map_shm(shm_name, stride * h)
    print(f"mmap'd /dev/shm{shm_name} ({stride * h} bytes)", flush=True)

    if args.solid:
        rgba = tuple(int(c) for c in args.solid.split(","))
        if len(rgba) != 4:
            sys.exit("--solid needs R,G,B,A")
        fill_solid(mm, w, h, stride, bpp, rgba)

    seq = 0
    period = 1.0 / max(1, args.fps)
    try:
        while True:
            if args.checker:
                fill_checker(mm, w, h, stride, bpp, seq)
            sock.sendall(struct.pack(FMT_FRAME, MAGIC, TAG_FRAME, seq, 0, 0, 0, 0))
            seq += 1
            print(f"sent FRAME seq={seq}", flush=True)
            time.sleep(period)
    except KeyboardInterrupt:
        pass
    finally:
        try:
            sock.sendall(struct.pack(FMT_BYE, MAGIC, TAG_BYE, 0))
        except OSError:
            pass
        mm.close()
        sock.close()

if __name__ == "__main__":
    main()
