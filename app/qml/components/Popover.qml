import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T
import Forma

// Fereastră plutitoare (meniuri, command palette, detalii) cu umbră moale.
T.Popup {
    id: root

    padding: Theme.space2
    margins: Theme.space2

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionFast }
            NumberAnimation { property: "scale"; from: 0.98; to: 1; duration: Theme.motionNormal; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.motionFast }
    }

    background: Item {
        RectangularShadow {
            anchors.fill: bg
            radius: bg.radius
            offset.y: 8
            blur: 32
            spread: -4
            color: Theme.shadow
        }
        Rectangle {
            id: bg
            anchors.fill: parent
            radius: Theme.radiusLg
            color: Theme.bgElevated
            border.color: Theme.borderStrong
        }
    }

    T.Overlay.modal: Rectangle {
        color: Theme.scrim
        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
    }
}
