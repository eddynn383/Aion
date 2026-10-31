import QtQuick
import Aion

// Ecranul de start (ADR 0040), ca splash-ul din Blender: Document nou, Deschide, Redeschide ultimul fișier și
// documentele recente (cu miniaturile salvate în .aion); după o închidere bruscă, copiile de recuperare, deasupra.
// Esc sau un click în afară îl închid: rămâne documentul gol. Cererile ajung la fereastră (care știe de dialoguri și
// de modificările nesalvate).
Popover {
    id: root

    property var document
    // Locul disponibil (fereastra); lista recentelor se scurtează în ferestre joase.
    property real windowHeight: parent ? parent.height : 900

    signal newRequested()
    signal openRequested()
    signal recentRequested(string path)
    signal recoverRequested(string session)
    signal tourRequested()
    signal preferencesRequested()

    // Documentele recente, citite la deschidere (cu miniaturile), și rândul ales cu săgețile (-1 = niciunul).
    property var recent: []
    property int current: -1
    readonly property var recoveries: document ? document.recoveries : []
    readonly property int rowHeight: 52
    readonly property int listRows: Math.max(2, Math.min(5, Math.floor((windowHeight - 420 - recoveryCard.height) / rowHeight)))

    readonly property var tips: [
        qsTrId("StartScreen.k_cauta_orice_comanda_unealta_sau_document_recen"),
        qsTrId("StartScreen.tine_cursorul_pe_o_unealta_din_toolbar_un_ghid_a"),
        qsTrId("StartScreen.1_2_si_3_trec_intre_modelare_animare_si_interact"),
        qsTrId("StartScreen.tasteaza_o_lungime_in_timp_ce_desenezi_de_exempl"),
        qsTrId("StartScreen.fisierele_aion_pastreaza_si_o_miniatura_o_vezi_a")
    ]
    property string tip: ""

    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(800, (parent ? parent.width : 800) - Theme.space8 * 2)
    padding: 0

    onAboutToShow: {
        recent = document ? document.recentDocuments() : []
        current = -1
        tip = AppInfo.platformText(tips[Math.floor(Math.random() * tips.length)])
    }
    // Un document deschis din altă parte (Finder, meniul, o copie recuperată) închide ecranul.
    Connections {
        target: root.document
        function onDocumentReplaced() { root.close() }
    }

    function openRecent(index) {
        if (index >= 0 && index < recent.length) {
            root.close()
            root.recentRequested(recent[index].path)
        }
    }

    contentItem: FocusScope {
        implicitWidth: root.width
        implicitHeight: layout.implicitHeight
        focus: true

        Keys.onUpPressed: root.current = Math.max(root.current < 0 ? root.recent.length : root.current, 1) - 1
        Keys.onDownPressed: root.current = Math.min(root.current + 1, root.recent.length - 1)
        Keys.onReturnPressed: root.openRecent(Math.max(root.current, 0))
        Keys.onEnterPressed: root.openRecent(Math.max(root.current, 0))

        Column {
            id: layout
            width: parent.width

            // --- Banda de sus: grid-ul în perspectivă, axele și numele ---
            Rectangle {
                id: hero
                x: 1
                y: 1
                width: parent.width - 2
                height: 168
                radius: Theme.radiusLg - 1
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.viewportTop }
                    GradientStop { position: 1; color: Theme.viewportBottom }
                }
                // Colțurile de jos rămân drepte (corpul începe dedesubt).
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: parent.radius
                    color: Theme.viewportBottom
                }

                Canvas {
                    id: grid
                    anchors.fill: parent
                    // Culorile temei: la o schimbare de temă, grid-ul se redesenează.
                    readonly property var colors: [Theme.gridMinor, Theme.gridMajor, Theme.axisX, Theme.axisY, Theme.axisZ,
                        Theme.viewportTop]
                    onColorsChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        const w = width, h = height
                        const horizon = h * 0.36
                        const vx = w * 0.7
                        const depth = (h - horizon) * 1.25
                        const step = w * 0.14
                        // Liniile spre punctul de fugă și cele paralele cu orizontul (adâncimea k: y = orizont + d / k).
                        ctx.lineWidth = 1
                        for (let i = -20; i <= 20; ++i) {
                            const g = ctx.createLinearGradient(0, horizon, 0, h)
                            g.addColorStop(0, "transparent")
                            g.addColorStop(0.5, i % 4 === 0 ? Theme.gridMajor : Theme.gridMinor)
                            ctx.strokeStyle = g
                            ctx.beginPath()
                            ctx.moveTo(vx, horizon)
                            ctx.lineTo(vx + i * step / 0.8, horizon + depth / 0.8)
                            ctx.stroke()
                        }
                        for (let k = 1; k <= 24; ++k) {
                            const y = horizon + depth / k
                            if (y > h)
                                continue
                            ctx.globalAlpha = Math.min(1, 2.5 / k)
                            ctx.strokeStyle = k % 4 === 0 ? Theme.gridMajor : Theme.gridMinor
                            ctx.beginPath()
                            ctx.moveTo(0, y)
                            ctx.lineTo(w, y)
                            ctx.stroke()
                        }
                        ctx.globalAlpha = 1
                        // Axele, ca în iconiță, din originea de pe linia -2 la adâncimea 2: roșu (X, de-a lungul
                        // orizontului), verde (Y, spre punctul de fugă), albastru (Z, în sus).
                        const ox = vx - 2 * step / 2
                        const oy = horizon + depth / 2
                        ctx.lineWidth = 2
                        ctx.lineCap = "round"
                        ctx.strokeStyle = Theme.axisX
                        ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(w, oy); ctx.stroke()
                        const green = ctx.createLinearGradient(ox, oy, vx, horizon)
                        green.addColorStop(0, Theme.axisY)
                        green.addColorStop(1, "transparent")
                        ctx.strokeStyle = green
                        ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(vx, horizon); ctx.stroke()
                        ctx.strokeStyle = Theme.axisZ
                        ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(ox, oy - h * 0.4); ctx.stroke()
                        ctx.fillStyle = Theme.textPrimary
                        ctx.beginPath(); ctx.arc(ox, oy, 3.5, 0, Math.PI * 2); ctx.fill()
                    }
                }

                Row {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Theme.space8
                    anchors.bottomMargin: Theme.space6
                    spacing: Theme.space4
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/aion/branding/aion-icon.svg"
                        sourceSize: Qt.size(52, 52)
                        width: 52
                        height: 52
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text {
                            text: qsTrId("StartScreen.aion")
                            color: Theme.textPrimary
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: qsTrId("StartScreen.modeleaza_ce_ramane")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeMd
                        }
                    }
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space4
                    text: "v" + AppInfo.version
                    color: Theme.textTertiary
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeXs
                }
            }

            // --- Copiile de recuperare (doar după o închidere bruscă) ---
            Item {
                width: parent.width
                height: recoveryCard.height > 0 ? recoveryCard.height + Theme.space6 : 0
                Rectangle {
                    id: recoveryCard
                    visible: root.recoveries.length > 0
                    x: Theme.space6
                    y: Theme.space6
                    width: parent.width - Theme.space6 * 2
                    height: visible ? recoveryColumn.implicitHeight + Theme.space4 * 2 : 0
                    radius: Theme.radiusMd
                    color: Theme.accentSubtle
                    border.color: Theme.focusRing

                    Column {
                        id: recoveryColumn
                        x: Theme.space4
                        y: Theme.space4
                        width: parent.width - Theme.space4 * 2
                        spacing: Theme.space3

                        Row {
                            spacing: Theme.space3
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "life-buoy"
                                size: Theme.iconSizeLg
                                color: Theme.accent
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text {
                                    text: qsTrId("StartScreen.aion_s_a_inchis_neasteptat")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeMd
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: root.recoveries.length === 1
                                          ? qsTrId("StartScreen.am_pastrat_ultima_copie_salvata_automat_a_docume")
                                          : qsTrId("StartScreen.am_pastrat_copiile_salvate_automat_ale_documente")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeSm
                                }
                            }
                        }
                        Repeater {
                            model: root.recoveries.slice(0, 3)
                            delegate: Item {
                                id: recoveryRow
                                required property var modelData
                                width: recoveryColumn.width
                                height: Theme.controlHeight
                                Column {
                                    anchors.left: parent.left
                                    anchors.right: recoveryButtons.left
                                    anchors.rightMargin: Theme.space3
                                    anchors.verticalCenter: parent.verticalCenter
                                    leftPadding: Theme.iconSizeLg + Theme.space3
                                    Text {
                                        width: parent.width - parent.leftPadding
                                        text: recoveryRow.modelData.name + "  ·  " + recoveryRow.modelData.when
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeMd
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width - parent.leftPadding
                                        text: recoveryRow.modelData.folder
                                        color: Theme.textTertiary
                                        font.pixelSize: Theme.fontSizeXs
                                        elide: Text.ElideMiddle
                                    }
                                }
                                Row {
                                    id: recoveryButtons
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Theme.space2
                                    Button {
                                        small: true
                                        variant: "ghost"
                                        text: qsTrId("StartScreen.renunta")
                                        onClicked: root.document.discardRecovery(recoveryRow.modelData.session)
                                    }
                                    Button {
                                        small: true
                                        variant: "primary"
                                        iconName: "history"
                                        text: qsTrId("StartScreen.recupereaza")
                                        onClicked: root.recoverRequested(recoveryRow.modelData.session)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // --- Începe (stânga) și Recente (dreapta) ---
            Item {
                width: parent.width
                height: Math.max(startActions.implicitHeight + Theme.space4 + tipCard.height, recentColumn.implicitHeight)
                        + Theme.space6 * 2

                Item {
                    id: startColumn
                    x: Theme.space6
                    y: Theme.space6
                    width: 272
                    height: parent.height - Theme.space6 * 2

                    Column {
                        id: startActions
                        width: parent.width
                        spacing: 2

                        SectionLabel { text: qsTrId("StartScreen.incepe") }
                        ActionRow {
                            iconName: "file-plus"
                            text: qsTrId("StartScreen.document_nou")
                            shortcut: "Ctrl+N"
                            onClicked: { root.close(); root.newRequested() }
                        }
                        ActionRow {
                            iconName: "folder-open"
                            text: qsTrId("StartScreen.deschide")
                            shortcut: "Ctrl+O"
                            onClicked: { root.close(); root.openRequested() }
                        }
                        ActionRow {
                            visible: root.recent.length > 0
                            iconName: "history"
                            text: root.recent.length > 0 ? qsTrId("StartScreen.redeschide") + root.recent[0].name + "”" : ""
                            shortcut: "Return"
                            onClicked: root.openRecent(0)
                        }
                    }

                    // Un sfat, jos în coloană.
                    Rectangle {
                        id: tipCard
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: tipText.implicitHeight + Theme.space3 * 2
                        radius: Theme.radiusMd
                        color: Theme.bgInput
                        border.color: Theme.borderSubtle
                        Icon {
                            id: tipIcon
                            x: Theme.space3
                            y: Theme.space3 + 1
                            name: "sparkles"
                            size: Theme.iconSizeSm
                            color: Theme.textTertiary
                        }
                        Text {
                            id: tipText
                            anchors.left: tipIcon.right
                            anchors.leftMargin: Theme.space2
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space3
                            y: Theme.space3
                            text: root.tip
                            wrapMode: Text.Wrap
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSm
                            lineHeight: 1.25
                        }
                    }
                }

                Rectangle {
                    x: startColumn.x + startColumn.width + Theme.space6
                    y: Theme.space6
                    width: 1
                    height: parent.height - Theme.space6 * 2
                    color: Theme.borderSubtle
                }

                Column {
                    id: recentColumn
                    x: startColumn.x + startColumn.width + Theme.space6 * 2 + 1
                    y: Theme.space6
                    width: parent.width - x - Theme.space6
                    spacing: 2

                    SectionLabel { text: qsTrId("StartScreen.recente") }
                    ListView {
                        id: recentList
                        width: parent.width
                        height: root.listRows * root.rowHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: root.recent
                        currentIndex: root.current
                        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
                        delegate: Rectangle {
                            id: recentRow
                            required property var modelData
                            required property int index
                            width: recentList.width
                            height: root.rowHeight
                            radius: Theme.radiusMd
                            color: root.current === index ? Theme.accentSubtle : rowArea.containsMouse ? Theme.bgHover : "transparent"

                            Rectangle {
                                id: thumb
                                x: Theme.space2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 56
                                height: 40
                                radius: Theme.radiusSm
                                color: Theme.bgInput
                                border.color: Theme.borderSubtle
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    visible: recentRow.modelData.thumbnail.length > 0
                                    source: recentRow.modelData.thumbnail
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    smooth: true
                                    mipmap: true
                                }
                                Icon {
                                    anchors.centerIn: parent
                                    visible: recentRow.modelData.thumbnail.length === 0
                                    name: "box"
                                    size: Theme.iconSizeMd
                                    color: Theme.textTertiary
                                }
                            }
                            Column {
                                anchors.left: thumb.right
                                anchors.leftMargin: Theme.space3
                                anchors.right: whenText.left
                                anchors.rightMargin: Theme.space3
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text {
                                    width: parent.width
                                    text: recentRow.modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeMd
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: recentRow.modelData.folder
                                    color: Theme.textTertiary
                                    font.pixelSize: Theme.fontSizeXs
                                    elide: Text.ElideMiddle
                                }
                            }
                            Text {
                                id: whenText
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.space3
                                anchors.verticalCenter: parent.verticalCenter
                                text: recentRow.modelData.when
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeXs
                            }
                            MouseArea {
                                id: rowArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.openRecent(recentRow.index)
                            }
                        }

                        // Fără documente recente.
                        Column {
                            anchors.centerIn: parent
                            visible: root.recent.length === 0
                            spacing: Theme.space2
                            Icon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                name: "folder"
                                size: Theme.iconSizeLg
                                color: Theme.textTertiary
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTrId("StartScreen.niciun_document_recent")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeMd
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTrId("StartScreen.documentele_pe_care_le_salvezi_sau_le_deschizi_a")
                                color: Theme.textTertiary
                                font.pixelSize: Theme.fontSizeSm
                            }
                        }
                    }
                }
            }

            // --- Jos: arată la pornire, ajutor ---
            Rectangle {
                width: parent.width
                height: 1
                color: Theme.borderSubtle
            }
            Item {
                width: parent.width
                height: 52
                Toggle {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space6
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("StartScreen.arata_la_pornire")
                    checked: UserPrefs.startScreen
                    onToggled: UserPrefs.startScreen = checked
                }
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space4
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space1
                    Button {
                        small: true
                        variant: "ghost"
                        iconName: "info"
                        text: qsTrId("StartScreen.turul_uneltelor")
                        onClicked: { root.close(); root.tourRequested() }
                    }
                    Button {
                        small: true
                        variant: "ghost"
                        iconName: "settings"
                        text: qsTrId("StartScreen.preferinte")
                        onClicked: { root.close(); root.preferencesRequested() }
                    }
                }
            }
        }
    }

    component SectionLabel: Text {
        height: 28
        verticalAlignment: Text.AlignVCenter
        leftPadding: Theme.space2
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
        font.weight: Font.Medium
    }

    // Un rând de acțiune: iconiță, text și scurtătura în dreapta.
    component ActionRow: Rectangle {
        id: action
        property string iconName
        property string text
        property string shortcut
        signal clicked()
        width: parent ? parent.width : 0
        height: 40
        radius: Theme.radiusMd
        color: actionArea.containsMouse ? Theme.bgHover : "transparent"
        Icon {
            id: actionIcon
            x: Theme.space3
            anchors.verticalCenter: parent.verticalCenter
            name: action.iconName
            size: Theme.iconSizeMd
            color: Theme.accent
        }
        Text {
            anchors.left: actionIcon.right
            anchors.leftMargin: Theme.space3
            anchors.right: actionKey.left
            anchors.rightMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            text: action.text
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            elide: Text.ElideRight
        }
        Kbd {
            id: actionKey
            anchors.right: parent.right
            anchors.rightMargin: Theme.space3
            anchors.verticalCenter: parent.verticalCenter
            sequence: action.shortcut
        }
        MouseArea {
            id: actionArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: action.clicked()
        }
    }
}
