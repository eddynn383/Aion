import QtQuick
import QtQuick.Dialogs
import Aion

// Preferințele (⌘,), pe toată fereastra: categoriile în stânga, setările în dreapta. Totul se aplică imediat și se
// păstrează între sesiuni (unitățile și autosalvarea prin Document, restul prin Settings / UserPrefs).
Rectangle {
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

    color: Theme.bgApp

    readonly property var categories: [
        { name: "General", icon: "settings" },
        { name: "Unități", icon: "ruler" },
        { name: "Salvare", icon: "check" },
        { name: "Aspect", icon: "palette" },
        { name: "Viewport", icon: "monitor" },
        { name: "Navigare", icon: "orbit" },
        { name: "Grid și snap", icon: "grid-3x3" },
        { name: "Scurtături", icon: "search" }
    ]

    Keys.onEscapePressed: root.closeRequested()
    Component.onCompleted: forceActiveFocus()

    // Blochează clicurile spre ce e dedesubt.
    MouseArea { anchors.fill: parent }

    // --- Bara de sus ---
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.titleBarHeight
        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.trafficLightsInset + Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            text: "Preferințe"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeLg
            font.weight: Font.DemiBold
        }
        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space3
            anchors.verticalCenter: parent.verticalCenter
            iconName: "x"
            tooltip: "Închide (Esc)"
            onClicked: root.closeRequested()
        }
        Separator { anchors.bottom: parent.bottom; width: parent.width }
    }

    // --- Categoriile ---
    Column {
        id: nav
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.margins: Theme.space4
        width: 200
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
                    }
                }
                MouseArea {
                    id: navArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.category = navItem.index
                }
            }
        }
    }
    Separator {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        x: nav.x + nav.width + Theme.space4
        width: 1
    }

    // --- Setările categoriei ---
    Flickable {
        id: content
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: nav.right
        anchors.right: parent.right
        anchors.leftMargin: Theme.space8
        contentHeight: page.height + Theme.space8 * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: page
            y: Theme.space8
            width: Math.min(620, content.width - Theme.space8)
            spacing: Theme.space6

            Text {
                text: root.categories[root.category].name
                color: Theme.textPrimary
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }

            // General
            Column {
                visible: root.category === 0
                width: parent.width
                spacing: Theme.space4
                Section {
                    width: parent.width
                    title: "Ghiduri"
                    Toggle {
                        width: parent.width
                        text: "Ghiduri animate la hover lung pe unelte"
                        checked: UserPrefs.hoverGuides
                        onToggled: UserPrefs.hoverGuides = checked
                    }
                }
                Section {
                    width: parent.width
                    title: "Documente recente"
                    Row {
                        spacing: Theme.space3
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.document ? root.document.recentFiles.length + " în listă" : ""
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSm
                        }
                        Button {
                            text: "Golește lista"
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
                spacing: Theme.space4
                Section {
                    width: parent.width
                    title: "Unitatea lungimilor"
                    Tabs {
                        width: parent.width
                        model: ["Auto", "mm", "cm", "m", "inci", "picioare"]
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
                        text: root.document
                              ? (root.document.displayUnit, root.document.displayDecimals,
                                 "Exemplu: " + root.document.formatLength(2537.25) + "  ·  " + root.document.formatLength(612))
                              : ""
                        color: Theme.textSecondary
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeSm
                    }
                }
            }

            // Salvare
            Column {
                visible: root.category === 2
                width: parent.width
                spacing: Theme.space4
                Section {
                    width: parent.width
                    title: "Autosalvare"
                    Toggle {
                        width: parent.width
                        text: "Salvează automat o copie de recuperare"
                        checked: root.document ? root.document.autosaveEnabled : false
                        onToggled: root.document.autosaveEnabled = checked
                    }
                    Slider {
                        visible: root.document ? root.document.autosaveEnabled : false
                        width: parent.width
                        label: "La fiecare"
                        from: 1
                        to: 30
                        stepSize: 1
                        value: root.document ? root.document.autosaveMinutes : 1
                        valueText: Math.round(value) + (Math.round(value) === 1 ? " minut" : " minute")
                        onMoved: root.document.autosaveMinutes = Math.round(value)
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Copia se face doar când documentul are modificări nesalvate și se oferă la repornire dacă "
                              + "aplicația s-a închis neașteptat. Fișierul tău nu e suprascris."
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
            }

            // Aspect
            Column {
                visible: root.category === 3
                width: parent.width
                spacing: Theme.space4
                Section {
                    width: parent.width
                    title: "Temă"
                    Tabs {
                        width: parent.width
                        model: ["Sistem", "Light", "Dark"]
                        currentIndex: Theme.mode
                        onCurrentIndexChanged: Theme.mode = currentIndex
                    }
                }
                Section {
                    width: parent.width
                    title: "Culoarea de accent"
                    Row {
                        spacing: Theme.space2
                        Repeater {
                            model: ["#3B82F6", "#8B5CF6", "#EC4899", "#EF4444", "#F59E0B", "#10B981", "#14B8A6", "#64748B"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 28
                                height: 28
                                radius: 14
                                color: modelData
                                border.color: Qt.colorEqual(Theme.accent, modelData) ? Theme.textPrimary : "transparent"
                                border.width: 2
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: Theme.setOverride("accent", parent.modelData)
                                }
                            }
                        }
                    }
                }
                Section {
                    width: parent.width
                    title: "Culori personalizate (" + (Theme.dark ? "dark" : "light") + ")"
                    Repeater {
                        model: [
                            { key: "accent", name: "Accent" },
                            { key: "bgApp", name: "Fundalul aplicației" },
                            { key: "bgPanel", name: "Panouri" },
                            { key: "bgElevated", name: "Ferestre plutitoare" },
                            { key: "viewportTop", name: "Viewport — sus" },
                            { key: "viewportBottom", name: "Viewport — jos" },
                            { key: "faceFront", name: "Fețele (față)" },
                            { key: "faceBack", name: "Fețele (verso)" },
                            { key: "edge", name: "Muchiile" }
                        ]
                        delegate: Item {
                            id: colorRow
                            required property var modelData
                            readonly property bool custom: Theme.overrides[modelData.key] !== undefined
                            width: parent.width
                            height: Theme.controlHeight
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: colorRow.modelData.name
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeSm
                            }
                            Row {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: Theme.space2
                                IconButton {
                                    visible: colorRow.custom
                                    width: 24; height: 24; iconSize: Theme.iconSizeSm
                                    iconName: "rotate-ccw"
                                    tooltip: "Revino la culoarea implicită"
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
                    Button {
                        text: "Resetează tema " + (Theme.dark ? "dark" : "light")
                        iconName: "rotate-ccw"
                        onClicked: Theme.resetOverrides()
                    }
                }
            }

            // Viewport
            Column {
                visible: root.category === 4
                width: parent.width
                spacing: Theme.space4
                // Preset-urile: aceleași setări de mai jos, alese dintr-un click.
                Section {
                    id: qualitySection
                    width: parent.width
                    title: "Calitate"
                    readonly property var presets: [
                        { name: "Performanță", msaa: 2, shadows: 1024, ao: false, aa: false },
                        { name: "Echilibrat", msaa: 4, shadows: 2048, ao: true, aa: true },
                        { name: "Calitate", msaa: 8, shadows: 4096, ao: true, aa: true } ]
                    readonly property int current: root.settings ? presets.findIndex(p => p.msaa === root.settings.msaa
                        && p.shadows === root.settings.shadowQuality && p.ao === root.settings.ambientOcclusion
                        && p.aa === root.settings.progressiveAA) : -1
                    Tabs {
                        width: parent.width
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
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qualitySection.current < 0 ? "Personalizat (setările de mai jos)." : "Performanță: pentru modele mari sau calculatoare "
                              + "mai slabe. Calitate: cele mai fine margini și umbre."
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
                Section {
                    width: parent.width
                    title: "Anti-aliasing (MSAA)"
                    Tabs {
                        width: parent.width
                        model: ["Fără", "2×", "4×", "8×"]
                        currentIndex: root.settings ? [1, 2, 4, 8].indexOf(root.settings.msaa) : 2
                        onCurrentIndexChanged: if (root.settings && currentIndex >= 0) root.settings.msaa = [1, 2, 4, 8][currentIndex]
                    }
                    Toggle {
                        width: parent.width
                        text: "Margini mai fine cât camera stă pe loc (8×)"
                        checked: root.settings ? root.settings.progressiveAA : true
                        onToggled: root.settings.progressiveAA = checked
                    }
                }
                Section {
                    width: parent.width
                    title: "Calitatea umbrelor"
                    Tabs {
                        width: parent.width
                        model: ["Scăzută", "Medie", "Înaltă"]
                        currentIndex: root.settings ? Math.max(0, [1024, 2048, 4096].indexOf(root.settings.shadowQuality)) : 1
                        onCurrentIndexChanged: if (root.settings) root.settings.shadowQuality = [1024, 2048, 4096][currentIndex]
                    }
                    Toggle {
                        width: parent.width
                        text: "Umbrire ambientală (SSAO): colțurile și contactele mai întunecate"
                        checked: root.settings ? root.settings.ambientOcclusion : true
                        onToggled: root.settings.ambientOcclusion = checked
                    }
                }
                Section {
                    width: parent.width
                    title: "Mediu de iluminare"
                    Tabs {
                        width: parent.width
                        readonly property var keys: ["simplu", "studio", "exterior", "innorat"]
                        model: ["Simplu", "Studio", "Exterior", "Înnorat"]
                        currentIndex: root.settings ? Math.max(0, keys.indexOf(root.settings.environment)) : 0
                        onCurrentIndexChanged: if (root.settings) root.settings.environment = keys[currentIndex]
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Culoarea luminii ambientale și reflexiile pe suprafețele netede (sticlă, metal, lac)."
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
                Section {
                    width: parent.width
                    title: "Informații"
                    Toggle {
                        width: parent.width
                        text: "Arată fps în bara de jos"
                        checked: UserPrefs.showFps
                        onToggled: UserPrefs.showFps = checked
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

            // Navigare
            Column {
                visible: root.category === 5
                width: parent.width
                spacing: Theme.space4
                Section {
                    width: parent.width
                    title: "Centrul orbitei"
                    Tabs {
                        width: parent.width
                        model: ["Centrul vederii", "Selecția", "Punctul de sub cursor"]
                        currentIndex: root.viewport ? root.viewport.orbitPivot : 0
                        onCurrentIndexChanged: if (root.viewport) root.viewport.orbitPivot = currentIndex
                    }
                }
                Section {
                    width: parent.width
                    title: "Zoom"
                    Toggle {
                        width: parent.width
                        text: "Zoom spre suprafața de sub cursor"
                        checked: root.viewport ? root.viewport.zoomToSurface : true
                        onToggled: if (root.viewport) root.viewport.zoomToSurface = checked
                    }
                }
                Section {
                    width: parent.width
                    title: "Planuri de tăiere"
                    Row {
                        width: parent.width
                        spacing: Theme.space2
                        NumberField {
                            width: (parent.width - Theme.space2) / 2
                            label: "Clip start"
                            unit: "mm"
                            from: 0
                            to: 100000
                            step: 0.1
                            decimals: 3
                            value: root.viewport ? root.viewport.clipStart : 0
                            onValueModified: (v) => { if (root.viewport) root.viewport.clipStart = v }
                        }
                        NumberField {
                            width: (parent.width - Theme.space2) / 2
                            label: "Clip end"
                            unit: "m"
                            from: 0
                            to: 1000000
                            step: 10
                            value: root.viewport ? root.viewport.clipEnd / 1000 : 0
                            onValueModified: (v) => { if (root.viewport) root.viewport.clipEnd = v * 1000 }
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "0 = automat (după distanța camerei și mărimea modelului)."
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
            }

            // Grid și snap
            Column {
                visible: root.category === 6
                width: parent.width
                spacing: Theme.space4
                Section {
                    width: parent.width
                    title: "Grid"
                    Toggle {
                        width: parent.width
                        text: "Arată grid-ul"
                        checked: root.viewport ? root.viewport.gridVisible : false
                        onToggled: root.viewport.gridVisible = checked
                    }
                }
                Section {
                    width: parent.width
                    title: "Snap la grid (⌥G)"
                    Toggle {
                        width: parent.width
                        text: "Punctele libere cad pe grid"
                        checked: root.document ? root.document.snapToGrid : false
                        onToggled: if (root.document) root.document.snapToGrid = checked
                    }
                    Tabs {
                        width: parent.width
                        readonly property var steps: [0, 1, 5, 10, 50, 100, 500, 1000]
                        model: ["Auto", "1 mm", "5 mm", "1 cm", "5 cm", "10 cm", "50 cm", "1 m"]
                        currentIndex: root.document ? Math.max(0, steps.indexOf(root.document.gridStep)) : 0
                        onCurrentIndexChanged: if (root.document) root.document.gridStep = steps[currentIndex]
                    }
                }
            }

            // Scurtături: toate comenzile, cu scurtătura editabilă (click → apasă combinația).
            Column {
                visible: root.category === 7
                width: parent.width
                spacing: Theme.space3

                Row {
                    width: parent.width
                    spacing: Theme.space2
                    Input {
                        id: shortcutSearch
                        width: parent.width - resetAll.width - Theme.space2
                        iconName: "search"
                        placeholderText: "Caută o comandă sau o scurtătură"
                    }
                    Button {
                        id: resetAll
                        text: "Resetează toate"
                        onClicked: { root.shortcutsReset(); root.shortcutNotice = "Toate scurtăturile au revenit la cele implicite." }
                    }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Click pe scurtătură și apasă noua combinație. Esc anulează, Backspace o șterge. O combinație folosită deja trece la comanda nouă."
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                }
                Text {
                    width: parent.width
                    visible: root.shortcutNotice.length > 0
                    wrapMode: Text.Wrap
                    text: root.shortcutNotice
                    color: Theme.accent
                    font.pixelSize: Theme.fontSizeSm
                }

                Column {
                    width: parent.width
                    spacing: 2
                    Repeater {
                        model: {
                            const q = shortcutSearch.text.toLowerCase()
                            return root.commands.filter(c => !c.id.startsWith("file.recent:") && !c.id.startsWith("debug.")
                                && (q.length === 0 || c.title.toLowerCase().indexOf(q) >= 0 || c.category.toLowerCase().indexOf(q) >= 0
                                    || AppInfo.shortcutText(c.shortcut).toLowerCase().indexOf(q) >= 0))
                        }
                        delegate: Rectangle {
                            id: shortcutRow
                            required property var modelData
                            readonly property bool changed: modelData.shortcut !== modelData.defaultShortcut
                            width: parent.width
                            height: Theme.controlHeight
                            radius: Theme.radiusSm
                            color: rowHover.hovered ? Theme.bgHover : "transparent"
                            HoverHandler { id: rowHover }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.space2
                                anchors.right: categoryText.left
                                anchors.rightMargin: Theme.space2
                                anchors.verticalCenter: parent.verticalCenter
                                text: shortcutRow.modelData.title
                                elide: Text.ElideRight
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeSm
                            }
                            Text {
                                id: categoryText
                                anchors.right: recorder.left
                                anchors.rightMargin: Theme.space3
                                anchors.verticalCenter: parent.verticalCenter
                                text: shortcutRow.modelData.category
                                color: Theme.textTertiary
                                font.pixelSize: Theme.fontSizeXs
                            }
                            ShortcutRecorder {
                                id: recorder
                                anchors.right: resetOne.left
                                anchors.rightMargin: Theme.space1
                                anchors.verticalCenter: parent.verticalCenter
                                sequence: shortcutRow.modelData.shortcut
                                onRecorded: (sequence) => root.shortcutChanged(shortcutRow.modelData.id, sequence)
                            }
                            IconButton {
                                id: resetOne
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.space1
                                anchors.verticalCenter: parent.verticalCenter
                                width: 24; height: 24; iconSize: 13
                                iconName: "rotate-ccw"
                                opacity: shortcutRow.changed ? 1 : 0
                                enabled: shortcutRow.changed
                                tooltip: "Înapoi la " + (shortcutRow.modelData.defaultShortcut ? AppInfo.shortcutText(shortcutRow.modelData.defaultShortcut) : "fără scurtătură")
                                onClicked: root.shortcutChanged(shortcutRow.modelData.id, null)
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
        title: "Alege culoarea"
        onAccepted: Theme.setOverride(key, selectedColor.toString())
    }
}
