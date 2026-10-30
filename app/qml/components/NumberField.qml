import QtQuick
import QtQuick.Templates as T
import Aion
import "MathExpr.js" as MathExpr

// Câmp numeric în stil Figma: etichetă scurtă în stânga (trage de ea pentru a modifica valoarea),
// valoarea în Geist Mono, unitatea în dreapta. Acceptă expresii („1000 - 30” → 970, la Enter sau la ieșire).
// Săgețile ↑/↓ adaugă / scad 1 (Ctrl 10, Shift 100); cu zecimale, cea mai mică zecimală (Scara: 0.01 / 0.1 / 1).
T.TextField {
    id: root

    property real value: 0
    property real step: 1
    property real from: -1e12
    property real to: 1e12
    property int decimals: 0
    property string label: ""
    property string unit: ""
    // Eticheta poate fi înlocuită de un alt control (de exemplu butonul de keyframe), păstrând spațiul ei.
    property bool labelVisible: true
    // Spațiu în plus la dreapta, pentru un buton pus peste câmp (de exemplu lacătul).
    property real extraRightPadding: 0
    // Varianta îngustă (trei câmpuri pe un rând în Inspector): margini și font mai mici.
    property bool compact: false

    signal valueModified(real value)

    function format(v) { return Number(v).toFixed(root.decimals) }
    // Valoarea textului (un număr sau o expresie); NaN dacă nu e una validă.
    function parsed() { return MathExpr.evaluate(root.text) }
    // Pasul săgeților: 1 (sau cea mai mică zecimală), ×10 cu Ctrl (pe Mac și Cmd), ×100 cu Shift.
    function arrowStep(modifiers) {
        const unitStep = Math.pow(10, -root.decimals)
        if (modifiers & Qt.ShiftModifier)
            return unitStep * 100
        if (modifiers & (Qt.ControlModifier | Qt.MetaModifier))
            return unitStep * 10
        return unitStep
    }
    // O săgeată pornește din ce e scris acum în câmp (și o expresie neterminată de aplicat), altfel din valoare.
    function nudge(delta) {
        liveCommit.stop()
        const current = root.activeFocus ? root.parsed() : NaN
        root.commit((isNaN(current) ? root.value : current) + delta)
        if (root.activeFocus)
            root.selectAll()
    }

    // Trimite valoarea. `value` rămâne legat de sursa lui (Inspector-ul îl citește din document), ca să urmeze mereu
    // obiectul selectat; numai dacă nimeni nu-l actualizează (câmp de sine stătător) îl ține câmpul. `keepText`: cât
    // se tastează, textul rămâne cum l-a scris utilizatorul.
    function commit(v, keepText) {
        const clamped = Math.max(root.from, Math.min(root.to, v))
        root.valueModified(clamped)
        if (Math.abs(root.value - clamped) > 1e-9)
            root.value = clamped
        if (!keepText)
            root.text = root.format(clamped)
    }

    // Valoarea tastată se aplică și singură, după o scurtă pauză (ca săgețile), fără Enter sau click în altă parte.
    Timer {
        id: liveCommit
        interval: 600
        onTriggered: {
            const v = root.parsed()
            if (root.activeFocus && !isNaN(v) && Math.abs(v - root.value) > 1e-9)
                root.commit(v, true)
        }
    }
    onTextEdited: liveCommit.restart()

    implicitWidth: 120
    implicitHeight: Theme.controlHeight
    readonly property real gap: compact ? Theme.space1 : Theme.space2
    leftPadding: label.length > 0 ? gap + 2 + labelText.implicitWidth + gap : (compact ? Theme.space2 : Theme.space3)
    rightPadding: (unit.length > 0 ? gap + unitText.implicitWidth + gap : (compact ? Theme.space1 : Theme.space3)) + extraRightPadding
    verticalAlignment: TextInput.AlignVCenter
    horizontalAlignment: TextInput.AlignLeft
    color: Theme.textPrimary
    selectionColor: Theme.accentSubtle
    selectedTextColor: Theme.textPrimary
    font.family: Theme.fontMono
    font.pixelSize: compact ? Theme.fontSizeSm : Theme.fontSizeMd
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    // Cifre, punct sau virgulă ca separator zecimal (DoubleValidator ar accepta doar separatorul limbii sistemului: în
    // română virgula, deci „1.5” ar deveni „15”), operatori și paranteze pentru expresii. Expresia o verifică
    // MathExpr la aplicare; limitele (from / to) le aplică commit().
    validator: RegularExpressionValidator { regularExpression: /^[-+*\/()\d.,\s]*$/ }

    text: format(value)
    onValueChanged: if (!activeFocus) text = format(value)
    // Zecimalele pot veni după valoare (ordinea inițializării): textul se reface.
    onDecimalsChanged: if (!activeFocus) text = format(value)
    // După crearea completă (toate valorile inițiale legate), textul pornește din valoarea reală.
    Component.onCompleted: text = format(value)
    onEditingFinished: {
        liveCommit.stop()
        const v = parsed()
        if (isNaN(v))
            text = format(value)
        else
            commit(v)
        // Rămas în câmp (după Enter): textul selectat, ca valoarea următoare să-l înlocuiască.
        if (activeFocus)
            selectAll()
    }
    onActiveFocusChanged: if (activeFocus) selectAll()

    Keys.onUpPressed: (event) => nudge(arrowStep(event.modifiers))
    Keys.onDownPressed: (event) => nudge(-arrowStep(event.modifiers))
    Keys.onEscapePressed: { text = format(value); focus = false }

    Text {
        id: labelText
        anchors.left: parent.left
        anchors.leftMargin: root.gap + 2
        anchors.verticalCenter: parent.verticalCenter
        visible: root.label.length > 0
        opacity: root.labelVisible ? 1 : 0
        text: root.label
        color: Theme.textTertiary
        font.pixelSize: root.compact ? Theme.fontSizeXs : Theme.fontSizeSm

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
        anchors.rightMargin: Theme.space2 + 2 + root.extraRightPadding
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
