import QtQuick
import QtQuick.Templates as T
import Aion

// Buton cu text și iconiță opțională. Variante: "primary", "secondary", "ghost".
T.Button {
    id: root

    property string variant: "secondary"
    property string iconName: ""
    property bool small: false

    readonly property bool primary: variant === "primary"

    implicitWidth: Math.max(implicitBackgroundWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: small ? Theme.controlHeightSm : Theme.controlHeight
    leftPadding: small ? Theme.space2 + 2 : Theme.space3
    rightPadding: leftPadding
    spacing: Theme.space2
    font.pixelSize: Theme.fontSizeMd
    font.weight: Font.Medium
    focusPolicy: Qt.StrongFocus

    readonly property color foreground: primary ? Theme.accentText
        : enabled ? Theme.textPrimary : Theme.textTertiary

    contentItem: Row {
        spacing: root.spacing
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.iconName.length > 0
            name: root.iconName
            color: root.foreground
            size: Theme.iconSizeSm
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font: root.font
            color: root.foreground
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        opacity: root.enabled ? 1 : 0.5
        color: root.primary
            ? (root.down ? Qt.darker(Theme.accent, 1.1) : root.hovered ? Theme.accentHover : Theme.accent)
            : root.variant === "ghost"
                ? (root.down ? Theme.bgActive : root.hovered ? Theme.bgHover : "transparent")
                : (root.down ? Theme.bgActive : root.hovered ? Theme.bgActive : Theme.bgHover)
        border.color: root.visualFocus ? Theme.focusRing
            : root.variant === "secondary" ? Theme.borderSubtle : "transparent"
        border.width: root.visualFocus ? 2 : 1

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
}
