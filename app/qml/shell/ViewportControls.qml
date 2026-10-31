import QtQuick
import Aion

// Setările de vizualizare, direct în viewport (ca bara de „shading” din Blender), lângă gizmo:
// proiecția și grid-ul, secțiunile, soarele și umbrele, stilul de afișare (iconițe) cu opțiunile lui.
// Inspector-ul din dreapta rămâne pentru selecție.
Row {
    id: root

    property var viewport
    property var document
    // Mediul ales din meniul soarelui (Main îl salvează în preferințe, iar viewport-ul îl urmează).
    signal environmentChosen(string key)

    spacing: Theme.space2

    readonly property var styles: [
        { id: "wireframe", icon: "style-wireframe", name: qsTrId("ViewportControls.wireframe") },
        { id: "hiddenline", icon: "style-hiddenline", name: qsTrId("ViewportControls.linii_ascunse") },
        { id: "clay", icon: "style-clay", name: qsTrId("ViewportControls.clay_alb_mat") },
        { id: "shaded", icon: "style-shaded", name: qsTrId("ViewportControls.materiale") },
        { id: "xray", icon: "style-xray", name: qsTrId("ViewportControls.x_ray") }
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
            tooltip: qsTrId("ViewportControls.zoom_extents_z")
            solidChecked: false
            onClicked: root.viewport.zoomExtents()
        }
        ShadeButton {
            iconName: root.viewport && root.viewport.orthographic ? "square" : "box"
            tooltip: root.viewport && root.viewport.orthographic ? qsTrId("ViewportControls.proiectie_paralela_click_perspectiva")
                                                                 : qsTrId("ViewportControls.perspectiva_click_proiectie_paralela")
            onClicked: root.viewport.toggleProjection()
        }
        ShadeButton {
            iconName: "grid-3x3"
            tooltip: qsTrId("ViewportControls.grid")
            checked: root.viewport && root.viewport.gridVisible
            solidChecked: false
            onClicked: root.viewport.gridVisible = !root.viewport.gridVisible
        }
    }

    // --- Ușile corpurilor de mobilier: închise / deschise (doar afișarea, în Modelare) ---
    Group {
        visible: root.document && root.document.hasDoors && root.document.workMode === 0
        ShadeButton {
            iconName: "door-open"
            tooltip: root.document && root.document.doorsOpen ? qsTrId("ViewportControls.usile_deschise_click_inchise")
                                                              : qsTrId("ViewportControls.usile_inchise_click_deschise_ca_sa_vezi_int")
            checked: root.document && root.document.doorsOpen
            onClicked: root.document.doorsOpen = !root.document.doorsOpen
        }
    }

    // --- Secțiuni (doar când există) ---
    Group {
        visible: root.document && root.document.sections.length > 0
        ShadeButton {
            iconName: "section"
            tooltip: qsTrId("ViewportControls.sectiuni")
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
                        text: qsTrId("ViewportControls.sectiuni")
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
                                    iconName: "arrow-down"; tooltip: qsTrId("ViewportControls.muta_100_mm_inapoi")
                                    onClicked: root.document.moveSection(sectionRow.modelData.id, -100)
                                }
                                IconButton {
                                    width: 24; height: 24; iconSize: Theme.iconSizeSm
                                    iconName: "arrow-up"; tooltip: qsTrId("ViewportControls.muta_100_mm_inainte")
                                    onClicked: root.document.moveSection(sectionRow.modelData.id, 100)
                                }
                                IconButton {
                                    width: 24; height: 24; iconSize: Theme.iconSizeSm
                                    iconName: "trash-2"; tooltip: qsTrId("ViewportControls.sterge_sectiunea")
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
            tooltip: qsTrId("ViewportControls.umbre")
            checked: root.viewport && root.viewport.shadows
            solidChecked: false
            onClicked: root.viewport.shadows = !root.viewport.shadows
        }
        Chevron {
            popup: sunPopup
            tooltip: qsTrId("ViewportControls.ora_luna_mediul")
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
                        text: qsTrId("ViewportControls.umbre")
                        checked: root.viewport && root.viewport.shadows
                        onToggled: root.viewport.shadows = checked
                    }
                    Slider {
                        width: 228
                        label: qsTrId("ViewportControls.ora")
                        from: 5
                        to: 21
                        stepSize: 0.25
                        value: root.viewport ? root.viewport.sunHour : 14
                        valueText: Math.floor(value) + ":" + ("0" + Math.round((value % 1) * 60)).slice(-2)
                        onMoved: root.viewport.sunHour = value
                    }
                    Slider {
                        width: 228
                        label: qsTrId("ViewportControls.luna")
                        from: 1
                        to: 12
                        stepSize: 1
                        value: root.viewport ? root.viewport.sunMonth : 6
                        valueText: Qt.locale("ro_RO").monthName(Math.round(value) - 1, Locale.ShortFormat)
                        onMoved: root.viewport.sunMonth = Math.round(value)
                    }
                    Text {
                        text: qsTrId("ViewportControls.mediu")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSm
                    }
                    Tabs {
                        width: 228
                        readonly property var keys: ["simplu", "studio", "exterior", "innorat"]
                        model: [qsTrId("ViewportControls.simplu"), qsTrId("ViewportControls.studio"), qsTrId("ViewportControls.ext"), qsTrId("ViewportControls.innorat")]
                        currentIndex: root.viewport ? Math.max(0, keys.indexOf(root.viewport.environment)) : 0
                        onCurrentIndexChanged: if (root.viewport && keys[currentIndex] !== root.viewport.environment)
                            root.environmentChosen(keys[currentIndex])
                    }
                    Text {
                        width: 228
                        wrapMode: Text.Wrap
                        text: qsTrId("ViewportControls.latitudinea_umbrirea_ambientala_si_calitate")
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
            tooltip: qsTrId("ViewportControls.optiunile_stilului")
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
                        text: qsTrId("ViewportControls.profile_contururi")
                        checked: root.viewport && root.viewport.profiles
                        onToggled: root.viewport.profiles = checked
                    }
                    Toggle {
                        width: 208
                        text: qsTrId("ViewportControls.ceata")
                        checked: root.viewport && root.viewport.fog
                        onToggled: root.viewport.fog = checked
                    }
                }
            }
        }
    }
}
