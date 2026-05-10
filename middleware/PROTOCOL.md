# rmcast Protocol — Draft

**Status:** reserved space, not yet implemented.
**Version identifier on the wire:** `rmcast/1`.
**Scope:** the wire between `vnsee` (running on the reMarkable) and `rcast-host` (running on the desktop being mirrored). Internal qtfb between vncast↔vnsee on the rM is a separate document.

---

## 1. Goals

`rmcast` is a small set of extensions layered on top of standard RFB 3.8 so that a tuned `rcast-host` and a tuned `vnsee` can:

1. **Auto-mode the desktop side.** rcast-host learns the rM's panel size + orientation and drives its IDD virtual display to match — the manual IddSampleDriver dance goes away.
2. **Stop sending frames the panel won't show.** Per-region waveform/encoding hints, FPS caps, idle suppression, mono palette for grayscale panels.
3. **Carry rich pen + touch input upstream** with enough fidelity that rcast-host can re-emit it on the desktop as a HID-compliant digitizer. End goal: any Windows app that already speaks Wacom-class pen (OneNote, Photoshop, Krita, Concepts, Clip Studio…) sees the rM's pen with no per-app changes.

Stock TightVNC / TigerVNC clients must keep working against an rmcast server (and vice versa). Anything rmcast-specific is gated behind a pseudoencoding negotiation; if the peer doesn't ack, those features are off and the session falls back to plain RFB.

## 2. Non-goals

* We do not redesign RFB. Pixel-data encodings, framebuffer-update messages, security types, etc. are unchanged.
* We do not require a custom server. Plain TightVNC + manually-configured IddSampleDriver still works exactly as it does today; rmcast is purely additive.
* No audio. No file transfer. No clipboard semantics beyond stock RFB cuttext.

## 3. Versioning & negotiation

After the standard RFB 3.8 handshake, the client sends a `SetEncodings` message that includes the `rmcast/Capabilities` pseudoencoding (see §4) followed by the rest of the encodings it supports. A server that recognises the pseudoencoding **must** respond by sending a `ServerCapabilities` message (§5) before the first `FramebufferUpdate`. A server that doesn't recognise it ignores it; the client then falls back to vanilla RFB and never sends rmcast-extension messages.

Version field is a u16. `0x0001` is `rmcast/1`. Future revisions bump this monotonically. Both peers must commit to the **lower** of the two advertised versions.

## 4. Reserved numbers

All numbers are picked from RFB's private-use ranges and are subject to change before `rmcast/1` is finalised, but are reserved here so implementations can stop guessing.

### 4.1 Pseudoencodings

| Number | Name | Purpose |
|---|---|---|
| `-1024` | `rmcast/Capabilities` | Negotiation handshake. Must be in the SetEncodings list. |
| `-1025` | `rmcast/RegionHint` | Server hints region semantics (text / photo / video / cursor). |
| `-1026` | `rmcast/MonoPalette` | Server pre-quantises into a 16-grey or 4-grey palette for mono panels. |
| `-1027` | `rmcast/IdleSuppression` | Server suppresses updates whose pixel-Δ is below a client-stated threshold. |
| `-1028` | `rmcast/FpsCap` | Server respects a client-stated maximum frames/sec (no buffer-and-drop). |

Encodings >= -1024 are reserved for future rmcast extensions; do not use them ad-hoc.

### 4.2 Client → Server messages

Stock RFB defines 0–7. We use 100+ to keep clear of any conflicting extensions.

| Type | Name | §  |
|---|---|---|
| `100` | `ClientCapabilities` | §5 |
| `101` | `TabletEvent` | §6 |
| `102` | `TouchEvent` | §7 |
| `103` | `WaveformPreferences` | §8.1 |

### 4.3 Server → Client messages

Stock RFB defines 0–3. We use 200+.

| Type | Name | § |
|---|---|---|
| `200` | `ServerCapabilities` | §5 |
| `201` | `RegionHint` | §8.2 |
| `202` | `Heartbeat` | §8.3 |

## 5. Capability handshake

