#!/opt/bin/python3
"""
Inject a single tap at (x, y) on /dev/input/touchscreen0 (Elan multitouch
protocol-B device on rMPP). Coordinates are raw touchscreen ABS units —
NOT framebuffer pixels; the script also queries and prints the device's
ABS_MT_POSITION_{X,Y} ranges so the caller can scale.

Usage: tap.py <x> <y> [--probe]
       tap.py --probe     # just print device ranges and exit.
"""
import ctypes, fcntl, os, struct, sys, time

DEV = "/dev/input/touchscreen0"

# input_event on aarch64 Linux: struct timeval (16) + u16 + u16 + s32 = 24 B
EVENT_FMT = "llHHi"

EV_SYN = 0x00
EV_KEY = 0x01
EV_ABS = 0x03
SYN_REPORT = 0x00
BTN_TOUCH = 0x14A
ABS_MT_SLOT = 0x2F
ABS_MT_POSITION_X = 0x35
ABS_MT_POSITION_Y = 0x36
ABS_MT_TRACKING_ID = 0x39
ABS_MT_TOUCH_MAJOR = 0x30
ABS_MT_PRESSURE = 0x3A

# struct input_absinfo: s32 value, min, max, fuzz, flat, resolution = 24 B
ABSINFO_FMT = "iiiiii"
EVIOCGABS_X = 0x80184540 | (ABS_MT_POSITION_X & 0xFF)
EVIOCGABS_Y = 0x80184540 | (ABS_MT_POSITION_Y & 0xFF)

def probe():
    with open(DEV, "rb") as f:
        for label, code in (("X", ABS_MT_POSITION_X), ("Y", ABS_MT_POSITION_Y)):
            req = 0x80184540 | code  # _IOR('E', 0x40+code, struct input_absinfo)
            buf = bytearray(24)
            fcntl.ioctl(f.fileno(), req, buf, True)
            v, mn, mx, fz, fl, rs = struct.unpack(ABSINFO_FMT, bytes(buf))
            print(f"{label}: min={mn} max={mx} value={v} resolution={rs}")

def emit(fd, etype, code, value):
    now = time.time()
    sec = int(now); usec = int((now - sec) * 1e6)
    os.write(fd, struct.pack(EVENT_FMT, sec, usec, etype, code, value))

def tap(x, y, hold_ms=80):
    fd = os.open(DEV, os.O_WRONLY)
    tid = (int(time.time()) & 0xFFFF) | 0x1000
    # Down
    emit(fd, EV_ABS, ABS_MT_SLOT, 0)
    emit(fd, EV_ABS, ABS_MT_TRACKING_ID, tid)
    emit(fd, EV_ABS, ABS_MT_POSITION_X, x)
    emit(fd, EV_ABS, ABS_MT_POSITION_Y, y)
    emit(fd, EV_ABS, ABS_MT_TOUCH_MAJOR, 6)
    emit(fd, EV_ABS, ABS_MT_PRESSURE, 60)
    emit(fd, EV_KEY, BTN_TOUCH, 1)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(hold_ms / 1000)
    # Up
    emit(fd, EV_ABS, ABS_MT_SLOT, 0)
    emit(fd, EV_ABS, ABS_MT_TRACKING_ID, -1)
    emit(fd, EV_KEY, BTN_TOUCH, 0)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    os.close(fd)

if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "--probe":
        probe(); sys.exit(0)
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    tap(int(sys.argv[1]), int(sys.argv[2]))
    print(f"tapped at ({sys.argv[1]}, {sys.argv[2]})")
