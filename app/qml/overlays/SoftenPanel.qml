import QtQuick
import QtQuick.Templates as T
import Aion

// Muchii netede (Soften): muchiile dintre fețe cu unghi mai mic decât pragul devin soft (ascunse, cu umbrire netedă).
// Se aplică pe muchiile selecției sau pe toate muchiile obiectelor selectate, cu previzualizare.
Popover {
    id: root

    property var document
    property real angle: 20
    property bool coplanar: true
    // Rezultatul ultimei previzualizări: { soft, total }.
    property var result: ({})

    function start() {
        root.open()
        root.update()
    }
    function update() {
        if (root.visible)
            root.result = root.document.previewSoften(root.angle, root.coplanar)
    }

    modal: false
    closePolicy: T.Popup.NoAutoClose
    width: 300
    padding: Theme.space4

    contentItem: Column {
        spacing: Theme.space3
        Text {
            text: qsTrId("SoftenPanel.muchii_netede")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Slider {
            width: parent.width
            label: qsTrId("SoftenPanel.unghi_maxim_intre_fete")
            from: 0
            to: 180
            stepSize: 1
            value: root.angle
            valueText: Math.round(value) + "°"
            onMoved: { root.angle = value; root.update() }
        }
        Toggle {
            width: parent.width
            text: qsTrId("SoftenPanel.si_muchiile_dintre_fete_coplanare")
            checked: root.coplanar
            onToggled: { root.coplanar = checked; root.update() }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            visible: root.result.total !== undefined
            text: root.result.total === 0 ? qsTrId("SoftenPanel.selectia_nu_are_muchii_intre_doua_fete")
                : root.result.soft > 0 ? root.result.soft + qsTrId("SoftenPanel.din") + root.result.total + qsTrId("SoftenPanel.muchii_sunt_netezite")
                : qsTrId("SoftenPanel.nicio_muchie_nu_e_sub_prag_muchiile_unui_cub_au_")
            color: root.result.soft > 0 ? Theme.textSecondary : Theme.textPrimary
            font.pixelSize: Theme.fontSizeSm
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("SoftenPanel.sub_prag_muchia_dispare_si_umbrirea_trece_lin_pe")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Row {
            anchors.right: parent.right
            spacing: Theme.space2
            Button {
                text: qsTrId("SoftenPanel.anuleaza")
                onClicked: { root.document.cancelOperation(); root.close() }
            }
            Button {
                variant: "primary"
                text: qsTrId("SoftenPanel.gata")
                onClicked: { root.document.commitOperation(); root.close() }
            }
        }
    }
}
