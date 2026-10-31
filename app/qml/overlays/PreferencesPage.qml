import QtQuick
import QtQuick.Dialogs
import QtQuick.Effects
import Aion

// Preferințele (⌘,): un modal peste aplicație, cu categoriile în stânga și setările categoriei în dreapta, grupate
// în carduri (titlu și explicație în stânga rândului, controlul în dreapta). Totul se aplică imediat și se păstrează
// între sesiuni (unitățile și autosalvarea prin Document, restul prin Settings / UserPrefs).
Item {
    id: root

    property var document
    // Preferințele salvate ale aplicației (Settings din Main): grafica se scrie acolo, viewport-ul le urmează.
    property var settings
    property var viewport
    property var commands: []
    property int category: 0

    signal closeRequested()
    // Scurtăturile personalizate: o secvență nouă ("" = fără), null = revine la cea implicită.
    signal shortcutChanged(string id, var sequence)
    signal shortcutsReset()
    // Mesajul ultimei schimbări de scurtătură (de exemplu combinația luată de la altă comandă).
    property string shortcutNotice: ""

    readonly property var categories: [
        { name: qsTrId("PreferencesPage.general"), icon: "settings", description: qsTrId("PreferencesPage.ajutorul_din_aplicatie_ecranul_de_start_si_d") },
        { name: qsTrId("PreferencesPage.unitati"), icon: "ruler", description: qsTrId("PreferencesPage.cum_se_afiseaza_si_cum_se_citesc_lungimile") },
        { name: qsTrId("PreferencesPage.salvare"), icon: "check", description: qsTrId("PreferencesPage.copiile_de_recuperare_ale_documentului_desch") },
        { name: qsTrId("PreferencesPage.aspect"), icon: "palette", description: qsTrId("PreferencesPage.tema_culoarea_de_accent_si_culorile_interfet") },
        { name: qsTrId("PreferencesPage.viewport"), icon: "monitor", description: qsTrId("PreferencesPage.calitatea_randarii_lumina_si_informatiile_di") },
        { name: qsTrId("PreferencesPage.navigare"), icon: "orbit", description: qsTrId("PreferencesPage.orbita_zoom_ul_si_planurile_de_taiere_ale_ca") },
        { name: qsTrId("PreferencesPage.grid_si_snap"), icon: "grid-3x3", description: qsTrId("PreferencesPage.grid_ul_de_pe_sol_si_prinderea_punctelor_de_") },
        { name: qsTrId("PreferencesPage.scurtaturi"), icon: "search", description: qsTrId("PreferencesPage.tastele_comenzilor_si_ale_uneltelor_click_pe") },
        { name: qsTrId("PreferencesPage.mobilier"), icon: "box", description: qsTrId("PreferencesPage.cantul_cum_se_vede_in_viewport_si_cat_se_ada") }
    ]

    Keys.onEscapePressed: root.closeRequested()
    Component.onCompleted: forceActiveFocus()

    // Fundalul estompat: blochează aplicația de dedesubt; un click pe el închide.
    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        opacity: card.opacity
        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRequested()
            onWheel: (wheel) => wheel.accepted = true
        }
    }

    RectangularShadow {
        anchors.fill: card
        radius: card.radius
        offset.y: 12
        blur: 48
        spread: -4
        color: Theme.shadow
        opacity: card.opacity
        scale: card.scale
    }

    Rectangle {
        id: card
        width: Math.min(920, root.width - Theme.space8 * 2)
        height: Math.min(660, root.height - Theme.space8 * 2)
        anchors.centerIn: parent
        radius: Theme.radiusLg
        color: Theme.bgElevated
        border.color: Theme.borderStrong
        clip: true

        // Intră ușor (mărire + opacitate), ca un dialog.
        opacity: 0
        scale: 0.97
        Component.onCompleted: { opacity = 1; scale = 1 }
        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
        Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }

        // Clicurile din card nu ajung la fundal (care l-ar închide).
        MouseArea { anchors.fill: parent }

        // --- Categoriile (stânga) ---
        Rectangle {
            id: sidebar
            width: 220
            height: parent.height
            color: Theme.bgPanel
            radius: card.radius
            // Colțurile din dreapta ale barei nu se rotunjesc (o bandă dreaptă le acoperă).
            Rectangle {
                anchors.right: parent.right
                width: card.radius
                height: parent.height
                color: parent.color
            }

            Text {
                id: sidebarTitle
                x: Theme.space6
                y: Theme.space6
                text: qsTrId("PreferencesPage.preferinte")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
            }

            Column {
                id: nav
                anchors.top: sidebarTitle.bottom
                anchors.topMargin: Theme.space4
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.space3
                spacing: 2
                Repeater {
                    model: root.categories
                    delegate: Rectangle {
                        id: navItem
                        required property var modelData
                        required property int index
                        readonly property bool current: root.category === index
                        width: nav.width
                        height: Theme.controlHeight
                        radius: Theme.radiusMd
                        color: current ? Theme.accentSubtle : navArea.containsMouse ? Theme.bgHover : "transparent"
                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space3
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.space2
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: navItem.modelData.icon
                                size: Theme.iconSizeSm
                                color: navItem.current ? Theme.accent : Theme.textSecondary
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: navItem.modelData.name
                                color: navItem.current ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.fontSizeMd
                                font.weight: navItem.current ? Font.Medium : Font.Normal
                            }
                        }
                        MouseArea {
                            id: navArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { root.category = navItem.index; content.contentY = 0 }
                        }
                    }
                }
            }
        }
        Rectangle {
            x: sidebar.width
            width: 1
            height: parent.height
            color: Theme.borderSubtle
        }

        // --- Antetul paginii (dreapta) ---
        Item {
            id: pageHeader
            x: sidebar.width + 1
            width: parent.width - x
            height: headerTexts.implicitHeight + Theme.space6 * 2
            Column {
                id: headerTexts
                anchors.left: parent.left
                anchors.right: closeButton.left
                anchors.leftMargin: Theme.space6
                anchors.rightMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    text: root.categories[root.category].name
                    color: Theme.textPrimary
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: root.categories[root.category].description
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeSm
                }
            }
            IconButton {
                id: closeButton
                anchors.right: parent.right
                anchors.rightMargin: Theme.space4
                anchors.top: parent.top
                anchors.topMargin: Theme.space4
                iconName: "x"
                tooltip: qsTrId("PreferencesPage.inchide_esc")
                onClicked: root.closeRequested()
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.borderSubtle
                opacity: content.contentY > 1 ? 1 : 0
            }
        }

        // --- Setările categoriei ---
        Flickable {
            id: content
            x: sidebar.width + 1
            anchors.top: pageHeader.bottom
            anchors.bottom: parent.bottom
            width: parent.width - x
            contentHeight: page.height + Theme.space6
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: page
                x: Theme.space6
                width: content.width - Theme.space6 * 2
                spacing: Theme.space6

                // General
                Column {
                    visible: root.category === 0
                    width: parent.width
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.limba")
                        SettingsRow {
                            stacked: true
                            title: qsTrId("PreferencesPage.limba_interfetei")
                            description: qsTrId("PreferencesPage.se_aplica_dupa_repornire_traducerea_e_in_luc")
                            Flow {
                                width: parent.width
                                spacing: Theme.space2
                                Repeater {
                                    model: [{ key: "ro", name: qsTrId("PreferencesPage.romana") }, { key: "en", name: qsTrId("PreferencesPage.english") },
                                        { key: "de", name: qsTrId("PreferencesPage.deutsch_beta") }, { key: "pl", name: qsTrId("PreferencesPage.polski_beta") },
                                        { key: "it", name: qsTrId("PreferencesPage.italiano_beta") }, { key: "fr", name: qsTrId("PreferencesPage.francais_beta") },
                                        { key: "es", name: qsTrId("PreferencesPage.espanol_beta") }, { key: "pt", name: qsTrId("PreferencesPage.portugues_beta") }]
                                    delegate: Button {
                                        required property var modelData
                                        small: true
                                        text: modelData.name
                                        variant: UserPrefs.language === modelData.key ? "primary" : "secondary"
                                        onClicked: UserPrefs.language = modelData.key
                                    }
                                }
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.ajutor")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.ghiduri_animate")
                            description: qsTrId("PreferencesPage.un_ghid_scurt_apare_cand_tii_cursorul_pe_o_u")
                            Toggle {
                                checked: UserPrefs.hoverGuides
                                onToggled: UserPrefs.hoverGuides = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.pornire")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.ecranul_de_start")
                            description: qsTrId("PreferencesPage.la_pornire_document_nou_deschide_ultimul_fis")
                            Toggle {
                                checked: UserPrefs.startScreen
                                onToggled: UserPrefs.startScreen = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.documente_recente")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.lista_documentelor_recente")
                            description: root.document ? (root.document.recentFiles.length === 1 ? qsTrId("PreferencesPage.un_document_in_lista")
                                : root.document.recentFiles.length + qsTrId("PreferencesPage.documente_in_lista")) : ""
                            Button {
                                text: qsTrId("PreferencesPage.goleste_lista")
                                enabled: root.document ? root.document.recentFiles.length > 0 : false
                                onClicked: root.document.clearRecentFiles()
                            }
                        }
                    }
                }

                // Unități
                Column {
                    visible: root.category === 1
                    width: parent.width
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.lungimi")
                        SettingsRow {
                            stacked: true
                            title: qsTrId("PreferencesPage.unitatea")
                            description: qsTrId("PreferencesPage.auto_milimetri_sub_1_m_metri_peste_valorile_")
                            Tabs {
                                width: parent.width
                                model: [qsTrId("PreferencesPage.auto"), "mm", "cm", "m", "inci", "picioare"]
                                currentIndex: root.document ? root.document.displayUnit : 0
                                onCurrentIndexChanged: if (root.document) root.document.displayUnit = currentIndex
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.precizie")
                            description: qsTrId("PreferencesPage.cate_zecimale_se_afiseaza")
                            Tabs {
                                width: 300
                                model: [qsTrId("PreferencesPage.implicit"), "0", "0.0", "0.00", "0.000"]
                                currentIndex: root.document ? root.document.displayDecimals + 1 : 0
                                onCurrentIndexChanged: if (root.document) root.document.displayDecimals = currentIndex - 1
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.exemplu")
                            Text {
                                text: root.document
                                      ? (root.document.displayUnit, root.document.displayDecimals,
                                         root.document.formatLength(2537.25) + "  ·  " + root.document.formatLength(612))
                                      : ""
                                color: Theme.textSecondary
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontSizeSm
                            }
                        }
                    }
                }

                // Salvare
                Column {
                    visible: root.category === 2
                    width: parent.width
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.autosalvare")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.salveaza_automat_o_copie_de_recuperare")
                            description: qsTrId("PreferencesPage.doar_cand_documentul_are_modificari_nesalvat")
                            Toggle {
                                checked: root.document ? root.document.autosaveEnabled : false
                                onToggled: root.document.autosaveEnabled = checked
                            }
                        }
                        SettingsRow {
                            visible: root.document ? root.document.autosaveEnabled : false
                            height: visible ? implicitHeight : 0
                            title: qsTrId("PreferencesPage.intervalul")
                            Slider {
                                width: 260
                                from: 1
                                to: 30
                                stepSize: 1
                                value: root.document ? root.document.autosaveMinutes : 1
                                valueText: Math.round(value) + (Math.round(value) === 1 ? qsTrId("PreferencesPage.minut") : qsTrId("PreferencesPage.minute"))
                                onMoved: root.document.autosaveMinutes = Math.round(value)
                            }
                        }
                    }
                }

                // Aspect
                Column {
                    visible: root.category === 3
                    width: parent.width
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.interfata")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.tema")
                            description: qsTrId("PreferencesPage.sistem_urmeaza_setarea_calculatorului")
                            Tabs {
                                width: 260
                                model: [qsTrId("PreferencesPage.sistem"), qsTrId("PreferencesPage.light"), qsTrId("PreferencesPage.dark")]
                                currentIndex: Theme.mode
                                onCurrentIndexChanged: Theme.mode = currentIndex
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.culoarea_de_accent")
                            Row {
                                spacing: Theme.space2
                                Repeater {
                                    model: ["#3B82F6", "#8B5CF6", "#EC4899", "#EF4444", "#F59E0B", "#10B981", "#14B8A6", "#64748B"]
                                    delegate: Rectangle {
                                        required property string modelData
                                        width: 22
                                        height: 22
                                        radius: 11
                                        color: modelData
                                        border.color: Qt.colorEqual(Theme.accent, modelData) ? Theme.textPrimary : "transparent"
                                        border.width: 2
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: Theme.setOverride("accent", parent.modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.culori_personalizate_tema") + (Theme.dark ? "dark" : "light") + ")"
                        Repeater {
                            model: [
                                { key: "accent", name: qsTrId("PreferencesPage.accent") },
                                { key: "bgApp", name: qsTrId("PreferencesPage.fundalul_aplicatiei") },
                                { key: "bgPanel", name: qsTrId("PreferencesPage.panouri") },
                                { key: "bgElevated", name: qsTrId("PreferencesPage.ferestre_plutitoare") },
                                { key: "viewportTop", name: qsTrId("PreferencesPage.viewport_sus") },
                                { key: "viewportBottom", name: qsTrId("PreferencesPage.viewport_jos") },
                                { key: "faceFront", name: qsTrId("PreferencesPage.fetele_fata") },
                                { key: "faceBack", name: qsTrId("PreferencesPage.fetele_verso") },
                                { key: "edge", name: qsTrId("PreferencesPage.muchiile") }
                            ]
                            delegate: SettingsRow {
                                id: colorRow
                                required property var modelData
                                readonly property bool custom: Theme.overrides[modelData.key] !== undefined
                                compact: true
                                title: modelData.name
                                Row {
                                    spacing: Theme.space2
                                    IconButton {
                                        anchors.verticalCenter: parent.verticalCenter
                                        opacity: colorRow.custom ? 1 : 0
                                        enabled: colorRow.custom
                                        width: 24; height: 24; iconSize: Theme.iconSizeSm
                                        iconName: "rotate-ccw"
                                        tooltip: qsTrId("PreferencesPage.revino_la_culoarea_implicita")
                                        onClicked: Theme.setOverride(colorRow.modelData.key, "")
                                    }
                                    Rectangle {
                                        width: 44
                                        height: 24
                                        radius: Theme.radiusSm
                                        color: Theme[colorRow.modelData.key]
                                        border.color: Theme.borderStrong
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                colorDialog.key = colorRow.modelData.key
                                                colorDialog.selectedColor = Theme[colorRow.modelData.key]
                                                colorDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Button {
                        anchors.right: parent.right
                        text: qsTrId("PreferencesPage.reseteaza_tema") + (Theme.dark ? "dark" : "light")
                        iconName: "rotate-ccw"
                        onClicked: Theme.resetOverrides()
                    }
                }

                // Viewport
                Column {
                    visible: root.category === 4
                    width: parent.width
                    spacing: Theme.space6
                    // Preset-urile: aceleași setări de mai jos, alese dintr-un click.
                    SettingsGroup {
                        id: qualitySection
                        width: parent.width
                        title: qsTrId("PreferencesPage.calitate")
                        readonly property var presets: [
                            { name: qsTrId("PreferencesPage.performanta"), msaa: 2, shadows: 1024, ao: false, aa: false },
                            { name: qsTrId("PreferencesPage.echilibrat"), msaa: 4, shadows: 2048, ao: true, aa: true },
                            { name: qsTrId("PreferencesPage.calitate"), msaa: 8, shadows: 4096, ao: true, aa: true } ]
                        readonly property int current: root.settings ? presets.findIndex(p => p.msaa === root.settings.msaa
                            && p.shadows === root.settings.shadowQuality && p.ao === root.settings.ambientOcclusion
                            && p.aa === root.settings.progressiveAA) : -1
                        SettingsRow {
                            title: qsTrId("PreferencesPage.preset")
                            description: qualitySection.current < 0 ? qsTrId("PreferencesPage.personalizat_setarile_de_mai_jos")
                                : qsTrId("PreferencesPage.performanta_modele_mari_sau_calculatoare_mai")
                            Tabs {
                                width: 330
                                model: qualitySection.presets.map(p => p.name)
                                currentIndex: qualitySection.current
                                onCurrentIndexChanged: {
                                    const p = qualitySection.presets[currentIndex]
                                    if (!p || !root.settings || currentIndex === qualitySection.current)
                                        return
                                    root.settings.msaa = p.msaa
                                    root.settings.shadowQuality = p.shadows
                                    root.settings.ambientOcclusion = p.ao
                                    root.settings.progressiveAA = p.aa
                                }
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.randare")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.anti_aliasing_msaa")
                            Tabs {
                                width: 260
                                model: [qsTrId("PreferencesPage.fara"), "2×", "4×", "8×"]
                                currentIndex: root.settings ? [1, 2, 4, 8].indexOf(root.settings.msaa) : 2
                                onCurrentIndexChanged: if (root.settings && currentIndex >= 0) root.settings.msaa = [1, 2, 4, 8][currentIndex]
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.margini_mai_fine_cu_camera_oprita")
                            description: qsTrId("PreferencesPage.trece_la_8_cat_camera_sta_pe_loc")
                            Toggle {
                                checked: root.settings ? root.settings.progressiveAA : true
                                onToggled: root.settings.progressiveAA = checked
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.calitatea_umbrelor")
                            Tabs {
                                width: 260
                                model: [qsTrId("PreferencesPage.scazuta"), qsTrId("PreferencesPage.medie"), qsTrId("PreferencesPage.inalta")]
                                currentIndex: root.settings ? Math.max(0, [1024, 2048, 4096].indexOf(root.settings.shadowQuality)) : 1
                                onCurrentIndexChanged: if (root.settings) root.settings.shadowQuality = [1024, 2048, 4096][currentIndex]
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.umbrire_ambientala_ssao")
                            description: qsTrId("PreferencesPage.colturile_si_contactele_dintre_obiecte_mai_i")
                            Toggle {
                                checked: root.settings ? root.settings.ambientOcclusion : true
                                onToggled: root.settings.ambientOcclusion = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.lumina")
                        SettingsRow {
                            stacked: true
                            title: qsTrId("PreferencesPage.mediul_de_iluminare")
                            description: qsTrId("PreferencesPage.culoarea_luminii_ambientale_si_reflexiile_pe")
                            Tabs {
                                width: parent.width
                                readonly property var keys: ["simplu", "studio", "exterior", "innorat"]
                                model: [qsTrId("PreferencesPage.simplu"), qsTrId("PreferencesPage.studio"), qsTrId("PreferencesPage.exterior"), qsTrId("PreferencesPage.innorat")]
                                currentIndex: root.settings ? Math.max(0, keys.indexOf(root.settings.environment)) : 0
                                onCurrentIndexChanged: if (root.settings) root.settings.environment = keys[currentIndex]
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.latitudinea")
                            description: qsTrId("PreferencesPage.pentru_pozitia_soarelui_si_umbre")
                            Slider {
                                width: 260
                                from: -66
                                to: 66
                                stepSize: 0.5
                                value: root.viewport ? root.viewport.latitude : 45
                                valueText: Math.abs(value).toFixed(1) + "° " + (value >= 0 ? qsTrId("PreferencesPage.n") : qsTrId("PreferencesPage.s"))
                                onMoved: if (root.viewport) root.viewport.latitude = value
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.informatii")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.arata_fps_in_bara_de_jos")
                            Toggle {
                                checked: UserPrefs.showFps
                                onToggled: UserPrefs.showFps = checked
                            }
                        }
                    }
                }

                // Navigare
                Column {
                    visible: root.category === 5
                    width: parent.width
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.camera")
                        SettingsRow {
                            stacked: true
                            title: qsTrId("PreferencesPage.centrul_orbitei")
                            description: qsTrId("PreferencesPage.in_jurul_carui_punct_se_roteste_camera_la_or")
                            Tabs {
                                width: parent.width
                                model: [qsTrId("PreferencesPage.centrul_vederii"), qsTrId("PreferencesPage.selectia"), qsTrId("PreferencesPage.punctul_de_sub_cursor")]
                                currentIndex: root.viewport ? root.viewport.orbitPivot : 0
                                onCurrentIndexChanged: if (root.viewport) root.viewport.orbitPivot = currentIndex
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.zoom_spre_suprafata_de_sub_cursor")
                            description: qsTrId("PreferencesPage.zoom_ul_incetineste_langa_detalii_si_nu_trec")
                            Toggle {
                                checked: root.viewport ? root.viewport.zoomToSurface : true
                                onToggled: if (root.viewport) root.viewport.zoomToSurface = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.planuri_de_taiere")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.clip_start")
                            description: qsTrId("PreferencesPage.0_automat_dupa_distanta_camerei_si_marimea_m")
                            NumberField {
                                width: 180
                                unit: "mm"
                                from: 0
                                to: 100000
                                step: 0.1
                                decimals: 3
                                value: root.viewport ? root.viewport.clipStart : 0
                                onValueModified: (v) => { if (root.viewport) root.viewport.clipStart = v }
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.clip_end")
                            description: qsTrId("PreferencesPage.0_automat")
                            NumberField {
                                width: 180
                                unit: "m"
                                from: 0
                                to: 1000000
                                step: 10
                                value: root.viewport ? root.viewport.clipEnd / 1000 : 0
                                onValueModified: (v) => { if (root.viewport) root.viewport.clipEnd = v * 1000 }
                            }
                        }
                    }
                }

                // Grid și snap
                Column {
                    visible: root.category === 6
                    width: parent.width
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.grid")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.arata_grid_ul")
                            Toggle {
                                checked: root.viewport ? root.viewport.gridVisible : false
                                onToggled: root.viewport.gridVisible = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.snap_la_grid") + AppInfo.shortcutText("Alt+G") + ")"
                        SettingsRow {
                            title: qsTrId("PreferencesPage.punctele_libere_cad_pe_grid")
                            description: qsTrId("PreferencesPage.doar_punctele_fara_alta_inferenta_capat_mijl")
                            Toggle {
                                checked: root.document ? root.document.snapToGrid : false
                                onToggled: if (root.document) root.document.snapToGrid = checked
                            }
                        }
                        SettingsRow {
                            stacked: true
                            title: qsTrId("PreferencesPage.pasul")
                            description: qsTrId("PreferencesPage.auto_urmeaza_grid_ul_afisat_se_schimba_cu_zo")
                            Tabs {
                                width: parent.width
                                readonly property var steps: [0, 1, 5, 10, 50, 100, 500, 1000]
                                model: [qsTrId("PreferencesPage.auto"), qsTrId("PreferencesPage.1_mm"), qsTrId("PreferencesPage.5_mm"), qsTrId("PreferencesPage.1_cm"), qsTrId("PreferencesPage.5_cm"), qsTrId("PreferencesPage.10_cm"), qsTrId("PreferencesPage.50_cm"), qsTrId("PreferencesPage.1_m")]
                                currentIndex: root.document ? Math.max(0, steps.indexOf(root.document.gridStep)) : 0
                                onCurrentIndexChanged: if (root.document) root.document.gridStep = steps[currentIndex]
                            }
                        }
                    }
                }

                // Mobilier: cantul.
                Column {
                    visible: root.category === 8
                    width: parent.width
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: qsTrId("PreferencesPage.cant")
                        SettingsRow {
                            title: qsTrId("PreferencesPage.liniile_de_cant_in_viewport")
                            description: qsTrId("PreferencesPage.laturile_placilor_cu_cant_au_o_linie_in_culo")
                            Toggle {
                                checked: UserPrefs.showBands
                                onToggled: UserPrefs.showBands = checked
                            }
                        }
                        SettingsRow {
                            title: qsTrId("PreferencesPage.adaosul_pe_muchie")
                            description: qsTrId("PreferencesPage.cat_cant_in_plus_se_numara_la_fiecare_muchie")
                            NumberField {
                                width: 180
                                unit: "mm"
                                from: 0
                                to: 500
                                value: UserPrefs.bandAllowance
                                onValueModified: (v) => UserPrefs.bandAllowance = v
                            }
                        }
                        Repeater {
                            model: root.document ? root.document.allBandTypes() : []
                            delegate: SettingsRow {
                                required property var modelData
                                readonly property var byType: {
                                    try { return JSON.parse(UserPrefs.bandAllowanceByType) } catch (e) { return ({}) }
                                }
                                title: qsTrId("PreferencesPage.adaosul_pentru") + modelData.label
                                description: qsTrId("PreferencesPage.gol_sau_egal_cu_cel_general_cel_general")
                                NumberField {
                                    width: 180
                                    unit: "mm"
                                    from: 0
                                    to: 500
                                    value: parent.byType[modelData.key] ?? UserPrefs.bandAllowance
                                    onValueModified: (v) => {
                                        const map = parent.byType
                                        if (Math.abs(v - UserPrefs.bandAllowance) < 0.001) delete map[modelData.key]
                                        else map[modelData.key] = v
                                        UserPrefs.bandAllowanceByType = JSON.stringify(map)
                                    }
                                }
                            }
                        }
                    }
                }

                // Scurtături: toate comenzile, cu scurtătura editabilă (click → apasă combinația).
                Column {
                    visible: root.category === 7
                    width: parent.width
                    spacing: Theme.space4

                    Row {
                        width: parent.width
                        spacing: Theme.space2
                        Input {
                            id: shortcutSearch
                            width: parent.width - resetAll.width - Theme.space2
                            iconName: "search"
                            placeholderText: qsTrId("PreferencesPage.cauta_o_comanda_sau_o_scurtatura")
                        }
                        Button {
                            id: resetAll
                            text: qsTrId("PreferencesPage.reseteaza_toate")
                            iconName: "rotate-ccw"
                            onClicked: { root.shortcutsReset(); root.shortcutNotice = qsTrId("PreferencesPage.toate_scurtaturile_au_revenit_la_cele_implic") }
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: root.shortcutNotice.length > 0 ? root.shortcutNotice
                            : qsTrId("PreferencesPage.click_pe_scurtatura_si_apasa_noua_combinatie")
                        color: root.shortcutNotice.length > 0 ? Theme.accent : Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }

                    // Grupate pe categorii (ordinea primei apariții), fiecare într-un card.
                    Repeater {
                        model: {
                            const q = shortcutSearch.text.toLowerCase()
                            const list = root.commands.filter(c => !c.id.startsWith("file.recent:") && !c.id.startsWith("debug.")
                                && (q.length === 0 || c.title.toLowerCase().indexOf(q) >= 0 || c.category.toLowerCase().indexOf(q) >= 0
                                    || AppInfo.shortcutText(c.shortcut).toLowerCase().indexOf(q) >= 0))
                            const groups = []
                            for (const c of list) {
                                let g = groups.find(x => x.name === c.category)
                                if (!g) {
                                    g = { name: c.category, items: [] }
                                    groups.push(g)
                                }
                                g.items.push(c)
                            }
                            return groups
                        }
                        delegate: SettingsGroup {
                            id: shortcutGroup
                            required property var modelData
                            width: parent.width
                            title: modelData.name
                            Repeater {
                                model: shortcutGroup.modelData.items
                                delegate: SettingsRow {
                                    id: shortcutRow
                                    required property var modelData
                                    readonly property bool changed: modelData.shortcut !== modelData.defaultShortcut
                                    compact: true
                                    title: modelData.title
                                    Row {
                                        spacing: Theme.space1
                                        ShortcutRecorder {
                                            anchors.verticalCenter: parent.verticalCenter
                                            sequence: shortcutRow.modelData.shortcut
                                            onRecorded: (sequence) => root.shortcutChanged(shortcutRow.modelData.id, sequence)
                                        }
                                        IconButton {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 24; height: 24; iconSize: 13
                                            iconName: "rotate-ccw"
                                            opacity: shortcutRow.changed ? 1 : 0
                                            enabled: shortcutRow.changed
                                            tooltip: qsTrId("PreferencesPage.inapoi_la") + (shortcutRow.modelData.defaultShortcut ? AppInfo.shortcutText(shortcutRow.modelData.defaultShortcut) : qsTrId("PreferencesPage.fara_scurtatura"))
                                            onClicked: root.shortcutChanged(shortcutRow.modelData.id, null)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ColorDialog {
        id: colorDialog
        property string key: ""
        title: qsTrId("PreferencesPage.alege_culoarea")
        onAccepted: Theme.setOverride(key, selectedColor.toString())
    }
}
