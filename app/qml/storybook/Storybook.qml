import QtQuick
import Aion

// Pagină internă (numai în debug) cu toate componentele, pentru review vizual în ambele teme.
Rectangle {
    id: root

    signal closeRequested()

    color: Theme.bgApp

    component Specimen: Column {
        property string title
        default property alias content: row.data
        spacing: Theme.space2
        Text {
            text: parent.title
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }
        Flow {
            id: row
            width: parent.width
            spacing: Theme.space3
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + Theme.space8 * 2
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            x: Theme.space8
            y: Theme.space8
            width: Math.min(root.width - Theme.space8 * 2, 880)
            spacing: Theme.space6

            Row {
                width: parent.width
                spacing: Theme.space3
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Componente"
                    color: Theme.textPrimary
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
                Item { width: parent.width - 520; height: 1 }
                Tabs {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 220
                    model: ["Sistem", "Light", "Dark"]
                    currentIndex: Theme.mode
                    onCurrentIndexChanged: Theme.mode = currentIndex
                }
                Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Închide"
                    iconName: "x"
                    variant: "ghost"
                    onClicked: root.closeRequested()
                }
            }

            Specimen {
                width: column.width
                title: "Tipografie"
                Column {
                    spacing: Theme.space1
                    Text { text: "Inter 15 · Modelator 3D modern"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeLg; font.weight: Font.DemiBold }
                    Text { text: "Inter 13 · Textul obișnuit al interfeței, cu diacritice: ăâîșț"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeMd }
                    Text { text: "Inter 12 · Text secundar"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSm }
                    Text { text: "Geist Mono · 2 500 mm · 1 234,56 · (12.5, −3.0, 400)"; color: Theme.textPrimary; font.family: Theme.fontMono; font.pixelSize: Theme.fontSizeMd }
                }
            }

            Specimen {
                width: column.width
                title: "Culori"
                Repeater {
                    model: [
                        ["bgApp", Theme.bgApp], ["bgPanel", Theme.bgPanel], ["bgElevated", Theme.bgElevated],
                        ["accent", Theme.accent], ["textPrimary", Theme.textPrimary], ["textSecondary", Theme.textSecondary],
                        ["axisX", Theme.axisX], ["axisY", Theme.axisY], ["axisZ", Theme.axisZ]
                    ]
                    delegate: Column {
                        required property var modelData
                        spacing: Theme.space1
                        Rectangle {
                            width: 72; height: 40; radius: Theme.radiusMd
                            color: modelData[1]
                            border.color: Theme.borderStrong
                        }
                        Text { text: modelData[0]; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXs }
                    }
                }
            }

            Specimen {
                width: column.width
                title: "Button"
                Button { text: "Primar"; variant: "primary" }
                Button { text: "Secundar" }
                Button { text: "Ghost"; variant: "ghost" }
                Button { text: "Cu iconiță"; iconName: "plus" }
                Button { text: "Mic"; small: true; iconName: "check" }
                Button { text: "Dezactivat"; enabled: false }
            }

            Specimen {
                width: column.width
                title: "IconButton și Tooltip"
                IconButton { iconName: "mouse-pointer-2"; tooltip: "Select"; shortcut: "Space"; checked: true }
                IconButton { iconName: "pen-line"; tooltip: "Line"; shortcut: "L" }
                IconButton { iconName: "square"; tooltip: "Rectangle"; shortcut: "R" }
                IconButton { iconName: "eye"; tooltip: "Tentă subtilă"; checked: true; solidChecked: false }
            }

            Specimen {
                width: column.width
                title: "Input și NumberField"
                Input { width: 260; placeholderText: "Caută…"; iconName: "search" }
                Input { width: 200; text: "Casa.aion" }
                NumberField { width: 140; label: "X"; unit: "mm"; value: 2500 }
                NumberField { width: 140; label: "Y"; unit: "mm"; value: -1200 }
                NumberField { width: 120; label: "∠"; unit: "°"; value: 45; decimals: 1; step: 0.5 }
            }

            Specimen {
                width: column.width
                title: "Tabs, Toggle, Kbd"
                Tabs { width: 240; model: ["Outliner", "Tag-uri", "Scene"] }
                Toggle { text: "Grid"; checked: true }
                Toggle { text: "Umbre" }
                Kbd { sequence: "Ctrl+K" }
                Kbd { sequence: "Ctrl+Shift+Z" }
                Kbd { sequence: "Space" }
            }

            Specimen {
                width: column.width
                title: "Panel, Section, Separator"
                Panel {
                    width: 260
                    height: 200
                    title: "Inspector"
                    border.color: Theme.borderSubtle
                    radius: Theme.radiusMd
                    Section {
                        width: parent.width
                        title: "Transform"
                        NumberField { width: parent.width; label: "X"; unit: "mm"; value: 0 }
                        NumberField { width: parent.width; label: "Y"; unit: "mm"; value: 1500 }
                    }
                }
                Column {
                    spacing: Theme.space3
                    Separator { width: 200 }
                    Separator { width: 200; color: Theme.borderStrong }
                }
            }

            Specimen {
                width: column.width
                title: "Menu"
                Button {
                    id: menuButton
                    text: "Deschide meniul"
                    iconName: "chevron-down"
                    onClicked: demoMenu.open()
                    Menu {
                        id: demoMenu
                        y: menuButton.height + Theme.space1
                        items: [
                            { id: "move", icon: "move", name: "Move", shortcut: "M", checked: true },
                            { id: "rotate", icon: "rotate-cw", name: "Rotate", shortcut: "Q" },
                            { id: "scale", icon: "scale-3d", name: "Scale", shortcut: "S", enabled: false }
                        ]
                    }
                }
            }

            Specimen {
                width: column.width
                title: "Popover"
                Button {
                    id: popButton
                    text: "Deschide popover"
                    onClicked: pop.open()
                    Popover {
                        id: pop
                        y: popButton.height + Theme.space1
                        width: 240
                        contentItem: Column {
                            spacing: Theme.space2
                            Text { text: "Popover"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeMd; font.weight: Font.DemiBold }
                            Text { width: 220; wrapMode: Text.Wrap; text: "Fereastră plutitoare cu umbră moale și margine subtilă."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSm }
                        }
                    }
                }
            }
        }
    }
}