Both sides exchange a single capability message early in the session. The format is a TLV blob so we can keep adding fields without bumping the version.

```
struct CapabilitiesMessage {
    u8   message-type;     // 100 (client) or 200 (server)
    u8   _padding[3];
    u16  protocol-version; // 0x0001 for rmcast/1
    u16  num-tlvs;
    TLV  tlvs[num-tlvs];
};

struct TLV {
    u16  tag;
    u16  length;          // bytes of value following
    u8   value[length];
};
```

### 5.1 Tags the **client** sends

| Tag | Name | Type | Meaning |
|---|---|---|---|
| `0x0001` | `device-class` | string | `"ferrari"`, `"kraken"`, etc.; matches `/sys/devices/soc0/machine`. |
| `0x0002` | `panel-size` | u32 w, u32 h | Native portrait pixels (e.g. 1620 × 2160). |
| `0x0003` | `panel-format` | u8 bpp, u8 channels, u8 grayscale_bits, u8 _pad | e.g. (32, 4, 0, 0) for rMPP, (16, 1, 16, 0) for rM2. |
| `0x0004` | `native-orientation` | u8 (0=portrait, 1=landscape) | Hardware native; current display rotation reported separately. |
| `0x0005` | `current-orientation` | u8 (0/90/180/270, in degrees CW from native) | Updated whenever the user rotates. |
| `0x0006` | `waveforms` | u32 bitmask | Bits: 0=A2, 1=DU, 2=GC16, 3=GLR16, 4=GLD16, 5=INIT. |
| `0x0007` | `fps-cap` | u16 | Max frames/sec the client wants pushed (0 = unbounded). |
| `0x0008` | `tablet-caps` | u32 bitmask | See §6.1. |
| `0x0009` | `touch-caps` | u32 bitmask | See §7.1. |
| `0x000a` | `client-name` | string | `"vncast/0.x"`. Diagnostic. |

### 5.2 Tags the **server** sends in response

| Tag | Name | Type | Meaning |
|---|---|---|---|
| `0x0101` | `display-set` | u8 (0/1) | 1 if the server has driven its virtual display to match the client's panel. |
| `0x0102` | `virtual-display-size` | u32 w, u32 h | What the server set the IDD virtual display to. May differ if the client requested a non-supported mode. |
| `0x0103` | `hid-pen-attached` | u8 (0/1) | 1 if rcast-host enumerated a HID pen device matching the client's tablet caps. |
| `0x0104` | `hid-touch-attached` | u8 (0/1) | Same for touch. |
| `0x0105` | `accepted-fps-cap` | u16 | What the server will actually obey. |
| `0x0106` | `server-encodings` | u16 count, then pairs (u8 kind, u8 quality) | rmcast-aware encoder list. Diagnostic. |
| `0x0107` | `server-name` | string | `"rcast-host/0.x"`. |

If the client sees `display-set=0` or `hid-pen-attached=0` it MAY surface a degraded-mode notice in the rM-side UI. It MUST still proceed.

## 6. Tablet (pen) events

We tunnel Wayland's `wp_tablet_tool_v2` event vocabulary directly. This avoids inventing a new pen protocol — Wayland's design has years of multi-vendor input thinking baked in and maps cleanly to Windows HID descriptors and macOS NSEvent fields alike.

### 6.1 `tablet-caps` bitmask

The client advertises the pen capabilities it actually intends to send. rcast-host uses this to decide what HID usages to expose on the virtual digitizer.

| Bit | Capability | Implies HID usage |
|---|---|---|
| 0 | tip pressure (16-bit) | Tip Pressure (`0x30 / 0x30`) |
| 1 | tilt X / Y (signed, ±90°) | X Tilt (`0x3D`), Y Tilt (`0x3E`) |
| 2 | rotation (twist, 16-bit) | Twist (`0x41`) |
| 3 | distance / hover (16-bit) | In Range, Distance |
| 4 | eraser tool | Invert (`0x3C`) |
| 5 | barrel button(s) | Barrel Switch (`0x44`) |
| 6 | wheel / slider (e.g. Lamy AL-star variants) | Wheel |
| 7 | tool ID (multi-pen disambiguation) | Transducer Index |
| 8 | proximity in / out distinct from tip | In Range as separate report |

