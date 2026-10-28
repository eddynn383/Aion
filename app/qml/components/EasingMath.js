.pragma library

// Curbele de easing, ca în nucleu (model::ease, ADR 0034): o curbă Bézier cubică cu capetele (0, 0) și (1, 1) și
// mânerele (x1, y1), (x2, y2), ca `cubic-bezier` din CSS, sau un salt la final (step).

function clamp01(v) {
    return Math.max(0, Math.min(1, v))
}

function bezier(a, b, s) {
    const u = 1 - s
    return 3 * u * u * s * a + 3 * u * s * s * b + s * s * s
}

// Punctul curbei la parametrul s ∈ [0, 1].
function point(e, s) {
    return { x: bezier(clamp01(e.x1), clamp01(e.x2), s), y: bezier(e.y1, e.y2, s) }
}

// Valoarea la momentul t ∈ [0, 1] (x-ul curbei e timpul; se rezolvă prin bisecție).
function ease(e, t) {
    t = clamp01(t)
    if (e.step)
        return t < 1 ? 0 : 1
    let lo = 0
    let hi = 1
    let s = t
    for (let i = 0; i < 40; ++i) {
        const x = bezier(clamp01(e.x1), clamp01(e.x2), s)
        if (Math.abs(x - t) < 1e-6)
            break
        if (x < t)
            lo = s
        else
            hi = s
        s = (lo + hi) / 2
    }
    return bezier(e.y1, e.y2, s)
}

function same(a, b) {
    return !!a && !!b && !!a.step === !!b.step && Math.abs(a.x1 - b.x1) < 1e-6 && Math.abs(a.y1 - b.y1) < 1e-6
        && Math.abs(a.x2 - b.x2) < 1e-6 && Math.abs(a.y2 - b.y2) < 1e-6
}

// Numele de afișat ale presetărilor din aplicație.
const presetTitles = {
    linear: "Liniar",
    ease: "Ease",
    easeIn: "Accelerează (in)",
    easeOut: "Încetinește (out)",
    easeInOut: "Lin (in-out)",
    easeInBack: "Cu recul la start",
    easeOutBack: "Cu depășire la final",
    step: "Salt (step)"
}

function title(e) {
    if (!e)
        return ""
    return presetTitles[e.name] ?? (e.name && e.name.length > 0 ? e.name : "Curbă proprie")
}
