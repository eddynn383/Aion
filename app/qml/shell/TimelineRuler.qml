import QtQuick
import Aion

// Rigla timeline-ului: gradații adaptate zoom-ului (cadre, zecimi, secunde, minute), cu etichete la cele mari.
// Click sau tragere pe riglă mută playhead-ul.
Item {
    id: root

    required property var timeline

    // Pasul gradațiilor mari: cel mai mic din listă care lasă cel puțin 64 px între etichete.
    readonly property real step: {
        const fps = Math.max(1, timeline.fps)
        const candidates = [1 / fps, 2 / fps, 5 / fps, 0.1, 0.2, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300]
        for (const s of candidates) {
            if (s * timeline.pxPerSecond >= 64)
                return s
        }
        return 600
    }
    readonly property bool frames: step < 0.1 - 1e-9
    readonly property int first: Math.max(0, Math.ceil(timeline.viewStart / step - 1e-9))
    readonly property int last: Math.floor(Math.min(timeline.duration, timeline.timeAt(width)) / step + 1e-9)

    function label(t) {
        if (root.frames)
            return Math.round(t * root.timeline.fps) + "f"
        if (t >= 60)
            return Math.floor(t / 60) + ":" + String(Math.round(t % 60)).padStart(2, "0")
        return Number(t.toFixed(2)) + "s"
    }

    clip: true

    Repeater {
        model: Math.max(0, root.last - root.first + 1)
        delegate: Item {
            required property int index
            readonly property real t: (root.first + index) * root.step
            x: root.timeline.xAt(t)
            height: root.height
            Rectangle { y: root.height - 8; width: 1; height: 8; color: Theme.borderStrong }
            // Gradațiile mici (un sfert de pas).
            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    x: (index + 1) * root.step * root.timeline.pxPerSecond / 4
                    y: root.height - 4
                    width: 1
                    height: 4
                    color: Theme.borderSubtle
                }
            }
            Text {
                x: 3
                y: 2
                text: root.label(parent.t)
                color: Theme.textTertiary
                font.pixelSize: 10
                font.family: Theme.fontMono
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.SizeHorCursor
        onPressed: (mouse) => root.timeline.scrubTo(mouse.x)
        onPositionChanged: (mouse) => { if (pressed) root.timeline.scrubTo(mouse.x) }
    }
}
