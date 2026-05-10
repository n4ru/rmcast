// Active session view. Shows header + status line + Disconnect while we're
// connecting; once frames are flowing the chrome hides and the FrameView
// fills the full panel for true fullscreen mirror.
import QtQuick 2.15
import net.example.Vncast 1.0
// xofm.libs.epaper is xochitl's internal QML module that exposes
// ScreenModeItem — a QQuickItem that tags the panel region it covers
// with a specific EPDC waveform. This is the actual API rm-appload uses
// to get fast B&W refresh; we use the same lever here.
import xofm.libs.epaper 1.0 as Epaper

Rectangle {
    id: root
    color: "white"
    anchors.fill: parent
    signal requestClose()

    Component.onCompleted: console.log("[vncast/session.qml] loaded")

    readonly property bool fullscreen: Vncast.sessionStatus === "Receiving frames"

    // ---- header (visible only while connecting) ----
    Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 168
        visible: !root.fullscreen

        Text {
            anchors {
                left: parent.left
                bottom: parent.bottom
                bottomMargin: 24
                leftMargin: 160
            }
            text: "Cast"
            font.family: "EB Garamond"
            font.weight: Font.Normal
            font.pixelSize: 56
            color: "black"
        }
        Text {
            anchors {
                right: parent.right
                bottom: parent.bottom
                bottomMargin: 32
                rightMargin: 160
            }
            text: Settings.host + ":" + Settings.port
            font.family: "Noto Sans"
            font.pixelSize: 20
            color: "black"
            opacity: 0.6
        }
    }

    // ---- live status line (visible only while connecting) ----
    Text {
        id: statusLine
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            leftMargin: 160
            rightMargin: 160
            topMargin: 8
        }
        visible: !root.fullscreen
        text: Vncast.sessionStatus || ""
        font.family: "Noto Sans"
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: "black"
        opacity: 0.8
        wrapMode: Text.WordWrap
    }

    // ---- panel waveform tag ----
    // Fast B&W ON  → pin to Animation (A2) — ~120ms two-level B&W refresh
    //                regardless of frame content. Best for text-heavy work.
    // Fast B&W OFF → bind to the per-frame mode hint vnsee classified from
    //                the dirty-rect size (cursor → Animation, text scroll →
    //                Mono, page change → Content). This is the asivery
    //                trick: dynamic mode per frame, whole-area tagged.
    function _frameHintToMode(hint) {
        // 0xFFFFFFFF = unset / never received → safe default.
        if (hint === undefined || hint < 0 || hint > 4)
            return Epaper.ScreenModeItem.UI;
        // FrameMode integers line up 1:1 with EPScreenModeItem::Mode.
        return [
            Epaper.ScreenModeItem.Pen,
            Epaper.ScreenModeItem.Mono,
            Epaper.ScreenModeItem.Animation,
            Epaper.ScreenModeItem.Content,
            Epaper.ScreenModeItem.UI
        ][hint];
    }
    function _waveformToMode(name) {
        if (name === "PEN")  return Epaper.ScreenModeItem.Pen;
        if (name === "DU")   return Epaper.ScreenModeItem.Mono;
        if (name === "GC16") return Epaper.ScreenModeItem.Content;
        return Epaper.ScreenModeItem.Animation; // A2 default
    }
    Epaper.ScreenModeItem {
        anchors.fill: frameView
        visible: root.fullscreen
        // Fast B&W ON  → pin to user-chosen waveform (Settings.waveform).
        // Fast B&W OFF → per-frame mode hint from vnsee's classifier.
        mode: Settings.grayscale
            ? root._waveformToMode(Settings.waveform)
            : root._frameHintToMode(Vncast.qtfbServer
                                    ? Vncast.qtfbServer.lastFrameMode
                                    : -1)
    }

    // ---- per-region cursor tag (rotation-aware) ----
    QtObject {
        id: cursorMap
        property real srcW: Vncast.qtfbServer ? Vncast.qtfbServer.w : 0
        property real srcH: Vncast.qtfbServer ? Vncast.qtfbServer.h : 0
        property real logW: (frameView.rotation === 90 || frameView.rotation === 270)
                            ? srcH : srcW
        property real logH: (frameView.rotation === 90 || frameView.rotation === 270)
                            ? srcW : srcH
        property real scale: {
            if (logW <= 0 || logH <= 0) return 1;
            return Math.min(frameView.width / logW, frameView.height / logH);
        }
        property real drawnW: logW * scale
        property real drawnH: logH * scale
        property real offsetX: (frameView.width  - drawnW) / 2
        property real offsetY: (frameView.height - drawnH) / 2
        property int  csX: Vncast.qtfbServer ? Vncast.qtfbServer.cursorX : -1
        property int  csY: Vncast.qtfbServer ? Vncast.qtfbServer.cursorY : -1
        property int  csW: Vncast.qtfbServer ? Vncast.qtfbServer.cursorW : 0
        property int  csH: Vncast.qtfbServer ? Vncast.qtfbServer.cursorH : 0
        property real logX: (frameView.rotation === 90)  ? (srcH - csY - csH)
                          : (frameView.rotation === 270) ? csY
                          : csX
        property real logY: (frameView.rotation === 90)  ? csX
                          : (frameView.rotation === 270) ? (srcW - csX - csW)
                          : csY
        property real logCW: (frameView.rotation === 90 || frameView.rotation === 270) ? csH : csW
        property real logCH: (frameView.rotation === 90 || frameView.rotation === 270) ? csW : csH
        property real finalX: frameView.x + offsetX + logX * scale
        property real finalY: frameView.y + offsetY + logY * scale
        property real finalW: Math.max(48, logCW * scale)
        property real finalH: Math.max(48, logCH * scale)
    }
    Epaper.ScreenModeItem {
        id: cursorTag
        x:      cursorMap.finalX
        y:      cursorMap.finalY
        width:  cursorMap.finalW
        height: cursorMap.finalH
        visible: root.fullscreen
                 && Vncast.qtfbServer
                 && Vncast.qtfbServer.cursorVisible
                 && Vncast.qtfbServer.cursorX >= 0
        mode: Epaper.ScreenModeItem.Animation
    }

    // ---- frame view ----
    // Aspect-aware rotation:
    //   vnsee pre-rotates landscape source into a panel-portrait shm
    //   (1620×2160). When the user holds the tablet portrait, the QML
    //   scene matches the shm aspect — show as-is, the cast fills the
    //   panel. When the user holds the tablet landscape, xochitl rotates
    //   the QML scene 90° so frameView's visible bounds become
    //   landscape-shaped. Without compensation the portrait shm gets
    //   squeezed into the rotated bounds (the "started in landscape"
    //   bug). Detect the aspect mismatch and rotate the FrameView to
    //   undo it, so the cast fills regardless of how the device is held.
    //
    //   The Flip button below adds 180° on top, for the case where the
    //   detected rotation orients the content the wrong way up. We don't
    //   try to auto-detect upside-down (CW vs CCW) — too brittle without
    //   ground-truth orientation data we trust on this firmware.
    property bool flipped: false
    FrameView {
        id: frameView
        server: Vncast.qtfbServer
        rotation: {
            var srv = Vncast.qtfbServer
            var base = 0
            if (srv && srv.w > 0 && srv.h > 0 && width > 0 && height > 0) {
                var shmPortrait  = srv.h > srv.w
                var viewPortrait = height > width
                if (shmPortrait !== viewPortrait) base = 90
            }
            return (base + (root.flipped ? 180 : 0)) % 360
        }
        anchors {
            top:    root.fullscreen ? parent.top    : statusLine.bottom
            left:   parent.left
            right:  parent.right
            bottom: root.fullscreen ? parent.bottom : disconnectRow.top
            margins: root.fullscreen ? 0 : 16
            topMargin: root.fullscreen ? 0 : 24
        }
    }

    // ---- disconnect row (chrome) ----
    Item {
        id: disconnectRow
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 160
            bottomMargin: 40
        }
        height: 76
        visible: !root.fullscreen
        FilledButton {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            width: 280
            height: 76
            text: "Disconnect"
            onTapped: { Vncast.stopSession(); root.requestClose() }
        }
    }

    // ---- session action buttons ----
    // Tries the xochitl-internal ghost buster first; falls back to a brief
    // forced full-quality mode cycle (Content) which produces a complete
    // GC16 panel pass that overwrites all the ghosting from prior A2/Pen
    // frames. Used by the Refresh button AND automatically when toggling
    // to Pen (which has no built-in ghost cleanup).
    Timer {
        id: clearReturnTimer
        interval: 700  // long enough for one full GC16 cycle on rMPP
        repeat: false
        onTriggered: forceClearOverlay.visible = false
    }
    Epaper.ScreenModeItem {
        id: forceClearOverlay
        anchors.fill: frameView
        visible: false
        mode: Epaper.ScreenModeItem.Content
    }
    function _forceClear() {
        // Best effort: poke ghostBuster if it's in scope (xochitl injects
        // it as a context property in some QML scenes). Wrapped in a try
        // because the property doesn't exist in every scene.
        try {
            if (typeof ghostBuster !== "undefined" && ghostBuster) {
                ghostBuster.forceClearNow("vncast manual refresh");
                return;
            }
        } catch (e) { /* fall through */ }
        // Fallback: layered Content-mode item over the cast surface for
        // ~700ms forces a full GC16 panel pass which clears ghosting.
        forceClearOverlay.visible = true;
        clearReturnTimer.restart();
    }

    Row {
        id: actionRow
        anchors {
            top:   parent.top
            right: parent.right
            margins: 16
        }
        visible: root.fullscreen
        spacing: 8
        // (Live B&W toggle removed for stability. Reconnecting mid-session
        // races the scenegraph render thread reading the old shm against
        // the launcher tearing it down to reallocate with the new format.
        // The setting still lives in the Cast config screen where the
        // session-start path negotiates cleanly with no live race. A
        // dual-buffered shm approach would let us bring the live toggle
        // back, but it's a bigger change than belongs in this iteration.)
        OutlineButton {
            width: 110
            height: 56
            // Toggles a 180° flip on top of the auto-detected rotation.
            // Useful when the auto pick is the right axis but upside-down
            // (e.g. tablet held with the camera at the bottom edge).
            text: root.flipped ? "Flip ✓" : "Flip"
            onTapped: root.flipped = !root.flipped
        }
        OutlineButton {
            width: 110
            height: 56
            text: "Refresh"
            onTapped: root._forceClear()
        }
        FilledButton {
            width: 180
            height: 56
            text: "Disconnect"
            onTapped: { Vncast.stopSession(); root.requestClose() }
        }
    }
}
