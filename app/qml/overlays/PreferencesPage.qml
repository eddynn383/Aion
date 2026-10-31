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
        { name: "General", icon: "settings", description: "Ajutorul din aplicație, ecranul de start și documentele recente." },
        { name: "Unități", icon: "ruler", description: "Cum se afișează și cum se citesc lungimile." },
        { name: "Salvare", icon: "check", description: "Copiile de recuperare ale documentului deschis." },
        { name: "Aspect", icon: "palette", description: "Tema, culoarea de accent și culorile interfeței." },
        { name: "Viewport", icon: "monitor", description: "Calitatea randării, lumina și informațiile din viewport." },
        { name: "Navigare", icon: "orbit", description: "Orbita, zoom-ul și planurile de tăiere ale camerei." },
        { name: "Grid și snap", icon: "grid-3x3", description: "Grid-ul de pe sol și prinderea punctelor de el." },
        { name: "Scurtături", icon: "search", description: "Tastele comenzilor și ale uneltelor; click pe una ca s-o schimbi." },
        { name: "Mobilier", icon: "box", description: "Cantul: cum se vede în viewport și cât se adaugă la fiecare muchie în lista de debitare." }
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
                text: "Preferințe"
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
                tooltip: "Închide (Esc)"
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
                        title: "Limbă"
                        SettingsRow {
                            stacked: true
                            title: "Limba interfeței"
                            description: "Se aplică după repornire. Traducerea e în lucru: deocamdată sunt traduse lista de debitare și Inspector-ul; restul rămâne în română. Limbile „beta” cad pe engleză unde nu au traducere."
                            Flow {
                                width: parent.width
                                spacing: Theme.space2
                                Repeater {
                                    model: [{ key: "ro", name: "Română" }, { key: "en", name: "English" },
                                        { key: "de", name: "Deutsch (beta)" }, { key: "pl", name: "Polski (beta)" },
                                        { key: "it", name: "Italiano (beta)" }, { key: "fr", name: "Français (beta)" },
                                        { key: "es", name: "Español (beta)" }, { key: "pt", name: "Português (beta)" }]
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
                        title: "Ajutor"
                        SettingsRow {
                            title: "Ghiduri animate"
                            description: "Un ghid scurt apare când ții cursorul pe o unealtă din toolbar."
                            Toggle {
                                checked: UserPrefs.hoverGuides
                                onToggled: UserPrefs.hoverGuides = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: "Pornire"
                        SettingsRow {
                            title: "Ecranul de start"
                            description: "La pornire: document nou, deschide, ultimul fișier și documentele recente. După o închidere bruscă apare oricum, cu copia de recuperare."
                            Toggle {
                                checked: UserPrefs.startScreen
                                onToggled: UserPrefs.startScreen = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: "Documente recente"
                        SettingsRow {
                            title: "Lista documentelor recente"
                            description: root.document ? (root.document.recentFiles.length === 1 ? "Un document în listă."
                                : root.document.recentFiles.length + " documente în listă.") : ""
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
                    spacing: Theme.space6
                    SettingsGroup {
                        width: parent.width
                        title: "Lungimi"
                        SettingsRow {
                            stacked: true
                            title: "Unitatea"
                            description: "Auto: milimetri sub 1 m, metri peste. Valorile tastate fără unitate se citesc în unitatea aleasă."
                            Tabs {
                                width: parent.width
                                model: ["Auto", "mm", "cm", "m", "inci", "picioare"]
                                currentIndex: root.document ? root.document.displayUnit : 0
                                onCurrentIndexChanged: if (root.document) root.document.displayUnit = currentIndex
                            }
                        }
                        SettingsRow {
                            title: "Precizie"
                            description: "Câte zecimale se afișează."
                            Tabs {
                                width: 300
                                model: ["Implicit", "0", "0.0", "0.00", "0.000"]
                                currentIndex: root.document ? root.document.displayDecimals + 1 : 0
                                onCurrentIndexChanged: if (root.document) root.document.displayDecimals = currentIndex - 1
                            }
                        }
                        SettingsRow {
                            title: "Exemplu"
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
                        title: "Autosalvare"
                        SettingsRow {
                            title: "Salvează automat o copie de recuperare"
                            description: "Doar când documentul are modificări nesalvate. Se oferă la repornire dacă aplicația s-a închis neașteptat; fișierul tău nu e suprascris."
                            Toggle {
                                checked: root.document ? root.document.autosaveEnabled : false
                                onToggled: root.document.autosaveEnabled = checked
                            }
                        }
                        SettingsRow {
                            visible: root.document ? root.document.autosaveEnabled : false
                            height: visible ? implicitHeight : 0
                            title: "Intervalul"
                            Slider {
                                width: 260
                                from: 1
                                to: 30
                                stepSize: 1
                                value: root.document ? root.document.autosaveMinutes : 1
                                valueText: Math.round(value) + (Math.round(value) === 1 ? " minut" : " minute")
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
                        title: "Interfață"
                        SettingsRow {
                            title: "Tema"
                            description: "Sistem urmează setarea calculatorului."
                            Tabs {
                                width: 260
                                model: ["Sistem", "Light", "Dark"]
                                currentIndex: Theme.mode
                                onCurrentIndexChanged: Theme.mode = currentIndex
                            }
                        }
                        SettingsRow {
                            title: "Culoarea de accent"
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
                        title: "Culori personalizate (tema " + (Theme.dark ? "dark" : "light") + ")"
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
                    }
                    Button {
                        anchors.right: parent.right
                        text: "Resetează tema " + (Theme.dark ? "dark" : "light")
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
                        title: "Calitate"
                        readonly property var presets: [
                            { name: "Performanță", msaa: 2, shadows: 1024, ao: false, aa: false },
                            { name: "Echilibrat", msaa: 4, shadows: 2048, ao: true, aa: true },
                            { name: "Calitate", msaa: 8, shadows: 4096, ao: true, aa: true } ]
                        readonly property int current: root.settings ? presets.findIndex(p => p.msaa === root.settings.msaa
                            && p.shadows === root.settings.shadowQuality && p.ao === root.settings.ambientOcclusion
                            && p.aa === root.settings.progressiveAA) : -1
                        SettingsRow {
                            title: "Preset"
                            description: qualitySection.current < 0 ? "Personalizat (setările de mai jos)."
                                : "Performanță: modele mari sau calculatoare mai slabe. Calitate: cele mai fine margini și umbre."
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
                        title: "Randare"
                        SettingsRow {
                            title: "Anti-aliasing (MSAA)"
                            Tabs {
                                width: 260
                                model: ["Fără", "2×", "4×", "8×"]
                                currentIndex: root.settings ? [1, 2, 4, 8].indexOf(root.settings.msaa) : 2
                                onCurrentIndexChanged: if (root.settings && currentIndex >= 0) root.settings.msaa = [1, 2, 4, 8][currentIndex]
                            }
                        }
                        SettingsRow {
                            title: "Margini mai fine cu camera oprită"
                            description: "Trece la 8× cât camera stă pe loc."
                            Toggle {
                                checked: root.settings ? root.settings.progressiveAA : true
                                onToggled: root.settings.progressiveAA = checked
                            }
                        }
                        SettingsRow {
                            title: "Calitatea umbrelor"
                            Tabs {
                                width: 260
                                model: ["Scăzută", "Medie", "Înaltă"]
                                currentIndex: root.settings ? Math.max(0, [1024, 2048, 4096].indexOf(root.settings.shadowQuality)) : 1
                                onCurrentIndexChanged: if (root.settings) root.settings.shadowQuality = [1024, 2048, 4096][currentIndex]
                            }
                        }
                        SettingsRow {
                            title: "Umbrire ambientală (SSAO)"
                            description: "Colțurile și contactele dintre obiecte mai întunecate."
                            Toggle {
                                checked: root.settings ? root.settings.ambientOcclusion : true
                                onToggled: root.settings.ambientOcclusion = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: "Lumină"
                        SettingsRow {
                            stacked: true
                            title: "Mediul de iluminare"
                            description: "Culoarea luminii ambientale și reflexiile pe suprafețele netede (sticlă, metal, lac)."
                            Tabs {
                                width: parent.width
                                readonly property var keys: ["simplu", "studio", "exterior", "innorat"]
                                model: ["Simplu", "Studio", "Exterior", "Înnorat"]
                                currentIndex: root.settings ? Math.max(0, keys.indexOf(root.settings.environment)) : 0
                                onCurrentIndexChanged: if (root.settings) root.settings.environment = keys[currentIndex]
                            }
                        }
                        SettingsRow {
                            title: "Latitudinea"
                            description: "Pentru poziția soarelui și umbre."
                            Slider {
                                width: 260
                                from: -66
                                to: 66
                                stepSize: 0.5
                                value: root.viewport ? root.viewport.latitude : 45
                                valueText: Math.abs(value).toFixed(1) + "° " + (value >= 0 ? "N" : "S")
                                onMoved: if (root.viewport) root.viewport.latitude = value
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: "Informații"
                        SettingsRow {
                            title: "Arată fps în bara de jos"
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
                        title: "Cameră"
                        SettingsRow {
                            stacked: true
                            title: "Centrul orbitei"
                            description: "În jurul cărui punct se rotește camera la orbit."
                            Tabs {
                                width: parent.width
                                model: ["Centrul vederii", "Selecția", "Punctul de sub cursor"]
                                currentIndex: root.viewport ? root.viewport.orbitPivot : 0
                                onCurrentIndexChanged: if (root.viewport) root.viewport.orbitPivot = currentIndex
                            }
                        }
                        SettingsRow {
                            title: "Zoom spre suprafața de sub cursor"
                            description: "Zoom-ul încetinește lângă detalii și nu trece prin ele."
                            Toggle {
                                checked: root.viewport ? root.viewport.zoomToSurface : true
                                onToggled: if (root.viewport) root.viewport.zoomToSurface = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: "Planuri de tăiere"
                        SettingsRow {
                            title: "Clip start"
                            description: "0 = automat (după distanța camerei și mărimea modelului)."
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
                            title: "Clip end"
                            description: "0 = automat."
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
                        title: "Grid"
                        SettingsRow {
                            title: "Arată grid-ul"
                            Toggle {
                                checked: root.viewport ? root.viewport.gridVisible : false
                                onToggled: root.viewport.gridVisible = checked
                            }
                        }
                    }
                    SettingsGroup {
                        width: parent.width
                        title: "Snap la grid (" + AppInfo.shortcutText("Alt+G") + ")"
                        SettingsRow {
                            title: "Punctele libere cad pe grid"
                            description: "Doar punctele fără altă inferență (capăt, mijloc, muchie) și distanțele pe direcții."
                            Toggle {
                                checked: root.document ? root.document.snapToGrid : false
                                onToggled: if (root.document) root.document.snapToGrid = checked
                            }
                        }
                        SettingsRow {
                            stacked: true
                            title: "Pasul"
                            description: "Auto urmează grid-ul afișat (se schimbă cu zoom-ul)."
                            Tabs {
                                width: parent.width
                                readonly property var steps: [0, 1, 5, 10, 50, 100, 500, 1000]
                                model: ["Auto", "1 mm", "5 mm", "1 cm", "5 cm", "10 cm", "50 cm", "1 m"]
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
                        title: "Cant"
                        SettingsRow {
                            title: "Liniile de cant în viewport"
                            description: "Laturile plăcilor cu cant au o linie în culoarea decorului cantului."
                            Toggle {
                                checked: UserPrefs.showBands
                                onToggled: UserPrefs.showBands = checked
                            }
                        }
                        SettingsRow {
                            title: "Adaosul pe muchie"
                            description: "Cât cant în plus se numără la fiecare muchie (tăiere, prindere în mașină)."
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
                                title: "Adaosul pentru " + modelData.label
                                description: "Gol sau egal cu cel general = cel general."
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
                            placeholderText: "Caută o comandă sau o scurtătură"
                        }
                        Button {
                            id: resetAll
                            text: "Resetează toate"
                            iconName: "rotate-ccw"
                            onClicked: { root.shortcutsReset(); root.shortcutNotice = "Toate scurtăturile au revenit la cele implicite." }
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: root.shortcutNotice.length > 0 ? root.shortcutNotice
                            : "Click pe scurtătură și apasă noua combinație. Esc anulează, Backspace o șterge. O combinație folosită deja trece la comanda nouă."
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
                                            tooltip: "Înapoi la " + (shortcutRow.modelData.defaultShortcut ? AppInfo.shortcutText(shortcutRow.modelData.defaultShortcut) : "fără scurtătură")
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
        title: "Alege culoarea"
        onAccepted: Theme.setOverride(key, selectedColor.toString())
    }
}
