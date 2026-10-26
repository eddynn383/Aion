import QtQuick
import Aion

// Preferințe (⌘,): unitatea și precizia lungimilor, tema, latitudinea pentru soare.
// Unitățile se păstrează prin Document (QSettings); tema și latitudinea, prin Settings în Main.qml.
Popover {
    id: root

    property var document
    property var viewport

    modal: true
    anchors.centerIn: parent
    width: 460
    padding: Theme.space6

    readonly property var unitNames: ["Auto", "mm", "cm", "m", "inci", "picioare"]

    contentItem: Column {
        spacing: Theme.space4

        Row {
            width: parent.width
            Text {
                width: parent.width - closeButton.width
                anchors.verticalCenter: parent.verticalCenter
                text: "Preferințe"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
            }
            IconButton {
                id: closeButton
                iconName: "x"
                tooltip: "Închide"
                onClicked: root.close()
            }
        }

        Section {
            width: parent.width
            title: "Unități"

            Tabs {
                width: parent.width
                model: root.unitNames
                currentIndex: root.document ? root.document.displayUnit : 0
                onCurrentIndexChanged: if (root.document) root.document.displayUnit = currentIndex
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "Auto: milimetri sub 1 m, metri peste. Valorile tastate fără unitate se citesc în unitatea aleasă."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }

        Section {
            width: parent.width
            title: "Precizie"

            Tabs {
                width: parent.width
                model: ["Implicit", "0", "0.0", "0.00", "0.000"]
                currentIndex: root.document ? root.document.displayDecimals + 1 : 0
                onCurrentIndexChanged: if (root.document) root.document.displayDecimals = currentIndex - 1
            }
            Text {
                // Se reevaluează când se schimbă unitățile (dependența de displayUnit / displayDecimals).
                text: root.document
                      ? (root.document.displayUnit, root.document.displayDecimals,
                         "Exemplu: " + root.document.formatLength(2537.25) + "  ·  " + root.document.formatLength(612))
                      : ""
                color: Theme.textSecondary
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeSm
            }
        }

        Section {
            width: parent.width
            title: "Aspect"

            Tabs {
                width: parent.width
                model: ["Sistem", "Light", "Dark"]
                currentIndex: Theme.mode
                onCurrentIndexChanged: Theme.mode = currentIndex
            }
            Toggle {
                width: parent.width
                text: "Ghiduri animate la hover lung pe unelte"
                checked: UserPrefs.hoverGuides
                onToggled: UserPrefs.hoverGuides = checked
            }
        }

        Section {
            width: parent.width
            title: "Locație (pentru umbre)"

            Slider {
                width: parent.width
                label: "Latitudine"
                from: -66
                to: 66
                stepSize: 0.5
                value: root.viewport ? root.viewport.latitude : 45
                valueText: Math.abs(value).toFixed(1) + "° " + (value >= 0 ? "N" : "S")
                onMoved: if (root.viewport) root.viewport.latitude = value
            }
        }
    }
}
