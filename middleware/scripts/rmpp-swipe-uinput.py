#!/usr/bin/env python3
"""
Swipe from (X1,Y1) to (X2,Y2) over /dev/uinput. Used to invoke gestures
the rMPP only responds to as motion (e.g. edge-swipe to open navigator).

Usage: rmpp-swipe-uinput.py X1 Y1 X2 Y2 [duration_ms]
       coords are panel-pixel (0..1619 × 0..2159).
"""
import fcntl, os, struct, sys, time

UI_DEV_CREATE  = 0x5501
UI_DEV_DESTROY = 0x5502
UI_SET_EVBIT   = 0x40045564
UI_SET_KEYBIT  = 0x40045565
UI_SET_ABSBIT  = 0x40045567
UI_SET_PROPBIT = 0x4004556e

UINPUT_MAX_NAME_SIZE = 80
ABS_MAX = 0x3F

INPUT_ID_FMT = "HHHH"
USER_DEV_FMT = f"{UINPUT_MAX_NAME_SIZE}s{INPUT_ID_FMT}I{(ABS_MAX+1)*4}i"
EV_FMT = "llHHi"

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT, BTN_TOUCH = 0, 0x14a
ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID = \
    0x2f, 0x35, 0x36, 0x39
INPUT_PROP_DIRECT = 0x01

PANEL_W, PANEL_H = 1620, 2160

def ev(t, c, v): return struct.pack(EV_FMT, 0, 0, t, c, v)

def main():
    if len(sys.argv) < 5:
        print(__doc__); sys.exit(1)
    x1, y1, x2, y2 = map(int, sys.argv[1:5])
    duration_ms = int(sys.argv[5]) if len(sys.argv) > 5 else 250
    steps = max(8, duration_ms // 16)

    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    try:
        for evbit in (EV_SYN, EV_KEY, EV_ABS):
            fcntl.ioctl(fd, UI_SET_EVBIT, evbit)
        fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH)
        fcntl.ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT)
        for axis in (ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y,
                     ABS_MT_TRACKING_ID):
            fcntl.ioctl(fd, UI_SET_ABSBIT, axis)

        absmax  = [0]*(ABS_MAX+1); absmin = [0]*(ABS_MAX+1)
        absfuzz = [0]*(ABS_MAX+1); absflat = [0]*(ABS_MAX+1)
        absmax[ABS_MT_POSITION_X] = PANEL_W - 1
        absmax[ABS_MT_POSITION_Y] = PANEL_H - 1
        absmax[ABS_MT_SLOT] = 9
        absmax[ABS_MT_TRACKING_ID] = 0xFFFF
        absmin[ABS_MT_TRACKING_ID] = -1

        ud = struct.pack(USER_DEV_FMT,
                         b"vncast-virtual-touch", 0x06, 0x1234, 0x5678, 1, 0,
                         *absmax, *absmin, *absfuzz, *absflat)
        os.write(fd, ud)
        fcntl.ioctl(fd, UI_DEV_CREATE)
        time.sleep(0.3)

        tid = (int(time.time()) & 0x7fff) | 1
        # Initial touch-down
        for pkt in [
            ev(EV_ABS, ABS_MT_SLOT, 0),
            ev(EV_ABS, ABS_MT_TRACKING_ID, tid),
            ev(EV_ABS, ABS_MT_POSITION_X, x1),
            ev(EV_ABS, ABS_MT_POSITION_Y, y1),
            ev(EV_KEY, BTN_TOUCH, 1),
            ev(EV_SYN, SYN_REPORT, 0),
        ]:
            os.write(fd, pkt)

        for i in range(1, steps + 1):
            t = i / steps
            x = int(x1 + (x2 - x1) * t)
            y = int(y1 + (y2 - y1) * t)
            for pkt in [
                ev(EV_ABS, ABS_MT_POSITION_X, x),
                ev(EV_ABS, ABS_MT_POSITION_Y, y),
                ev(EV_SYN, SYN_REPORT, 0),
            ]:
                os.write(fd, pkt)
            time.sleep(duration_ms / 1000.0 / steps)

        for pkt in [
            ev(EV_ABS, ABS_MT_SLOT, 0),
            ev(EV_ABS, ABS_MT_TRACKING_ID, -1),
            ev(EV_KEY, BTN_TOUCH, 0),
            ev(EV_SYN, SYN_REPORT, 0),
        ]:
            os.write(fd, pkt)

        time.sleep(0.1)
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        print(f"swiped panel({x1},{y1})->({x2},{y2})", file=sys.stderr)
    finally:
        os.close(fd)

if __name__ == "__main__":
    main()
