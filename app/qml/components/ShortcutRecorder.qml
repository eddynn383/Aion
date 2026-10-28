import QtQuick
import Aion

// Câmpul unei scurtături (Preferințe → Scurtături): arată combinația; un click o înregistrează pe următoarea.
// Esc anulează, Backspace / Delete o șterge. Semnalul `recorded` primește secvența în format Qt („Ctrl+Shift+K”)
// sau "" pentru ștearsă.
Rectangle {
    id: root

    property string sequence: ""
    property bool recording: false
    signal recorded(string sequence)

    implicitWidth: Math.max(96, content.implicitWidth + Theme.space2 * 2)
    implicitHeight: Theme.controlHeightSm
    radius: Theme.radiusSm
    color: recording ? Theme.accentSubtle : hover.hovered ? Theme.bgHover : "transparent"
    border.color: recording ? Theme.accent : hover.hovered ? Theme.borderStrong : Theme.borderSubtle
    activeFocusOnTab: true

    // Numele tastei în formatul secvențelor Qt; gol pentru modificatorii singuri.
    function keyName(event) {
        const k = event.key
        if (k >= Qt.Key_A && k <= Qt.Key_Z)
            return String.fromCharCode(k)
        if (k >= Qt.Key_0 && k <= Qt.Key_9)
            return String.fromCharCode(k)
        if (k >= Qt.Key_F1 && k <= Qt.Key_F12)
            return "F" + (k - Qt.Key_F1 + 1)
        const named = {
            [Qt.Key_Space]: "Space", [Qt.Key_Tab]: "Tab", [Qt.Key_Return]: "Return", [Qt.Key_Enter]: "Enter",
            [Qt.Key_Left]: "Left", [Qt.Key_Right]: "Right", [Qt.Key_Up]: "Up", [Qt.Key_Down]: "Down",
            [Qt.Key_Home]: "Home", [Qt.Key_End]: "End", [Qt.Key_PageUp]: "PgUp", [Qt.Key_PageDown]: "PgDown",
            [Qt.Key_Comma]: ",", [Qt.Key_Period]: ".", [Qt.Key_Slash]: "/", [Qt.Key_Semicolon]: ";",
            [Qt.Key_Minus]: "-", [Qt.Key_Equal]: "=", [Qt.Key_Plus]: "+", [Qt.Key_BracketLeft]: "[",
            [Qt.Key_BracketRight]: "]", [Qt.Key_Backslash]: "\\", [Qt.Key_Apostrophe]: "'", [Qt.Key_QuoteLeft]: "`"
        }
        return named[k] ?? ""
    }

    function finish(event) {
        event.accepted = true
        if (event.key === Qt.Key_Escape) {
            root.recording = false
            return
        }
        if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && event.modifiers === Qt.NoModifier) {
            root.recording = false
            root.recorded("")
            return
        }
        const name = keyName(event)
        if (name.length === 0)
            return // doar un modificator: se așteaptă și tasta
        const parts = []
        if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
        if (event.modifiers & Qt.MetaModifier) parts.push("Meta")
        if (event.modifiers & Qt.AltModifier) parts.push("Alt")
        if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")
        parts.push(name)
        root.recording = false
        root.recorded(parts.join("+"))
    }

    // Cât se înregistrează, tastele nu ajung la scurtăturile aplicației.
    Keys.onShortcutOverride: (event) => event.accepted = root.recording
    Keys.onPressed: (event) => { if (root.recording) root.finish(event) }
    onActiveFocusChanged: if (!activeFocus) recording = false

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler {
        onTapped: {
            root.forceActiveFocus()
            root.recording = !root.recording
        }
    }

    Item {
        id: content
        anchors.centerIn: parent
        implicitWidth: root.recording ? hint.implicitWidth : (root.sequence.length > 0 ? kbd.implicitWidth : none.implicitWidth)
        implicitHeight: 18
        Kbd {
            id: kbd
            anchors.centerIn: parent
            visible: !root.recording && root.sequence.length > 0
            sequence: root.sequence
        }
        Text {
            id: none
            anchors.centerIn: parent
            visible: !root.recording && root.sequence.length === 0
            text: "—"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeSm
        }
        Text {
            id: hint
            anchors.centerIn: parent
            visible: root.recording
            text: "Apasă combinația…"
            color: Theme.accent
            font.pixelSize: Theme.fontSizeXs
        }
    }
}
