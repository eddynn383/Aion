import QtQuick
import Aion

// Raportul de cantități: „General” (obiecte, arii, volume, materiale, tag-uri) și „Mobilier” (lista de debitare
// a pieselor, cu necesarul de plăci și cant). Ambele se pot exporta CSV. Se recalculează la deschidere.
Popover {
    id: root

    property var document
    property int tab: 0
    property var report: ({})
    property var cuts: ({})
    property real sheetLength: 2800
    property real sheetWidth: 2070
    property real waste: 15
    property real maxThickness: 100
    // Rândurile debifate (cheile lor): la deschidere, piesele ascunse (ADR 0044). Rămân la „Recalculează” și la
    // schimbarea opțiunilor; se refac la fiecare deschidere.
    property var excludedKeys: []
    property bool cutDimensions: false

    signal exportRequested(string kind, var options)

    readonly property var cutOptions: ({ sheetLength: sheetLength, sheetWidth: sheetWidth, waste: waste / 100,
                                         maxThickness: maxThickness, excluded: excludedKeys,
                                         cutDimensions: cutDimensions, bandAllowance: UserPrefs.bandAllowance,
                                         bandAllowanceByType: root.parseJson(UserPrefs.bandAllowanceByType) })

    function parseJson(text) {
        try { return JSON.parse(text) } catch (e) { return ({}) }
    }
    readonly property var checkedKeys: {
        const out = {}
        for (const p of (root.cuts.parts ?? []))
            if (!root.excludedKeys.includes(p.rowKey))
                out[p.rowKey] = true
        return out
    }
    readonly property var sectionNames: [qsTrId("QuantitiesDialog.placa"), qsTrId("QuantitiesDialog.feronerie"), qsTrId("QuantitiesDialog.altele"), qsTrId("QuantitiesDialog.nedefinit")]

    function refresh() {
        if (!root.document)
            return
        root.report = root.document.quantityReport()
        root.cuts = root.document.cutList(root.cutOptions)
    }
    function setIncluded(key, included) {
        const keys = root.excludedKeys.filter(k => k !== key)
        if (!included)
            keys.push(key)
        root.excludedKeys = keys
    }
    function num(value, decimals) {
        return Number(value).toLocaleString(Qt.locale("ro_RO"), "f", decimals)
    }

    modal: true
    anchors.centerIn: parent
    width: Math.min(1040, (parent ? parent.width : 1040) - 80)
    height: Math.min(700, (parent ? parent.height : 700) - 80)
    padding: Theme.space6

    onAboutToShow: {
        root.excludedKeys = []
        refresh()
        root.excludedKeys = (root.cuts.parts ?? []).filter(p => p.hidden).map(p => p.rowKey)
    }
    onCutOptionsChanged: if (opened) cuts = document.cutList(cutOptions)

    contentItem: Item {
        Row {
            id: header
            width: parent.width
            Text {
                width: parent.width - closeButton.width
                anchors.verticalCenter: parent.verticalCenter
                text: qsTrId("QuantitiesDialog.raport_de_cantitati")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
            }
            IconButton {
                id: closeButton
                iconName: "x"
                tooltip: qsTrId("QuantitiesDialog.inchide")
                onClicked: root.close()
            }
        }

        Tabs {
            id: tabs
            anchors.top: header.bottom
            anchors.topMargin: Theme.space3
            width: 420
            model: [qsTrId("QuantitiesDialog.general"), qsTrId("QuantitiesDialog.mobilier_lista_de_debitare")]
            currentIndex: root.tab
            onCurrentIndexChanged: root.tab = currentIndex
        }

        // --- General ---
        Item {
            visible: root.tab === 0
            anchors.top: tabs.bottom
            anchors.topMargin: Theme.space4
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.bottomMargin: Theme.space4

            Row {
                id: cards
                width: parent.width
                spacing: Theme.space3
                Repeater {
                    model: [
                        { label: qsTrId("QuantitiesDialog.arie_totala"), value: root.num(root.report.totalArea ?? 0, 2) + qsTrId("QuantitiesDialog.m2") },
                        { label: qsTrId("QuantitiesDialog.volum_solide"), value: root.num(root.report.totalVolume ?? 0, 3) + qsTrId("QuantitiesDialog.m3") },
                        { label: qsTrId("QuantitiesDialog.suprafete_orizontale"), value: root.num(root.report.horizontalArea ?? 0, 2) + qsTrId("QuantitiesDialog.m2") },
                        { label: qsTrId("QuantitiesDialog.lungime_muchii"), value: root.num(root.report.edgeLength ?? 0, 1) + qsTrId("QuantitiesDialog.m") },
                        { label: qsTrId("QuantitiesDialog.fete"), value: String(root.report.faceCount ?? 0) }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        width: (cards.width - cards.spacing * 4) / 5
                        height: 58
                        radius: Theme.radiusMd
                        color: Theme.bgHover
                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space3
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                text: modelData.label
                                color: Theme.textTertiary
                                font.pixelSize: Theme.fontSizeXs
                            }
                            Text {
                                text: modelData.value
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeLg
                                font.family: Theme.fontMono
                            }
                        }
                    }
                }
            }

            DataTable {
                id: objectsTable
                anchors.top: cards.bottom
                anchors.topMargin: Theme.space4
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: parent.width * 0.68
                columns: [
                    { title: qsTrId("QuantitiesDialog.obiect"), width: 0.30 },
                    { title: qsTrId("QuantitiesDialog.buc"), width: 0.07, align: "right" },
                    { title: qsTrId("QuantitiesDialog.material"), width: 0.15 },
                    { title: qsTrId("QuantitiesDialog.dimensiuni_x_y_z_mm"), width: 0.24, align: "right" },
                    { title: qsTrId("QuantitiesDialog.arie_m2"), width: 0.12, align: "right" },
                    { title: qsTrId("QuantitiesDialog.volum_m3"), width: 0.12, align: "right" }
                ]
                rows: (root.report.objects ?? []).map(o => [
                    o.name, String(o.count), o.material,
                    root.num(o.sizeX, 0) + " × " + root.num(o.sizeY, 0) + " × " + root.num(o.sizeZ, 0),
                    root.num(o.area, 3), o.solid ? root.num(o.volume, 4) : "—"
                ])
                emptyText: qsTrId("QuantitiesDialog.modelul_e_gol")
            }

            Column {
                anchors.top: cards.bottom
                anchors.topMargin: Theme.space4
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: parent.width * 0.3
                spacing: Theme.space3
                DataTable {
                    width: parent.width
                    height: (parent.height - Theme.space3) / 2
                    columns: [{ title: qsTrId("QuantitiesDialog.material"), width: 0.6 }, { title: qsTrId("QuantitiesDialog.arie_m2"), width: 0.4, align: "right" }]
                    rows: (root.report.materials ?? []).map(m => [m.name, root.num(m.area, 3)])
                }
                DataTable {
                    width: parent.width
                    height: (parent.height - Theme.space3) / 2
                    columns: [{ title: qsTrId("QuantitiesDialog.tag"), width: 0.6 }, { title: qsTrId("QuantitiesDialog.arie_m2"), width: 0.4, align: "right" }]
                    rows: (root.report.tags ?? []).map(t => [t.name, root.num(t.area, 3)])
                }
            }
        }

        // --- Mobilier ---
        Item {
            visible: root.tab === 1
            anchors.top: tabs.bottom
            anchors.topMargin: Theme.space4
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.bottomMargin: Theme.space4

            Row {
                id: settings
                width: parent.width
                spacing: Theme.space2
                NumberField {
                    width: 180
                    label: qsTrId("QuantitiesDialog.placa_l")
                    unit: "mm"
                    from: 100
                    to: 10000
                    value: root.sheetLength
                    onValueModified: (v) => root.sheetLength = v
                }
                NumberField {
                    width: 180
                    label: qsTrId("QuantitiesDialog.placa_l_2")
                    unit: "mm"
                    from: 100
                    to: 10000
                    value: root.sheetWidth
                    onValueModified: (v) => root.sheetWidth = v
                }
                NumberField {
                    width: 150
                    label: qsTrId("QuantitiesDialog.pierderi")
                    unit: "%"
                    from: 0
                    to: 80
                    value: root.waste
                    onValueModified: (v) => root.waste = v
                }
                NumberField {
                    width: 200
                    label: qsTrId("QuantitiesDialog.placa_pana_la")
                    unit: "mm"
                    from: 1
                    to: 1000
                    value: root.maxThickness
                    onValueModified: (v) => root.maxThickness = v
                }
            }
            Text {
                id: hint
                anchors.top: settings.bottom
                anchors.topMargin: Theme.space2
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTrId("QuantitiesDialog.placile_cu_tip_din_biblioteca_sau_fa_placa_")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }

            Rectangle {
                id: hiddenBanner
                anchors.top: hint.bottom
                anchors.topMargin: visible ? Theme.space2 : 0
                width: parent.width * 0.64
                height: visible ? 36 : 0
                visible: (root.cuts.hiddenPieces ?? 0) > 0
                radius: Theme.radiusMd
                color: Theme.bgHover
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space3
                    anchors.verticalCenter: parent.verticalCenter
                    text: (root.cuts.hiddenPieces ?? 0) + qsTrId("QuantitiesDialog.piese_ascunse_nu_sunt_incluse")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSm
                }
                Button {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("QuantitiesDialog.include_toate")
                    onClicked: root.excludedKeys = []
                }
            }

            DataTable {
                anchors.top: hiddenBanner.bottom
                anchors.topMargin: Theme.space3
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: parent.width * 0.64
                checkColumn: true
                rowKeys: (root.cuts.parts ?? []).map(p => p.rowKey)
                checkedKeys: root.checkedKeys
                dimmedRows: {
                    const out = {}
                    ;(root.cuts.parts ?? []).forEach((p, i) => { if (p.hidden) out[i] = true })
                    return out
                }
                onRowToggled: (key, checked) => root.setIncluded(key, checked)
                columns: [
                    { title: qsTrId("QuantitiesDialog.sect"), width: 0.08 },
                    { title: qsTrId("QuantitiesDialog.piesa"), width: 0.21 },
                    { title: qsTrId("QuantitiesDialog.tip"), width: 0.07 },
                    { title: qsTrId("QuantitiesDialog.decor"), width: 0.12 },
                    { title: qsTrId("QuantitiesDialog.l_mm"), width: 0.11, align: "right" },
                    { title: qsTrId("QuantitiesDialog.l_mm_2"), width: 0.09, align: "right" },
                    { title: qsTrId("QuantitiesDialog.g_mm"), width: 0.08, align: "right" },
                    { title: qsTrId("QuantitiesDialog.buc"), width: 0.05, align: "right" },
                    { title: qsTrId("QuantitiesDialog.motiv"), width: 0.19 }
                ]
                rows: (root.cuts.parts ?? []).map(p => {
                    const measured = p.section !== 1
                    const length = root.cutDimensions ? p.cutLength : p.length
                    const width = root.cutDimensions ? p.cutWidth : p.width
                    return [root.sectionNames[p.section] ?? "", p.usedIn, p.type, p.material,
                            measured ? root.num(length, 1) : "", measured ? root.num(width, 1) : "",
                            measured ? root.num(p.thickness, 1) : "", String(p.count), p.flags]
                })
                emptyText: qsTrId("QuantitiesDialog.nicio_piesa_adauga_placi_din_biblioteca_sau")
            }

            Column {
                anchors.top: hint.bottom
                anchors.topMargin: Theme.space3
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: parent.width * 0.34
                spacing: Theme.space3
                DataTable {
                    width: parent.width
                    height: (parent.height - Theme.space3 * 2) * 0.4
                    columns: [
                        { title: qsTrId("QuantitiesDialog.placi_brute"), width: 0.3 },
                        { title: qsTrId("QuantitiesDialog.g_mm"), width: 0.16, align: "right" },
                        { title: qsTrId("QuantitiesDialog.arie_m2"), width: 0.2, align: "right" },
                        { title: qsTrId("QuantitiesDialog.placi"), width: 0.14, align: "right" },
                        { title: qsTrId("QuantitiesDialog.format"), width: 0.2, align: "right" }
                    ]
                    rows: (root.cuts.sheets ?? []).map(s => [s.material, root.num(s.thickness, 1), root.num(s.area, 2),
                                                              String(s.sheets), s.sheetSize])
                    emptyText: qsTrId("QuantitiesDialog.nicio_placa_inclusa")
                }
                DataTable {
                    width: parent.width
                    height: (parent.height - Theme.space3 * 2) * 0.3
                    columns: [
                        { title: qsTrId("QuantitiesDialog.cant"), width: 0.34 },
                        { title: qsTrId("QuantitiesDialog.decor"), width: 0.3 },
                        { title: qsTrId("QuantitiesDialog.muchii"), width: 0.16, align: "right" },
                        { title: "m", width: 0.2, align: "right" }
                    ]
                    rows: (root.cuts.bands ?? []).map(b => [b.band, b.decor, String(b.edges), root.num(b.meters, 2)])
                        .concat((root.cuts.edgeBanding ?? 0) > 0
                                ? [[qsTrId("QuantitiesDialog.estimat_fara_tip"), "", "", root.num(root.cuts.edgeBanding, 2)]] : [])
                    emptyText: qsTrId("QuantitiesDialog.fara_cant")
                }
                DataTable {
                    width: parent.width
                    height: (parent.height - Theme.space3 * 2) * 0.3
                    columns: [{ title: qsTrId("QuantitiesDialog.feronerie"), width: 0.75 }, { title: qsTrId("QuantitiesDialog.buc"), width: 0.25, align: "right" }]
                    rows: (root.cuts.hardware ?? []).map(h => [h.label, String(h.count)])
                    emptyText: qsTrId("QuantitiesDialog.fara_feronerie")
                }
            }
        }

        Item {
            id: footer
            anchors.bottom: parent.bottom
            width: parent.width
            height: Theme.controlHeight
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space4
                Toggle {
                    visible: root.tab === 1
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("QuantitiesDialog.dimensiuni_de_taiere_fara_cantul_care_se_sc")
                    checked: root.cutDimensions
                    onToggled: root.cutDimensions = checked
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.tab === 1 && (root.cuts.hiddenPieces ?? 0) > 0
                          ? qsTrId("QuantitiesDialog.piesele_ascunse_debifate_nu_intra_in_csv")
                          : qsTrId("QuantitiesDialog.csv_pentru_excel_separator_virgula_zecimala")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                }
            }
            Row {
                anchors.right: parent.right
                spacing: Theme.space2
                Button {
                    text: qsTrId("QuantitiesDialog.recalculeaza")
                    iconName: "rotate-ccw"
                    onClicked: root.refresh()
                }
                Button {
                    variant: "primary"
                    text: qsTrId("QuantitiesDialog.exporta_csv")
                    iconName: "arrow-up"
                    onClicked: root.exportRequested(root.tab === 0 ? "general" : "cutlist", root.cutOptions)
                }
            }
        }
    }
}
