import QtQuick
import Aion
import "EasingMath.js" as EasingMath

// Miniatura unei curbe de easing (presetări, segmentele dintre chei): curba desenată în cutia ei, cu un spațiu
// deasupra și dedesubt pentru curbele care depășesc (back).
Canvas {
    id: root

    property var easing: null
    property color color: Theme.textSecondary
    property real lineWidth: 1.5
    // Cât din înălțime e rezervat deasupra și dedesubt pentru depășiri (fracțiune din cutia [0, 1]).
    property real overshoot: 0.25

    onEasingChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const e = root.easing
        if (!e)
            return
        const pad = root.lineWidth
        const w = width - pad * 2
        const h = (height - pad * 2) / (1 + root.overshoot * 2)
        const top = pad + h * root.overshoot
        const px = (x) => pad + x * w
        const py = (y) => top + (1 - y) * h
        ctx.strokeStyle = root.color
        ctx.lineWidth = root.lineWidth
        ctx.lineJoin = "round"
        ctx.lineCap = "round"
        ctx.beginPath()
        if (e.step) {
            ctx.moveTo(px(0), py(0))
            ctx.lineTo(px(1), py(0))
            ctx.lineTo(px(1), py(1))
        } else {
            ctx.moveTo(px(0), py(0))
            for (let i = 1; i <= 24; ++i) {
                const p = EasingMath.point(e, i / 24)
                ctx.lineTo(px(p.x), py(p.y))
            }
        }
        ctx.stroke()
    }
}
