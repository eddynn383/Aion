pragma Singleton

import QtQuick

// Design tokens (PLAN.md, secțiunea 5.3).
// Componentele folosesc numai valori de aici, fără culori hardcodate.
QtObject {
    id: theme

    // 0 = urmează sistemul, 1 = light, 2 = dark
    property int mode: 0

    readonly property bool dark: mode === 0
        ? Qt.styleHints.colorScheme !== Qt.ColorScheme.Light
        : mode === 2

    function cycleMode() {
        mode = (mode + 1) % 3
    }

    readonly property string modeLabel: ["Sistem", "Light", "Dark"][mode]

    // Culori
    readonly property color bgApp: dark ? "#0A0A0B" : "#FAFAFA"
    readonly property color bgPanel: dark ? "#141416" : "#FFFFFF"
    readonly property color bgElevated: dark ? "#1C1C1F" : "#FFFFFF"
    readonly property color bgHover: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.05)
    readonly property color bgActive: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color borderSubtle: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color textPrimary: dark ? "#EDEDED" : "#171717"
    readonly property color textSecondary: dark ? "#A1A1AA" : "#52525B"
    readonly property color textTertiary: dark ? "#71717A" : "#A1A1AA"
    readonly property color accent: dark ? "#3B82F6" : "#2563EB"
    readonly property color accentText: "#FFFFFF"
    readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(0, 0, 0, 0.12)

    // Viewport (fundal cu gradient)
    readonly property color viewportTop: dark ? "#1A1B1F" : "#EEF1F5"
    readonly property color viewportBottom: dark ? "#0E0F11" : "#D9DEE5"

    // Axe
    readonly property color axisX: "#EF4444"
    readonly property color axisY: "#22C55E"
    readonly property color axisZ: "#3B82F6"

    // Raze
    readonly property int radiusSm: 6
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12

    // Spațiere (grilă de 4 px)
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space6: 24
    readonly property int space8: 32

    // Tipografie. Inter și Geist Mono se încarcă din resurse în Faza 1;
    // până atunci se folosește fontul sistemului.
    readonly property string fontUi: "Inter"
    readonly property string fontMono: "Geist Mono"
    readonly property int fontSizeSm: 11
    readonly property int fontSizeMd: 13
    readonly property int fontSizeLg: 15

    // Dimensiuni
    readonly property int titleBarHeight: 44
    readonly property int statusBarHeight: 28
    readonly property int sidePanelWidth: 240
    readonly property int controlHeight: 32

    // Mișcare
    readonly property int motionFast: 120
    readonly property int motionNormal: 180
}
