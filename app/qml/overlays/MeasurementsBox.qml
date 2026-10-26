import QtQuick
import Aion

// Measurements box (VCB): câmp plutitor lângă cursor. Arată valoarea curentă a uneltei („2.5 m”);
// cât timp utilizatorul tastează, arată textul tastat, cu un cursor de text.
Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property string typed: ""
    // Textul tastat nu a fost acceptat (de exemplu „abc”): câmpul clipește roșu.
    property bool invalid: false

    readonly property bool typing: typed.length > 0

    implicitWidth: content.implicitWidth + Theme.space2 * 2
    implicitHeight: 26
    radius: Theme.radiusSm
    color: Theme.bgElevated
    border.color: invalid ? Theme.danger : typing ? Theme.accent : Theme.borderStrong
    border.width: typing || invalid ? 1.5 : 1

    Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Theme.space2

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.label
            color: Theme.textTertiary
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeXs
        }
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                text: root.typing ? root.typed : (root.value.length > 0 ? root.value : "—")
                color: root.typing ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeSm
            }
            Rectangle {
                visible: root.typing
                width: 1.5
                height: 14
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.accent
                SequentialAnimation on opacity {
                    running: root.typing && !Theme.reduceMotion
                    loops: Animation.Infinite
                    NumberAnimation { to: 0; duration: 500; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1; duration: 500; easing.type: Easing.InOutQuad }
                }
            }
        }
    }

    Timer {
        id: invalidTimer
        interval: 700
        onTriggered: root.invalid = false
    }
    function flashInvalid() {
        invalid = true
        invalidTimer.restart()
    }
}
