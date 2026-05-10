#!/usr/bin/env python3
"""
Inject a tap via /dev/uinput (creates a virtual touchscreen).
xochitl listens to all evdev devices, so a virtual one works even if
the physical Elan touch device is exclusively grabbed.

Usage: rmpp-tap-uinput.py X Y [hold_ms]
       coords are panel-pixel (0..1619 × 0..2159).
"""
import ctypes, fcntl, os, struct, sys, time

# uinput / input.h structs and ioctls (Linux uapi).

UI_DEV_CREATE  = 0x5501
UI_DEV_DESTROY = 0x5502
UI_SET_EVBIT   = 0x40045564
UI_SET_KEYBIT  = 0x40045565
UI_SET_ABSBIT  = 0x40045567
UI_SET_PROPBIT = 0x4004556e

# uinput ABS setup uses uinput_abs_setup (newer) or fall back to uinput_user_dev.
# Use uinput_user_dev (more portable).
UINPUT_MAX_NAME_SIZE = 80
ABS_MAX = 0x3F

# struct uinput_user_dev:
#   char name[UINPUT_MAX_NAME_SIZE];
#   struct input_id id;        # 4 * u16 = 8B
#   __u32 ff_effects_max;
#   __s32 absmax[ABS_MAX+1];
#   __s32 absmin[ABS_MAX+1];
#   __s32 absfuzz[ABS_MAX+1];
#   __s32 absflat[ABS_MAX+1];
INPUT_ID_FMT = "HHHH"
USER_DEV_FMT = f"{UINPUT_MAX_NAME_SIZE}s{INPUT_ID_FMT}I{(ABS_MAX+1)*4}i"

# Constants
EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT, BTN_TOUCH = 0, 0x14a
ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID = \
    0x2f, 0x35, 0x36, 0x39
# (TOUCH_MAJOR / PRESSURE used to be here; xochitl's input handler doesn't
# need them — basic SLOT + TRACKING_ID + POSITION + BTN_TOUCH is enough.)
INPUT_PROP_DIRECT = 0x01

EV_FMT = "llHHi"

PANEL_W, PANEL_H = 1620, 2160

def ev(t, c, v): return struct.pack(EV_FMT, 0, 0, t, c, v)

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    px = int(sys.argv[1])
    py = int(sys.argv[2])
    hold_ms = int(sys.argv[3]) if len(sys.argv) > 3 else 80

    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    try:
        for evbit in (EV_SYN, EV_KEY, EV_ABS):
            fcntl.ioctl(fd, UI_SET_EVBIT, evbit)
        fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH)
        fcntl.ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT)
        for axis in (ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y,
                     ABS_MT_TRACKING_ID):
            fcntl.ioctl(fd, UI_SET_ABSBIT, axis)

        absmax  = [0] * (ABS_MAX + 1)
        absmin  = [0] * (ABS_MAX + 1)
        absfuzz = [0] * (ABS_MAX + 1)
        absflat = [0] * (ABS_MAX + 1)
        absmax[ABS_MT_POSITION_X] = PANEL_W - 1
        absmax[ABS_MT_POSITION_Y] = PANEL_H - 1
        absmax[ABS_MT_SLOT]       = 9
        absmax[ABS_MT_TRACKING_ID] = 0xFFFF
        absmin[ABS_MT_TRACKING_ID] = -1

        name = b"vncast-virtual-touch"
        bus, vendor, product, version = 0x06, 0x1234, 0x5678, 1
        ud = struct.pack(USER_DEV_FMT,
                         name, bus, vendor, product, version, 0,
                         *absmax, *absmin, *absfuzz, *absflat)
        os.write(fd, ud)
        fcntl.ioctl(fd, UI_DEV_CREATE)

        # xochitl needs a moment to discover the new device.
        time.sleep(0.3)

        tid = (int(time.time()) & 0x7fff) | 1
        for pkt in [
            ev(EV_ABS, ABS_MT_SLOT, 0),
            ev(EV_ABS, ABS_MT_TRACKING_ID, tid),
            ev(EV_ABS, ABS_MT_POSITION_X, px),
            ev(EV_ABS, ABS_MT_POSITION_Y, py),
            ev(EV_KEY, BTN_TOUCH, 1),
            ev(EV_SYN, SYN_REPORT, 0),
        ]:
            os.write(fd, pkt)
        time.sleep(hold_ms / 1000.0)
        for pkt in [
            ev(EV_ABS, ABS_MT_SLOT, 0),
            ev(EV_ABS, ABS_MT_TRACKING_ID, -1),
            ev(EV_KEY, BTN_TOUCH, 0),
            ev(EV_SYN, SYN_REPORT, 0),
        ]:
            os.write(fd, pkt)

        # Hold the device alive a tick so xochitl picks the events up.
        time.sleep(0.1)
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        print(f"tapped panel({px},{py})", file=sys.stderr)
    finally:
        os.close(fd)

if __name__ == "__main__":
    main()