For the rMPP digitizer (Elan marker), the realistic set is bits 0, 1, 3, 4, 5, 8.

### 6.2 `TabletEvent` message (client → server, type 101)

```
struct TabletEvent {
    u8   message-type;       // 101
    u8   tool-type;          // 0=pen, 1=eraser, 2=brush, 3=pencil, 4=airbrush, 5=finger, 6=mouse, 7=lens
    u16  flags;              // bit 0=frame-end, bit 1=proximity-in, bit 2=proximity-out
    u32  tool-serial;        // stable per physical tool; 0 if unknown
    u32  ts-ms;              // milliseconds since session start
    u16  num-axes;
    Axis axes[num-axes];
};

struct Axis {
    u16  axis;               // see §6.3
    u32  value;              // axis-defined encoding
};
```

Events come in **bursts** terminated by `flags & frame-end` — the same atomicity Wayland's `frame` event provides, so rcast-host can issue a single HID report per burst and no app sees a torn position+pressure pair.

### 6.3 Axis tags

Mirrors Wayland tablet-v2 axis names so we can borrow the reference implementation if useful.

| Axis | Name | Encoding |
|---|---|---|
| `0x01` | x | u32, framebuffer pixels (use `panel-size` from caps) |
| `0x02` | y | u32, framebuffer pixels |
| `0x03` | tip-pressure | u32, normalized 0..0xFFFF |
| `0x04` | tilt-x | s32, hundredths of a degree, ±9000 |
| `0x05` | tilt-y | s32, hundredths of a degree, ±9000 |
| `0x06` | distance | u32, normalized 0..0xFFFF (0 = on the panel) |
| `0x07` | rotation | u32, hundredths of a degree, 0..36000 |
| `0x08` | wheel | s32, click ticks since last report |
| `0x09` | tip-down | u32, 0 or 1 |
| `0x0a` | barrel-1 | u32, 0 or 1 |
| `0x0b` | barrel-2 | u32, 0 or 1 |

