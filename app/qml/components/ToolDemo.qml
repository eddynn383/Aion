import QtQuick
import Aion

// O animație scurtă, în buclă, care arată cum se folosește o unealtă: o scenă izometrică desenată vectorial
// (fără video-uri sau imagini în repo), cu cursorul, clicurile, inferențele și valorile tastate.
// `t` merge de la 0 la 1 în fiecare buclă; fiecare unealtă își desenează momentele în funcție de el.
Canvas {
    id: root

    property string tool: "select"
    property bool playing: visible
    property real t: 0

    implicitWidth: 276
    implicitHeight: 156
    renderStrategy: Canvas.Cooperative

    onTChanged: requestPaint()
    onToolChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    // Cu „Reduce Motion”: un singur cadru, spre finalul animației.
    NumberAnimation on t {
        running: root.playing && !Theme.reduceMotion
        from: 0
        to: 1
        duration: 4600
        loops: Animation.Infinite
    }
    Component.onCompleted: if (Theme.reduceMotion) t = 0.86

    readonly property var colors: ({
        bgTop: String(Theme.viewportTop), bgBottom: String(Theme.viewportBottom),
        face: String(Theme.faceFront), edge: String(Theme.edge), freeEdge: String(Theme.edgeFree),
        accent: String(Theme.accent), x: String(Theme.axisX), y: String(Theme.axisY), z: String(Theme.axisZ),
        grid: String(Theme.gridMinor), cyan: String(Theme.inferenceCyan), magenta: String(Theme.inferenceMagenta),
        text: String(Theme.textPrimary), panel: String(Theme.bgElevated), border: String(Theme.borderStrong),
        dark: Theme.dark
    })

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const W = width, H = height, c = colors
        const g = ctx.createLinearGradient(0, 0, 0, H)
        g.addColorStop(0, c.bgTop)
        g.addColorStop(1, c.bgBottom)
        ctx.fillStyle = g
        ctx.fillRect(0, 0, W, H)

        const view = { cx: W * 0.5, cy: H * 0.56, s: H / 6, yaw: 0, zoom: 1, panX: 0, panY: 0 }
        const d = makeDraw(ctx, view, c)
        const scene = scenes[tool] || scenes.select
        scene(d, t, c)
    }

    // --- Utilitare de desen ---

    function clamp01(v) { return Math.max(0, Math.min(1, v)) }
    // Progresul între două momente din buclă, cu ease-in-out.
    function phase(t, a, b) {
        const k = clamp01((t - a) / (b - a))
        return k * k * (3 - 2 * k)
    }
    function lerp(a, b, k) { return a + (b - a) * k }
    function lerp2(p, q, k) { return [lerp(p[0], q[0], k), lerp(p[1], q[1], k)] }
    function lerp3(p, q, k) { return [lerp(p[0], q[0], k), lerp(p[1], q[1], k), lerp(p[2], q[2], k)] }

    function makeDraw(ctx, view, c) {
        const d = { ctx: ctx, view: view, c: c }

        // Proiecția izometrică (axa roșie spre dreapta-sus, verde spre stânga-sus), cu rotire (orbit), zoom, pan.
        d.p = function(x, y, z) {
            const cs = Math.cos(view.yaw), sn = Math.sin(view.yaw)
            const xr = x * cs - y * sn, yr = x * sn + y * cs
            const s = view.s * view.zoom
            return [view.cx + view.panX + (xr - yr) * 0.866 * s, view.cy + view.panY - (xr + yr) * 0.5 * s - z * s]
        }
        d.path = function(points) {
            ctx.beginPath()
            for (let i = 0; i < points.length; ++i) {
                const q = d.p(points[i][0], points[i][1], points[i][2])
                if (i === 0) ctx.moveTo(q[0], q[1]); else ctx.lineTo(q[0], q[1])
            }
        }
        d.line = function(a, b, color, width, dash) {
            ctx.save()
            ctx.strokeStyle = color
            ctx.lineWidth = width || 1.2
            ctx.setLineDash(dash ? [4, 3] : [])
            d.path([a, b])
            ctx.stroke()
            ctx.restore()
        }
        d.polygon = function(points, fill, stroke, width) {
            ctx.save()
            d.path(points)
            ctx.closePath()
            if (fill) { ctx.fillStyle = fill; ctx.fill() }
            if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = width || 1.1; ctx.stroke() }
            ctx.restore()
        }
        d.grid = function() {
            ctx.save()
            ctx.strokeStyle = c.grid
            ctx.lineWidth = 1
            for (let i = -4; i <= 4; ++i) {
                d.path([[i, -4, 0], [i, 4, 0]]); ctx.stroke()
                d.path([[-4, i, 0], [4, i, 0]]); ctx.stroke()
            }
            ctx.restore()
            d.line([0, 0, 0], [4, 0, 0], c.x, 1)
            d.line([0, 0, 0], [0, 4, 0], c.y, 1)
            d.line([0, 0, 0], [0, 0, 3.4], c.z, 1)
        }
        // Umbrirea unei fețe după normală (lumina vine de sus, din stânga).
        d.shade = function(n, base, k) {
            const cs = Math.cos(view.yaw), sn = Math.sin(view.yaw)
            const nx = n[0] * cs - n[1] * sn, ny = n[0] * sn + n[1] * cs
            const light = n[2] > 0.5 ? 1.0 : 0.80 + 0.12 * (-nx * 0.6 - ny * 0.8)
            return Qt.darker(base, 1 / Math.max(0.5, light * (k || 1)))
        }
        // O cutie (opțional transformată): fețele din spate nu se desenează, celelalte se umbresc după normală.
        // opts: { color, edge, faceColors: { top, front... }, highlight: true|"top", transform(p) }
        d.box = function(o, size, opts) {
            opts = opts || {}
            const tf = opts.transform || function(q) { return q }
            const P = (i, j, k) => tf([o[0] + i * size[0], o[1] + j * size[1], o[2] + k * size[2]])
            const faces = [
                { name: "bottom", n: [0, 0, -1], v: [P(0,0,0), P(0,1,0), P(1,1,0), P(1,0,0)] },
                { name: "top",    n: [0, 0, 1],  v: [P(0,0,1), P(1,0,1), P(1,1,1), P(0,1,1)] },
                { name: "front",  n: [0, -1, 0], v: [P(0,0,0), P(1,0,0), P(1,0,1), P(0,0,1)] },
                { name: "back",   n: [0, 1, 0],  v: [P(0,1,0), P(0,1,1), P(1,1,1), P(1,1,0)] },
                { name: "left",   n: [-1, 0, 0], v: [P(0,0,0), P(0,0,1), P(0,1,1), P(0,1,0)] },
                { name: "right",  n: [1, 0, 0],  v: [P(1,0,0), P(1,1,0), P(1,1,1), P(1,0,1)] }
            ]
            const rot = opts.normalRotation || 0
            for (const f of faces) {
                const s = f.v.map(q => d.p(q[0], q[1], q[2]))
                let area = 0
                for (let i = 0; i < 4; ++i) {
                    const a = s[i], b = s[(i + 1) % 4]
                    area += a[0] * b[1] - b[0] * a[1]
                }
                if (area >= 0) continue
                const n = [f.n[0] * Math.cos(rot) - f.n[1] * Math.sin(rot), f.n[0] * Math.sin(rot) + f.n[1] * Math.cos(rot), f.n[2]]
                const base = (opts.faceColors && opts.faceColors[f.name]) || opts.color || c.face
                const hl = opts.highlight === true || opts.highlight === f.name
                d.polygon(f.v, d.shade(n, base), hl ? c.accent : (opts.edge || c.edge), hl ? 2 : 1.1)
                if (hl) d.polygon(f.v, Qt.rgba(0.23, 0.51, 0.96, 0.22), null)
            }
        }
        d.dot = function(q, color, r) {
            const s = d.p(q[0], q[1], q[2])
            ctx.save()
            ctx.fillStyle = color
            ctx.strokeStyle = c.dark ? "#000000" : "#ffffff"
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.arc(s[0], s[1], r || 3.5, 0, Math.PI * 2)
            ctx.fill()
            ctx.stroke()
            ctx.restore()
        }
        // Eticheta de inferență („Pe axa roșie”), lângă un punct 3D.
        d.pill = function(q, text, color) {
            const s = d.p(q[0], q[1], q[2])
            ctx.save()
            ctx.font = "500 9px Inter"
            const w = ctx.measureText(text).width + 18
            const x = Math.min(width - w - 4, s[0] + 8), y = Math.max(4, s[1] - 22)
            ctx.fillStyle = c.panel
            ctx.strokeStyle = c.border
            roundRect(ctx, x, y, w, 15, 7.5)
            ctx.fill()
            ctx.stroke()
            ctx.fillStyle = color
            ctx.beginPath()
            ctx.arc(x + 7, y + 7.5, 2.5, 0, Math.PI * 2)
            ctx.fill()
            ctx.fillStyle = c.text
            ctx.fillText(text, x + 12, y + 11)
            ctx.restore()
        }
        // Textul tastat (Measurements box), în colțul din dreapta-jos.
        d.vcb = function(label, value) {
            ctx.save()
            ctx.font = "9px Inter"
            const lw = ctx.measureText(label).width
            ctx.font = "10px 'Geist Mono'"
            const vw = Math.max(34, ctx.measureText(value).width)
            const w = lw + vw + 22, x = width - w - 8, y = height - 24
            ctx.fillStyle = c.panel
            ctx.strokeStyle = c.border
            roundRect(ctx, x, y, w, 17, 5)
            ctx.fill()
            ctx.stroke()
            ctx.fillStyle = Qt.darker(c.text, c.dark ? 1.6 : 0.6)
            ctx.font = "9px Inter"
            ctx.fillText(label, x + 7, y + 12)
            ctx.fillStyle = c.text
            ctx.font = "10px 'Geist Mono'"
            ctx.fillText(value, x + lw + 15, y + 12)
            ctx.restore()
        }
        d.label = function(q, text, color) {
            const s = d.p(q[0], q[1], q[2])
            ctx.save()
            ctx.font = "10px 'Geist Mono'"
            const w = ctx.measureText(text).width + 8
            ctx.fillStyle = c.panel
            roundRect(ctx, s[0] - w / 2, s[1] - 8, w, 15, 4)
            ctx.fill()
            ctx.fillStyle = color || c.text
            ctx.fillText(text, s[0] - w / 2 + 4, s[1] + 3)
            ctx.restore()
        }
        // Cursorul (săgeată) într-un punct 3D sau 2D, cu undă la click.
        d.cursor = function(q, clickK, kind) {
            const s = q.length === 3 ? d.p(q[0], q[1], q[2]) : q
            ctx.save()
            if (clickK > 0 && clickK < 1) {
                ctx.strokeStyle = c.accent
                ctx.globalAlpha = 1 - clickK
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.arc(s[0], s[1], 4 + clickK * 12, 0, Math.PI * 2)
                ctx.stroke()
                ctx.globalAlpha = 1
            }
            ctx.translate(s[0], s[1])
            if (kind === "bucket") {
                ctx.fillStyle = "#ffffff"; ctx.strokeStyle = "#111111"; ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(9, -6); ctx.lineTo(15, 3); ctx.lineTo(6, 9); ctx.closePath()
                ctx.fill(); ctx.stroke()
            } else if (kind === "eraser") {
                ctx.rotate(-0.6)
                ctx.fillStyle = "#F9A8D4"; ctx.strokeStyle = "#111111"; ctx.lineWidth = 1
                roundRect(ctx, -2, -4, 14, 8, 2); ctx.fill(); ctx.stroke()
            } else if (kind === "hand") {
                ctx.fillStyle = "#ffffff"; ctx.strokeStyle = "#111111"; ctx.lineWidth = 1
                roundRect(ctx, -5, -5, 11, 12, 4); ctx.fill(); ctx.stroke()
            } else {
                ctx.fillStyle = "#ffffff"; ctx.strokeStyle = "#111111"; ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(0, 0); ctx.lineTo(0, 13); ctx.lineTo(3.5, 9.8); ctx.lineTo(6.2, 15); ctx.lineTo(8, 14)
                ctx.lineTo(5.4, 9); ctx.lineTo(10, 9); ctx.closePath()
                ctx.fill(); ctx.stroke()
            }
            ctx.restore()
        }
        // Undă de click: k = progresul de la momentul clicului (0..1).
        d.clickK = function(t, at) { return (t - at) / 0.07 }
        return d
    }

    function roundRect(ctx, x, y, w, h, r) {
        ctx.beginPath()
        ctx.moveTo(x + r, y)
        ctx.arcTo(x + w, y, x + w, y + h, r)
        ctx.arcTo(x + w, y + h, x, y + h, r)
        ctx.arcTo(x, y + h, x, y, r)
        ctx.arcTo(x, y, x + w, y, r)
        ctx.closePath()
    }

    // Cursorul pleacă din colțul de jos-dreapta spre primul punct.
    function approach(d, t, target, until) {
        const start = [root.width - 20, root.height - 10]
        const s = d.p(target[0], target[1], target[2])
        return lerp2(start, s, phase(t, 0, until))
    }

    // --- Scenele, câte una pentru fiecare unealtă ---
    readonly property var scenes: ({
        select: function(d, t, c) {
            d.grid()
            const pick = t > 0.3 && t < 0.56
            const all = t > 0.86
            d.box([-1, -1, 0], [2.2, 2, 1.3], { highlight: all ? true : pick ? "top" : false })
            if (t < 0.56) {
                d.cursor(approach(d, t, [0.1, 0, 1.3], 0.26), d.clickK(t, 0.3))
            } else {
                const a = d.p(-2.4, -0.6, 0), b0 = d.p(1.9, 1.7, 0)
                const k = phase(t, 0.62, 0.84)
                const b = [lerp(a[0] + 0, b0[0] + 26, k), lerp(a[1] + 0, b0[1] - 72, k)]
                const corner = [a[0] - 10, a[1] + 34]
                const end = [lerp(corner[0], b[0], k), lerp(corner[1], b[1], k)]
                if (t > 0.6 && t < 0.86) {
                    d.ctx.save()
                    d.ctx.strokeStyle = c.accent
                    d.ctx.fillStyle = Qt.rgba(0.23, 0.51, 0.96, 0.12)
                    d.ctx.fillRect(corner[0], Math.min(corner[1], end[1]), end[0] - corner[0], Math.abs(end[1] - corner[1]))
                    d.ctx.strokeRect(corner[0], Math.min(corner[1], end[1]), end[0] - corner[0], Math.abs(end[1] - corner[1]))
                    d.ctx.restore()
                }
                d.cursor(t < 0.62 ? lerp2(d.p(0.1, 0, 1.3), corner, phase(t, 0.56, 0.62)) : end, 0)
            }
        },
        line: function(d, t, c) {
            d.grid()
            const A = [-1.5, -1, 0], B = [1.5, -1, 0]
            const k = phase(t, 0.3, 0.58)
            const cur = lerp3(A, B, k)
            if (t >= 0.28) {
                d.line(A, t > 0.8 ? B : cur, t > 0.8 ? c.freeEdge : c.text, 1.6)
                if (t < 0.8) {
                    d.line(cur, [cur[0] + 1, cur[1], 0], c.x, 1, true)
                    if (t > 0.4) d.pill(cur, "Pe axa roșie", c.x)
                }
            }
            if (t > 0.6) d.vcb("Lungime", t > 0.62 ? (t > 0.68 ? "3m" : "3") : "")
            d.dot(A, c.y, t < 0.28 ? 3.5 : 3)
            if (t < 0.28) {
                d.pill(A, "Capăt", c.y)
                d.cursor(approach(d, t, A, 0.24), 0)
            } else {
                d.cursor(t > 0.8 ? d.p(B[0], B[1], B[2]) : cur, Math.max(d.clickK(t, 0.28), d.clickK(t, 0.78)))
            }
            if (t > 0.8) d.dot(B, c.y, 3)
        },
        arc: function(d, t, c) {
            d.grid()
            const A = [-1.6, -0.6, 0], B = [1.6, -0.6, 0]
            const bulge = t < 0.45 ? 0 : phase(t, 0.45, 0.75) * 1.4
            const chord = phase(t, 0.15, 0.4)
            if (t > 0.15) {
                const pts = []
                const end = lerp3(A, B, chord)
                for (let i = 0; i <= 24; ++i) {
                    const u = i / 24
                    const p = lerp3(A, end, u)
                    pts.push([p[0], p[1] + 4 * bulge * u * (1 - u), 0])
                }
                d.ctx.save()
                d.ctx.strokeStyle = t > 0.8 ? c.freeEdge : c.text
                d.ctx.lineWidth = 1.6
                d.path(pts)
                d.ctx.stroke()
                d.ctx.restore()
                if (t > 0.45 && t < 0.8) d.pill([0, -0.6 + bulge, 0], "Săgeată " + Math.round(bulge * 714) + " mm", c.magenta)
            }
            d.dot(A, c.y, 3)
            const mid = [0, -0.6 + bulge, 0]
            const cur = t < 0.15 ? approach(d, t, A, 0.12)
                      : t < 0.45 ? d.p(...lerp3(A, B, chord)) : d.p(mid[0], mid[1], 0)
            d.cursor(cur, Math.max(d.clickK(t, 0.15), d.clickK(t, 0.42), d.clickK(t, 0.78)))
        },
        rectangle: function(d, t, c) {
            d.grid()
            const A = [-1.4, -1.2, 0]
            const k = phase(t, 0.28, 0.62)
            const w = 3 * k, h = 2.2 * k
            if (t > 0.26) {
                const pts = [A, [A[0] + w, A[1], 0], [A[0] + w, A[1] + h, 0], [A[0], A[1] + h, 0]]
                d.polygon(pts, t > 0.82 ? d.shade([0, 0, 1], c.face) : Qt.rgba(0.23, 0.51, 0.96, 0.10),
                          t > 0.82 ? c.edge : c.text, 1.4)
                if (t < 0.82 && k > 0.2) d.label([A[0] + w / 2, A[1] - 0.3, 0], Math.round(w * 1000) + " mm")
            }
            if (t > 0.64) d.vcb("Dimensiuni", t > 0.7 ? "3000,2200" : "3000,")
            const cur = t < 0.28 ? approach(d, t, A, 0.24) : d.p(A[0] + w, A[1] + h, 0)
            d.cursor(cur, Math.max(d.clickK(t, 0.28), d.clickK(t, 0.8)))
        },
        circle: function(d, t, c) {
            d.grid()
            const C = [0, 0, 0]
            const r = phase(t, 0.28, 0.66) * 1.6
            if (t > 0.26) {
                const pts = []
                for (let i = 0; i < 32; ++i) pts.push([Math.cos(i / 32 * 2 * Math.PI) * r, Math.sin(i / 32 * 2 * Math.PI) * r, 0])
                d.polygon(pts, t > 0.82 ? d.shade([0, 0, 1], c.face) : Qt.rgba(0.23, 0.51, 0.96, 0.10), t > 0.82 ? c.edge : c.text, 1.4)
                if (t < 0.82) d.line(C, [r, 0, 0], c.x, 1, true)
            }
            if (t > 0.68) d.vcb("Rază", "1600")
            d.dot(C, c.cyan, 3)
            d.cursor(t < 0.28 ? approach(d, t, C, 0.24) : d.p(r, 0, 0), Math.max(d.clickK(t, 0.28), d.clickK(t, 0.8)))
        },
        polygon: function(d, t, c) {
            d.grid()
            const r = phase(t, 0.28, 0.66) * 1.6
            if (t > 0.26) {
                const pts = []
                for (let i = 0; i < 6; ++i) pts.push([Math.cos(i / 6 * 2 * Math.PI) * r, Math.sin(i / 6 * 2 * Math.PI) * r, 0])
                d.polygon(pts, t > 0.82 ? d.shade([0, 0, 1], c.face) : Qt.rgba(0.23, 0.51, 0.96, 0.10), t > 0.82 ? c.edge : c.text, 1.4)
            }
            if (t > 0.68) d.vcb("Laturi", "6s")
            d.dot([0, 0, 0], c.cyan, 3)
            d.cursor(t < 0.28 ? approach(d, t, [0, 0, 0], 0.24) : d.p(r, 0, 0), Math.max(d.clickK(t, 0.28), d.clickK(t, 0.8)))
        },
        pushpull: function(d, t, c) {
            d.grid()
            const h = phase(t, 0.32, 0.7) * 1.5
            if (h < 0.02) {
                d.polygon([[-1.2, -1, 0], [1.2, -1, 0], [1.2, 1, 0], [-1.2, 1, 0]], d.shade([0, 0, 1], c.face),
                          t > 0.2 ? c.accent : c.edge, t > 0.2 ? 2 : 1.1)
            } else {
                d.box([-1.2, -1, 0], [2.4, 2, h], { highlight: t < 0.8 ? "top" : false })
                if (t < 0.8) d.label([1.5, -1, h / 2], Math.round(h * 1000) + " mm")
            }
            if (t > 0.72) d.vcb("Distanță", "1.5m")
            const top = [0, 0, h]
            d.cursor(t < 0.3 ? approach(d, t, top, 0.26) : d.p(top[0], top[1], top[2]), Math.max(d.clickK(t, 0.3), d.clickK(t, 0.8)))
        },
        offset: function(d, t, c) {
            d.grid()
            d.box([-1.3, -1.1, 0], [2.6, 2.2, 0.8], { highlight: t > 0.24 && t < 0.3 ? "top" : false })
            const k = phase(t, 0.32, 0.7) * 0.45
            if (t > 0.3) {
                d.polygon([[-1.3 + k, -1.1 + k, 0.8], [1.3 - k, -1.1 + k, 0.8], [1.3 - k, 1.1 - k, 0.8], [-1.3 + k, 1.1 - k, 0.8]],
                          null, t > 0.8 ? c.edge : c.accent, 1.5)
                if (t < 0.8) d.label([0, -1.1 + k, 0.8], Math.round(k * 1000) + " mm")
            }
            if (t > 0.72) d.vcb("Distanță", "450")
            d.cursor(t < 0.3 ? approach(d, t, [1.3, 0, 0.8], 0.26) : d.p(1.3 - k, 0, 0.8), Math.max(d.clickK(t, 0.3), d.clickK(t, 0.8)))
        },
        followme: function(d, t, c) {
            d.grid()
            d.box([-1, -1, 0], [2, 2, 1], {})
        },
        move: function(d, t, c) {
            d.grid()
            const dx = phase(t, 0.32, 0.72) * 2
            if (t > 0.3 && t < 0.82) d.box([-2, -0.8, 0], [1.6, 1.6, 1.1], { color: Qt.rgba(0.5, 0.5, 0.5, 0.25), edge: Qt.rgba(0.5, 0.5, 0.5, 0.5) })
            d.box([-2 + dx, -0.8, 0], [1.6, 1.6, 1.1], { highlight: t > 0.1 })
            const corner = [-2 + dx, -0.8, 0]
            if (t > 0.3 && t < 0.82) {
                d.line([-2, -0.8, 0], corner, c.x, 1.2, true)
                if (dx > 0.3) d.pill(corner, "Pe axa roșie", c.x)
            }
            if (t > 0.74) d.vcb("Distanță", "2m")
            d.dot(corner, c.y, 3)
            d.cursor(t < 0.3 ? approach(d, t, corner, 0.26) : d.p(...corner), Math.max(d.clickK(t, 0.3), d.clickK(t, 0.8)))
        },
        rotate: function(d, t, c) {
            d.grid()
            const pivot = [-1, -1, 0]
            const angle = phase(t, 0.45, 0.78) * Math.PI / 6
            const rot = q => {
                const x = q[0] - pivot[0], y = q[1] - pivot[1]
                return [pivot[0] + x * Math.cos(angle) - y * Math.sin(angle), pivot[1] + x * Math.sin(angle) + y * Math.cos(angle), q[2]]
            }
            d.box([-1, -1, 0], [2.2, 1.4, 1], { transform: rot, normalRotation: angle, highlight: t > 0.1 })
            if (t > 0.22 && t < 0.86) {
                const pts = []
                for (let i = 0; i <= 40; ++i) pts.push([pivot[0] + Math.cos(i / 40 * 2 * Math.PI) * 1.4, pivot[1] + Math.sin(i / 40 * 2 * Math.PI) * 1.4, 0])
                d.ctx.save(); d.ctx.strokeStyle = c.z; d.ctx.lineWidth = 1.5; d.path(pts); d.ctx.stroke(); d.ctx.restore()
                d.line(pivot, [pivot[0] + 1.4, pivot[1], 0], c.z, 1, true)
                if (t > 0.45) {
                    d.line(pivot, [pivot[0] + 1.4 * Math.cos(angle), pivot[1] + 1.4 * Math.sin(angle), 0], c.z, 1.4)
                    d.pill([pivot[0] + 1.4, pivot[1] + 0.6, 0], Math.round(angle * 180 / Math.PI) + "°", c.z)
                }
            }
            if (t > 0.78) d.vcb("Unghi", "30")
            const handle = t < 0.42 ? [pivot[0] + 1.4 * phase(t, 0.26, 0.4), pivot[1], 0]
                                    : [pivot[0] + 1.4 * Math.cos(angle), pivot[1] + 1.4 * Math.sin(angle), 0]
            d.cursor(t < 0.24 ? approach(d, t, pivot, 0.2) : d.p(...handle),
                     Math.max(d.clickK(t, 0.24), d.clickK(t, 0.42), d.clickK(t, 0.8)))
        },
        scale: function(d, t, c) {
            d.grid()
            const s = 1 + phase(t, 0.32, 0.72) * 0.5
            const size = [1.6 * s, 1.4 * s, 1 * s]
            d.box([-1, -1, 0], size, { highlight: true })
            const handles = [[-1, -1, 0], [-1 + size[0], -1, 0], [-1 + size[0], -1 + size[1], 0], [-1, -1, size[2]],
                             [-1 + size[0], -1, size[2]], [-1 + size[0], -1 + size[1], size[2]], [-1, -1 + size[1], size[2]]]
            for (const h of handles) d.dot(h, c.y, 2.6)
            const grip = [-1 + size[0], -1, size[2]]
            if (t > 0.32 && t < 0.82) d.pill(grip, "× " + s.toFixed(2), c.y)
            if (t > 0.74) d.vcb("Factor", "1.5")
            d.cursor(t < 0.3 ? approach(d, t, grip, 0.26) : d.p(...grip), Math.max(d.clickK(t, 0.3), d.clickK(t, 0.8)))
        },
        eraser: function(d, t, c) {
            d.grid()
            d.box([-1.2, -1, 0], [2.4, 2, 1], {})
            const A = [-1.2, -1, 1], B = [1.2, 1, 1]
            const hit = t > 0.36 && t < 0.62
            if (t < 0.62) d.line(A, B, hit ? c.accent : c.edge, hit ? 2.2 : 1.1)
            const start = [-0.6, 1.2, 1], end = [0.6, -1.2, 1]
            const k = phase(t, 0.3, 0.6)
            d.cursor(t < 0.3 ? approach(d, t, start, 0.26) : d.p(...lerp3(start, end, k)), d.clickK(t, 0.3), "eraser")
        },
        paint: function(d, t, c) {
            d.grid()
            const brick = "#A8523A", wood = "#B08154"
            d.box([-1.2, -1, 0], [2.4, 2, 1.2], {
                faceColors: { front: t > 0.38 ? brick : null, top: t > 0.7 ? wood : null }
            })
            const front = [0, -1, 0.6], top = [0, 0, 1.2]
            const cur = t < 0.36 ? approach(d, t, front, 0.32) : t < 0.68 ? lerp2(d.p(...front), d.p(...top), phase(t, 0.5, 0.66)) : d.p(...top)
            d.cursor(cur, Math.max(d.clickK(t, 0.36), d.clickK(t, 0.68)), "bucket")
        },
        tape: function(d, t, c) {
            d.grid()
            d.box([-1.2, -0.6, 0], [2.4, 1.6, 1], {})
            const off = phase(t, 0.32, 0.7) * 0.9
            const y = -0.6 - off
            if (t > 0.3) {
                d.line([-3, y, 0], [3, y, 0], c.textTertiary || c.freeEdge, 1, true)
                d.line([0, -0.6, 0], [0, y, 0], c.text, 1)
                if (t < 0.86) d.label([0.4, y - 0.15, 0], Math.round(off * 1000) + " mm")
            }
            if (t > 0.74) d.vcb("Distanță", "900")
            d.cursor(t < 0.3 ? approach(d, t, [0, -0.6, 0], 0.26) : d.p(0, y, 0), Math.max(d.clickK(t, 0.3), d.clickK(t, 0.8)))
        },
        protractor: function(d, t, c) {
            d.grid()
            const V = [-1.5, -1, 0]
            const angle = phase(t, 0.45, 0.78) * Math.PI / 6
            if (t > 0.2) {
                const pts = []
                for (let i = 0; i <= 40; ++i) pts.push([V[0] + Math.cos(i / 40 * 2 * Math.PI) * 1.1, V[1] + Math.sin(i / 40 * 2 * Math.PI) * 1.1, 0])
                d.ctx.save(); d.ctx.strokeStyle = c.z; d.ctx.lineWidth = 1.4; d.path(pts); d.ctx.stroke(); d.ctx.restore()
                d.line(V, [V[0] + 3.5, V[1], 0], c.x, 1)
            }
            if (t > 0.45) {
                d.line(V, [V[0] + 4 * Math.cos(angle), V[1] + 4 * Math.sin(angle), 0], c.freeEdge, 1, true)
                d.pill([V[0] + 1.1, V[1] + 0.3, 0], Math.round(angle * 180 / Math.PI) + "°", c.z)
            }
            if (t > 0.78) d.vcb("Unghi", "30")
            const handle = t < 0.42 ? [V[0] + 1.1 * phase(t, 0.24, 0.4), V[1], 0] : [V[0] + 1.1 * Math.cos(angle), V[1] + 1.1 * Math.sin(angle), 0]
            d.cursor(t < 0.22 ? approach(d, t, V, 0.18) : d.p(...handle), Math.max(d.clickK(t, 0.22), d.clickK(t, 0.42), d.clickK(t, 0.8)))
        },
        dimension: function(d, t, c) {
            d.grid()
            d.box([-1, -0.5, 0], [2, 1.4, 1], {})
            const A = [-1, -0.5, 0], B = [1, -0.5, 0]
            const off = phase(t, 0.5, 0.75) * 0.8
            if (t > 0.48) {
                const a = [A[0], A[1] - off, 0], b = [B[0], B[1] - off, 0]
                d.line(A, a, c.text, 0.8); d.line(B, b, c.text, 0.8); d.line(a, b, c.text, 1.2)
                d.dot(a, c.text, 1.8); d.dot(b, c.text, 1.8)
                d.label([0, -0.5 - off, 0], "2000 mm")
            }
            d.dot(A, c.y, t > 0.18 ? 3 : 0.1)
            if (t > 0.38) d.dot(B, c.y, 3)
            const cur = t < 0.2 ? approach(d, t, A, 0.16) : t < 0.4 ? d.p(...lerp3(A, B, phase(t, 0.22, 0.38))) : d.p(0.6, -0.5 - off, 0)
            d.cursor(cur, Math.max(d.clickK(t, 0.2), d.clickK(t, 0.4), d.clickK(t, 0.78)))
        },
        text: function(d, t, c) {
            d.grid()
            d.box([-1.2, -0.8, 0], [2.4, 1.6, 1.2], {})
            const P = [-1.2, 0, 0.9]
            const L = lerp3(P, [-2.2, -1.4, 1.9], phase(t, 0.28, 0.5))
            if (t > 0.26) {
                d.line(P, L, c.text, 1)
                d.dot(P, c.text, 2)
            }
            if (t > 0.5) {
                const full = "Ușa"
                const n = Math.min(full.length, Math.floor(phase(t, 0.52, 0.72) * full.length + 0.999))
                d.label(L, full.slice(0, n) + (t < 0.76 ? "|" : ""))
            }
            d.cursor(t < 0.26 ? approach(d, t, P, 0.22) : d.p(...L), Math.max(d.clickK(t, 0.26), d.clickK(t, 0.5)))
        },
        section: function(d, t, c) {
            d.grid()
            const z = 2.2 - phase(t, 0.25, 0.7) * 1.4
            const h = Math.min(1.5, z)
            d.box([-1.2, -1, 0], [2.4, 2, h], { faceColors: { top: z < 1.5 ? (c.dark ? "#5b6170" : "#9aa1ad") : null } })
            d.polygon([[-2, -1.8, z], [2, -1.8, z], [2, 1.8, z], [-2, 1.8, z]], Qt.rgba(0.96, 0.62, 0.04, 0.10), "#F59E0B", 1.4)
            d.cursor(t < 0.25 ? approach(d, t, [1.8, -1.8, z], 0.22) : d.p(1.8, -1.8, z), d.clickK(t, 0.25), "hand")
        },
        interact: function(d, t, c) {
            d.grid()
            // Un dulap cu ușa pe balamaua din stânga; click pe ușă → se deschide spre față.
            d.box([-1.2, -0.8, 0], [2.4, 1.6, 1.8], {})
            const open = t < 0.3 ? 0 : t < 0.55 ? phase(t, 0.3, 0.55) : t < 0.72 ? 1 : 1 - phase(t, 0.72, 0.95)
            const angle = -open * Math.PI / 2
            const pivot = [-1.2, -0.8]
            const rot = q => {
                const x = q[0] - pivot[0], y = q[1] - pivot[1]
                return [pivot[0] + x * Math.cos(angle) - y * Math.sin(angle), pivot[1] + x * Math.sin(angle) + y * Math.cos(angle), q[2]]
            }
            d.box([-1.2, -0.88, 0.05], [2.4, 0.08, 1.7], { transform: rot, normalRotation: angle, color: "#B08154" })
            const knob = rot([0.9, -0.92, 0.9])
            d.cursor(t < 0.28 ? approach(d, t, knob, 0.24) : d.p(...knob), Math.max(d.clickK(t, 0.28), d.clickK(t, 0.7)), "hand")
        },
        orbit: function(d, t, c) {
            d.view.yaw = Math.sin(t * Math.PI * 2) * 0.7
            d.grid()
            d.box([-1, -0.8, 0], [2, 1.6, 1.2], { normalRotation: d.view.yaw })
            d.cursor([root.width * 0.5 + Math.sin(t * Math.PI * 2) * 60, root.height * 0.3], 0, "hand")
        },
        pan: function(d, t, c) {
            d.view.panX = Math.sin(t * Math.PI * 2) * 40
            d.view.panY = Math.sin(t * Math.PI * 4) * 8
            d.grid()
            d.box([-1, -0.8, 0], [2, 1.6, 1.2], {})
            d.cursor([root.width * 0.7 + d.view.panX, root.height * 0.3 + d.view.panY], 0, "hand")
        },
        zoom: function(d, t, c) {
            d.view.zoom = 0.8 + (1 - Math.cos(t * Math.PI * 2)) * 0.35
            d.grid()
            d.box([-1, -0.8, 0], [2, 1.6, 1.2], {})
            d.cursor([root.width * 0.75, root.height * 0.3 - (d.view.zoom - 0.8) * 50], 0)
        }
    })
}
