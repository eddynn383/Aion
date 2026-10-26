import QtQuick
import QtQuick.Templates as T
import Aion

// Tooltip-ul bogat (ca în Photoshop): după un hover lung pe o unealtă, arată ghidul ei cu animație.
// Se închide când cursorul pleacă de pe buton; se poate dezactiva din Preferințe.
Popover {
    id: root

    property string toolId: ""
    property string shortcut: ""

    width: 300
    padding: Theme.space4
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    // Deasupra butonului (toolbar-ul e jos), centrat pe el; Popup-ul se ține singur în fereastră (margins).
    x: parent ? (parent.width - width) / 2 : 0
    y: -implicitHeight - Theme.space3

    contentItem: GuideCard {
        toolId: root.toolId
        shortcut: root.shortcut
        playing: root.opened
    }
}
