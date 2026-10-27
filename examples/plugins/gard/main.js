// Plugin-exemplu pentru Aion (vezi docs/scripting.md). Copiază dosarul în dosarul plugin-urilor
// (consola de scripturi → 📁) și apasă ↻.

function gard(x0, y0, length, height) {
    const posts = Math.max(2, Math.round(length / 1000) + 1);
    const span = length / (posts - 1);
    for (let i = 0; i < posts; i++) {
        aion.material(aion.box(x0 + i * span - 50, y0 - 50, 0, 100, 100, height + 150, "Stâlp " + (i + 1)), "Lemn de nuc");
    }
    for (let x = 0; x + 120 <= length; x += 150) {
        aion.material(aion.box(x0 + x, y0 + 60, 100, 120, 20, height, "Scândură"), "Lemn de pin");
    }
}

function gard6() {
    gard(0, 0, 6000, 1200);
}

// Un gard pe latura din față a fiecărui obiect selectat (cu 500 mm în fața lui).
function gardSelectie() {
    const selected = aion.selection();
    if (selected.length === 0) {
        aion.message("Selectează întâi obiectele.");
        return;
    }
    for (const o of aion.objects()) {
        if (selected.indexOf(o.id) >= 0) {
            gard(o.x, o.y - 500, Math.max(1000, o.width), 1000);
        }
    }
}
