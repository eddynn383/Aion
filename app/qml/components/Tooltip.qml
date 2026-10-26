import QtQuick
import QtQuick.Templates as T
import Aion

// Tooltip de tip „pill”: text mic plus scurtătura, deasupra elementului.
T.ToolTip {
    id: root

    property string shortcut: ""

    // Deasupra elementului; dedesubt când nu are loc (de exemplu în bara de titlu).
    property bool placeBelow: false

    delay: 450
    timeout: -1
    // Templates nu calculează mărimea implicită; fără ea, fundalul ar avea 0 × 0.
    implicitWidth: Math.max(implicitBackgroundWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
    y: placeBelow ? (parent ? parent.height : 0) + Theme.space2 : -implicitHeight - Theme.space2
    x: (parent ? (parent.width - implicitWidth) / 2 : 0)

    onAboutToShow: {
        if (parent)
            placeBelow = parent.mapToItem(null, 0, 0).y < implicitHeight + Theme.space4
    }
    padding: Theme.space2
    leftPadding: Theme.space3
    rightPadding: shortcut.length > 0 ? Theme.space2 : Theme.space3
    margins: Theme.space2

    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionFast } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.motionFast } }

    contentItem: Row {
        spacing: Theme.space2
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeSm
        }
        Kbd {
            anchors.verticalCenter: parent.verticalCenter
            sequence: root.shortcut
        }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.bgElevated
        border.color: Theme.borderStrong
    }
}
