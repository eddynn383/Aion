import QtQuick
import Aion

// Setările de vizualizare, direct în viewport (ca bara de „shading” din Blender), lângă gizmo:
// proiecția și grid-ul, secțiunile, soarele și umbrele, stilul de afișare (iconițe) cu opțiunile lui.
// Inspector-ul din dreapta rămâne pentru selecție.
Row {
    id: root

    property var viewport
    property var document

    spacing: Theme.space2

    readonly property var styles: [
        { id: "wireframe", icon: "style-wireframe", name: "Wireframe" },
        { id: "hiddenline", icon: "style-hiddenline", name: "Linii ascunse" },
        { id: "clay", icon: "style-clay", name: "Clay (alb mat)" },
        { id: "shaded", icon: "style-shaded", name: "Materiale" },
        { id: "xray", icon: "style-xray", name: "X-Ray" }
    ]

    // Un grup de butoane lipite, pe fundalul comun (ca în Blender).
    component Group: Rectangle {
        default property alias content: groupRow.data
        implicitWidth: groupRow.implicitWidth + 4
        implicitHeight: 32
        radius: Theme.radiusMd
        color: Theme.bgElevated
        border.color: Theme.borderStrong
        Row {
            id: groupRow
            anchors.centerIn: parent
            spacing: 0
        }
    }

    component ShadeButton: IconButton {
        implicitWidth: 28
        implicitHeight: 28
        iconSize: Theme.iconSizeMd
        solidChecked: true
    }

    component Chevron: IconButton {
        property var popup
        implicitWidth: 20
        implicitHeight: 28
        iconSize: 12
        iconName: "chevron-down"
        checked: popup && popup.opened
        solidChecked: false
        onClicked: popup.opened ? popup.close() : popup.open()
    }

    // --- Încadrare, proiecție, grid ---
    Group {
        ShadeButton {
            iconName: "scan"
            tooltip: "Zoom Extents (⇧Z)"
            solidChecked: false
            onClicked: root.viewport.zoomExtents()
        }
        ShadeButton {
            iconName: root.viewport && root.viewport.orthographic ? "square" : "box"
            tooltip: root.viewport && root.viewport.orthographic ? "Proiecție paralelă (click: perspectivă)"
                                                                 : "Perspectivă (click: proiecție paralelă)"
            onClicked: root.viewport.toggleProjection()
        }
        ShadeButton {
            iconName: "grid-3x3"
            tooltip: "Grid"
            checked: root.viewport && root.viewport.gridVisible
            solidChecked: false
            onClicked: root.viewport.gridVisible = !root.viewport.gridVisible
        }
    }

    // --- Secțiuni (doar când există) ---
    Group {
        visible: root.document && root.document.sections.length > 0
        ShadeButton {
            iconName: "section"
            tooltip: "Secțiuni"
            checked: sectionsPopup.opened
            solidChecked: false
            onClicked: sectionsPopup.opened ? sectionsPopup.close() : sectionsPopup.open()

            Popover {
                id: sectionsPopup
                y: parent.height + Theme.space2
                x: parent.width - width
                width: 260
                padding: Theme.space3
                contentItem: Column {
                    spacing: Theme.space1
                    Text {
                        text: "Secțiuni"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                    }
                    Repeater {
                        model: root.document ? root.document.sections : []
                        delegate: Item {
                            id: sectionRow
                            required property var modelData
                            width: 236
                            height: Theme.controlHeight
                            Toggle {
                                anchors.left: parent.left
                                anchors.right: sectionActions.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: sectionRow.modelData.name
                                checked: sectionRow.modelData.active
                                onToggled: root.document.setSectionActive(sectionRow.modelData.id, checked)
                            }
                            Row {
                                id: sectionActions
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                IconButton {
                                    width: 24; height: 24; iconSize: Theme.iconSizeSm
                                    iconName: "arrow-down"; tooltip: "Mută 100 mm înapoi"
                                    onClicked: root.document.moveSection(sectionRow.modelData.id, -100)
                                }
                                IconButton {
                                    width: 24; height: 24; iconSize: Theme.iconSizeSm
                                    iconName: "arrow-up"; tooltip: "Mută 100 mm înainte"
                                    onClicked: root.document.moveSection(sectionRow.modelData.id, 100)
                                }
                                IconButton {
                                    width: 24; height: 24; iconSize: Theme.iconSizeSm
                                    iconName: "trash-2"; tooltip: "Șterge secțiunea"
                                    onClicked: root.document.removeSection(sectionRow.modelData.id)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // --- Soarele și umbrele ---
    Group {
        ShadeButton {
            iconName: "sun-medium"
            tooltip: "Umbre"
            checked: root.viewport && root.viewport.shadows
            solidChecked: false
            onClicked: root.viewport.shadows = !root.viewport.shadows
        }
        Chevron {
            popup: sunPopup
            tooltip: "Ora, luna"
            Popover {
                id: sunPopup
                y: parent.height + Theme.space2
                x: parent.width - width
                width: 260
                padding: Theme.space4
                contentItem: Column {
                    spacing: Theme.space2
                    Toggle {
                        width: 228
                        text: "Umbre"
                        checked: root.viewport && root.viewport.shadows
                        onToggled: root.viewport.shadows = checked
                    }
                    Slider {
                        width: 228
                        label: "Ora"
                        from: 5
                        to: 21
                        stepSize: 0.25
                        value: root.viewport ? root.viewport.sunHour : 14
                        valueText: Math.floor(value) + ":" + ("0" + Math.round((value % 1) * 60)).slice(-2)
                        onMoved: root.viewport.sunHour = value
                    }
                    Slider {
                        width: 228
                        label: "Luna"
                        from: 1
                        to: 12
                        stepSize: 1
                        value: root.viewport ? root.viewport.sunMonth : 6
                        valueText: Qt.locale("ro_RO").monthName(Math.round(value) - 1, Locale.ShortFormat)
                        onMoved: root.viewport.sunMonth = Math.round(value)
                    }
                    Text {
                        width: 228
                        wrapMode: Text.Wrap
                        text: "Latitudinea se schimbă din Preferințe (⌘,)."
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
            }
        }
    }

    // --- Stilul de afișare ---
    Group {
        Repeater {
            model: root.styles
            delegate: ShadeButton {
                required property var modelData
                iconName: modelData.icon
                tooltip: modelData.name
                checked: root.viewport && root.viewport.displayStyle === modelData.id
                onClicked: root.viewport.displayStyle = modelData.id
            }
        }
        Chevron {
            popup: stylePopup
            tooltip: "Opțiunile stilului"
            Popover {
                id: stylePopup
                y: parent.height + Theme.space2
                x: parent.width - width
                width: 240
                padding: Theme.space4
                contentItem: Column {
                    spacing: Theme.space2
                    Toggle {
                        width: 208
                        text: "Profile (contururi)"
                        checked: root.viewport && root.viewport.profiles
                        onToggled: root.viewport.profiles = checked
                    }
                    Toggle {
                        width: 208
                        text: "Ceață"
                        checked: root.viewport && root.viewport.fog
                        onToggled: root.viewport.fog = checked
                    }
                }
            }
        }
    }
}
