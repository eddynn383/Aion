import QtQuick
import QtQuick.Templates as T
import Forma

// Câmp numeric în stil Figma: etichetă scurtă în stânga (trage de ea pentru a modifica valoarea),
// valoarea în Geist Mono, unitatea în dreapta. Săgețile ↑/↓ modifică cu `step` (Shift = ×10).
T.TextField {
    id: root

    property real value: 0
    property real step: 1
    property real from: -1e12
    property real to: 1e12
    property int decimals: 0
    property string label: ""
    property string unit: ""

    signal valueModified(real value)

    function format(v) { return Number(v).toFixed(root.decimals) }

    function commit(v) {
        const clamped = Math.max(root.from, Math.min(root.to, v))
        root.value = clamped
        root.text = root.format(clamped)
        root.valueModified(clamped)
    }

    implicitWidth: 120
    implicitHeight: Theme.controlHeight
    leftPadding: label.length > 0 ? Theme.space2 + labelText.implicitWidth + Theme.space2 : Theme.space3
    rightPadding: unit.length > 0 ? Theme.space2 + unitText.implicitWidth + Theme.space2 : Theme.space3
    verticalAlignment: TextInput.AlignVCenter
    horizontalAlignment: TextInput.AlignLeft
    color: Theme.textPrimary
    selectionColor: Theme.accentSubtle
    selectedTextColor: Theme.textPrimary
    font.family: Theme.fontMono
    font.pixelSize: Theme.fontSizeMd
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    validator: DoubleValidator { bottom: root.from; top: root.to; decimals: root.decimals }

    text: format(value)
    onValueChanged: if (!activeFocus) text = format(value)
    onEditingFinished: {
        const parsed = Number(text.replace(",", "."))
        if (isNaN(parsed))
            text = format(value)
        else
            commit(parsed)
    }
    onActiveFocusChanged: if (activeFocus) selectAll()

    Keys.onUpPressed: (event) => commit(value + step * (event.modifiers & Qt.ShiftModifier ? 10 : 1))
    Keys.onDownPressed: (event) => commit(value - step * (event.modifiers & Qt.ShiftModifier ? 10 : 1))
    Keys.onEscapePressed: { text = format(value); focus = false }

    Text {
        id: labelText
        anchors.left: parent.left
        anchors.leftMargin: Theme.space2 + 2
        anchors.verticalCenter: parent.verticalCenter
        visible: root.label.length > 0
        text: root.label
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm

        // Tras orizontal pe etichetă = modificare continuă a valorii.
        DragHandler {
            id: scrub
            target: null
            xAxis.enabled: true
            yAxis.enabled: false
            cursorShape: Qt.SizeHorCursor
            property real startValue: 0
            onActiveChanged: if (active) startValue = root.value
            onTranslationChanged: if (active) root.commit(startValue + Math.round(translation.x) * root.step)
        }
        HoverHandler { cursorShape: Qt.SizeHorCursor }
    }

    Text {
        id: unitText
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2 + 2
        anchors.verticalCenter: parent.verticalCenter
        visible: root.unit.length > 0
        text: root.unit
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.bgInput
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.focusRing : root.hovered ? Theme.borderStrong : Theme.borderSubtle

        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }
}