Unknown axes MUST be ignored by the receiver (forward-compat). Required axes per event burst:
- proximity-in burst: `x`, `y`, `distance`
- motion burst: `x`, `y` (plus whatever's changed)
- tip-down/up: `tip-down`
- proximity-out: `distance` (set to max)

## 7. Touch events

Multi-touch follows the same pattern, mapped to Wayland's `wl_touch` event vocabulary. Reserved here, deferred until pen lands first.

### 7.1 `touch-caps` bitmask

| Bit | Capability |
|---|---|
| 0 | up to N concurrent contacts (next field carries N) |
| 1 | per-contact pressure |
| 2 | contact area (major/minor radii) |
| 3 | orientation |

### 7.2 `TouchEvent` message (client → server, type 102)

```
struct TouchEvent {
    u8   message-type;       // 102
    u8   _pad;
    u16  flags;              // bit 0=frame-end
    u32  ts-ms;
    u16  num-contacts;
    Contact contacts[num-contacts];
};

struct Contact {
    u32  slot-id;            // protocol-B tracking ID
    u8   phase;              // 0=down, 1=move, 2=up, 3=cancel
    u8   _pad[3];
    u32  x;                  // framebuffer pixels
    u32  y;
    u16  pressure;           // 0xFFFF if not reported
    u16  major;              // contact radius major axis (panel pixels)
    u16  minor;
    s16  orientation;        // hundredths of a degree, ±9000
};
```

## 8. Frame-stream extensions

These are the optimisations that justify a custom server at all. None affect input.

### 8.1 `WaveformPreferences` (client → server, type 103)

The client tells the server which waveform it intends to use per region kind. The server's `RegionHint` (§8.2) then references *kinds*, and the client picks the waveform locally. Lets the user flip "fast & blurry" ↔ "slow & sharp" without a reconnect.

```
struct WaveformPreferences {
    u8   message-type;       // 103
    u8   _pad[3];
    u8   text-waveform;      // see §6.1 bitmask values, but as a single index
    u8   ui-waveform;
    u8   image-waveform;
    u8   default-waveform;
};
```

### 8.2 `RegionHint` (server → client, type 201)

Sent immediately before a `FramebufferUpdate` whose rectangles correspond to the regions described.

```
struct RegionHint {
    u8   message-type;       // 201
    u8   _pad;
    u16  num-regions;
    Region regions[num-regions];
};

struct Region {
    s16  x, y;
    u16  w, h;
    u8   kind;               // 0=text, 1=ui, 2=image, 3=cursor, 4=video
    u8   change-magnitude;   // 0..255, server's estimate of pixel-Δ
    u16  flags;              // bit 0=transient (cursor etc.), bit 1=interactive
};
```

Client-side mapping:
* `text` → A2 by default, GC16 if user prefers sharp.
* `ui` → DU.
* `image` → GLR16 / GC16.
* `cursor` → suppress (we're an e-ink device, cursor blink is wasteful).
* `video` → suppress / heavily downsample; rmcast is for desktop work, not video.

### 8.3 `Heartbeat` (server → client, type 202)

Periodic noop so vnsee can detect a hung server within `2 * heartbeat-interval` instead of waiting on TCP keepalive. Sent every 2 s by default. Payload is empty.

### 8.4 `MonoPalette` (pseudoencoding -1026)

When the client advertises `panel-format` as 16-grey or 4-grey, the server pre-quantises the framebuffer on its side using ordered dithering, and ships indices instead of RGB. Saves bandwidth and avoids client-side dither shimmer (the dither pattern stays stable across updates because it's a function of the source pixel, not of decode timing).

### 8.5 `IdleSuppression` (pseudoencoding -1027)

Client sends `(threshold u8, dwell-ms u16)` once during caps. Server omits update rectangles whose mean per-pixel Δ from the previously-sent state is below `threshold`, and any rectangle that hasn't been "stable then dirty" for at least `dwell-ms`. Reduces "the cursor flicker keeps repainting" pathology.

### 8.6 `FpsCap` (pseudoencoding -1028)

Client sends `(max-fps u16)` once. Server rate-limits its update emission. Crucially — server **does not buffer-and-drop**; it integrates dirt up to the cap interval and emits one consolidated update. Avoids latency spikes from queue drain after long idle.

## 9. HID parity contract

This is the part the user calls out specifically: the desktop side has to look *exactly* like a Wacom-class device to other apps. The wire only carries data; the contract is on rcast-host's IDD bundle to expose it correctly.

### 9.1 What rcast-host MUST enumerate

When `tablet-caps` arrives in the client capability message, rcast-host's IDD driver bundle MUST instantiate a HID device on the same enumerator that satisfies the following:

* **Usage Page 0x0D (Digitizer), Usage 0x02 (Pen)** as the top-level collection.
* One **Stylus collection (Usage 0x20)** per advertised tool. Tools persist across proximity transitions if `tool-serial != 0`.
* Reports include, conditional on tablet-caps bits:
  * **Tip Switch** (`Usage 0x42`) — always.
  * **In Range** (`Usage 0x32`) — if bit 3 (distance) advertised.
  * **Tip Pressure** (`Usage 0x30`) — if bit 0.
  * **X / Y** (Generic Desktop, Usages 0x30/0x31) — always.
  * **X Tilt / Y Tilt** (`Usage 0x3D / 0x3E`) — if bit 1.
  * **Twist** (`Usage 0x41`) — if bit 2.
  * **Invert** (`Usage 0x3C`) — if bit 4 (eraser).
  * **Barrel Switch** (`Usage 0x44`) — if bit 5.
  * **Transducer Index** (`Usage 0x38`) — if bit 7.
* Logical/physical ranges on X, Y, pressure, tilt MUST match what's declared in the capability message — Windows uses these to compute Ink coordinates.
* Reports are emitted **once per `frame-end`** event burst, never per individual axis. (Windows stack tolerates per-axis but app-level Ink suffers; one report per gesture is the contract real Wacom drivers honour.)

If rcast-host can't allocate a HID device (e.g. driver install failed), it sets `hid-pen-attached=0` in its capability response and the client surfaces a "pen forwarding unavailable" notice. Core paint still works.

### 9.2 What this gets us "for free"

* **Windows Ink Workspace**, OneNote, Whiteboard — all listen on the standard HID pen interfaces. They pick up an rmcast pen with no integration on their side.
* **Pressure-aware brushes** in Photoshop/Krita/CSP/Procreate-via-Affinity — these read pressure from `WM_POINTER*` messages, which are sourced directly from HID pen reports.
* **Eraser flip** — apps that switch tool on `Invert` (most pro raster apps) automatically work.
* **Palm rejection** — Windows treats pen-with-In-Range as authoritative over touch, exactly the heuristic you'd want.

The reason for going through HID rather than a synthetic-pointer API (Win32's `InjectSyntheticPointerInput`): synthetic pointers carry pressure but **not** invert/eraser, **not** stable tool serial, **not** twist; many third-party pen-aware apps gate features on "is this a Wacom-class device?" and synthetic pointers fail that check. HID-via-IDD doesn't.

## 10. Worked example — pen-down on rMPP

```
client                                           server (rcast-host)
------                                           --------------------
RFB 3.8 handshake
SetEncodings: [-1024, …, raw, copyrect, hextile]
                                                 (recognises -1024)
ClientCapabilities (type 100):
  device-class    "ferrari"
  panel-size      1620 × 2160
  panel-format    32-bit RGBA
  native-or       portrait
  current-or      90 (held landscape, USB right)
  waveforms       0b011111
  fps-cap         8
  tablet-caps     0b100111001        // pressure, tilt, distance, eraser, barrel, proximity
  client-name     "vncast/0.1"
                                              ServerCapabilities (type 200):
                                                display-set         1
                                                virtual-display     1620 × 2160
                                                hid-pen-attached    1
                                                accepted-fps-cap    8
                                                server-name         "rcast-host/0.1"

…regular framebuffer update flow…

(user lifts pen near the display)
TabletEvent type 101:
  tool-type 0 (pen), flags=proximity-in
  tool-serial 0xDEADBEEF, ts 1240
  axes: x=812, y=1100, distance=0xC000
                                              → HID report: In Range=1, Pressure=0,
                                                X=812, Y=1100, Distance=0.75 → routed
                                                via WM_POINTERENTER

(user touches and presses)
TabletEvent (single burst):
  flags=frame-end
  axes: x=815, y=1102, tip-down=1, tip-pressure=0x4800, tilt-x=-1500, tilt-y=420
                                              → HID report: Tip Switch=1, Pressure=0x4800,
                                                Tilt-X=-15°, Tilt-Y=4.2° → Photoshop sees
                                                a real pen-down with pressure
```

## 11. Open questions / TBD

* Encoding number `-1024` is private-use but I haven't audited every fork in the wild. Pre-finalisation, sanity-check against `tigervnc/common/rfb` and `noVNC` to make sure no one's squatting it.
* Touch-and-pen *simultaneously* needs a coexistence rule on the rcast-host side — Windows has well-defined "promote pen over touch" semantics; reproduce them exactly so palm rejection works.
* Multi-monitor: out of scope for v1. The client always asks for one display.
* Color management: rMPP is Gallery 3, color-accurate; rM2 is grayscale. Ship a `mono-palette` extension for the latter; ignore color profiles for the former until users complain.
* Audio: deliberately omitted. If desired later, layer over a separate stream — RFB-the-protocol shouldn't grow audio messages.

## 12. Versioning policy going forward

* Adding fields to existing TLVs is allowed within the same `rmcast/N` (consumers ignore unknown tags).
* Adding new pseudoencodings or message types in the reserved ranges is allowed within the same `rmcast/N` *only if* a peer that doesn't understand them still produces correct output.
* Anything that changes wire-level invariants (axis encoding, struct layout) bumps to `rmcast/N+1`. Both peers MUST commit to the lower of the two advertised versions.
