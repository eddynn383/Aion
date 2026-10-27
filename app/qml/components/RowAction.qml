import QtQuick
import Aion

// O acțiune pe un rând de listă (Obiecte, Tag-uri, Scene). Ocupă mereu locul ei, ca rândul să nu-și schimbe
// așezarea la hover: `shown` doar o estompează. Regula comună: de la stânga la dreapta ștergerea, comenzile, stările
// (lacăt, aprins / stins), iar ochiul ultimul; o stare diferită de cea obișnuită (ascuns, blocat, stins) rămâne
// vizibilă și fără hover (`active`).
IconButton {
    id: root

    property bool shown: false // rândul e sub cursor
    property bool active: false // starea e diferită de cea obișnuită: se vede mereu

    width: 22
    height: 22
    iconSize: 13
    opacity: shown || active ? 1 : 0
    enabled: shown || active
    Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
}
