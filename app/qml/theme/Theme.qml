pragma Singleton

import QtQuick
import Aion

// Design tokens (PLAN.md, secțiunea 5.3).
// Componentele folosesc numai valori de aici, fără culori hardcodate.
QtObject {
    id: theme

    // 0 = urmează sistemul, 1 = light, 2 = dark
    property int mode: AppInfo.themeOverride

    readonly property bool dark: mode === 0
        ? Qt.styleHints.colorScheme !== Qt.ColorScheme.Light
        : mode === 2

    function cycleMode() {
        mode = (mode + 1) % 3
    }

    readonly property string modeLabel: ["Sistem", "Light", "Dark"][mode]

    // Culorile personalizate (Preferințe → Aspect) au prioritate față de cele implicite ale modului curent.
    readonly property var overrides: {
        try {
            const all = JSON.parse(UserPrefs.themeOverrides)
            return (dark ? all.dark : all.light) || {}
        } catch (e) {
            return {}
        }
    }
    function pick(name, darkDefault, lightDefault) {
        const value = overrides[name]
        return value !== undefined && value !== "" ? value : (dark ? darkDefault : lightDefault)
    }
    // Schimbă (sau, cu "", resetează) o culoare a temei pentru modul curent.
    function setOverride(name, value) {
        let all = {}
        try { all = JSON.parse(UserPrefs.themeOverrides) } catch (e) { all = {} }
        const key = dark ? "dark" : "light"
        all[key] = all[key] || {}
        if (value === "")
            delete all[key][name]
        else
            all[key][name] = String(value)
        UserPrefs.themeOverrides = JSON.stringify(all)
    }
    function resetOverrides() {
        let all = {}
        try { all = JSON.parse(UserPrefs.themeOverrides) } catch (e) { all = {} }
        delete all[dark ? "dark" : "light"]
        UserPrefs.themeOverrides = JSON.stringify(all)
    }

    // Fundaluri
    readonly property color bgApp: pick("bgApp", "#0A0A0B", "#FAFAFA")
    readonly property color bgPanel: pick("bgPanel", "#141416", "#FFFFFF")
    readonly property color bgElevated: pick("bgElevated", "#1C1C1F", "#FFFFFF")
    readonly property color bgInput: dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.03)
    readonly property color bgHover: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.05)
    readonly property color bgActive: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color scrim: dark ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(0, 0, 0, 0.18)

    // Margini
    readonly property color borderSubtle: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color borderStrong: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.14)

    // Text
    readonly property color textPrimary: dark ? "#EDEDED" : "#171717"
    readonly property color textSecondary: dark ? "#A1A1AA" : "#52525B"
    readonly property color textTertiary: dark ? "#71717A" : "#A1A1AA"

    // Accent (singura culoare „vie” din UI)
    readonly property color accent: pick("accent", "#3B82F6", "#2563EB")
    readonly property color accentHover: dark ? Qt.lighter(accent, 1.25) : Qt.darker(accent, 1.15)
    readonly property color accentSubtle: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.16 : 0.10)
    readonly property color accentText: "#FFFFFF"
    readonly property color danger: dark ? "#F87171" : "#DC2626"
    readonly property color focusRing: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.55 : 0.45)

    readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(0, 0, 0, 0.14)

    // Viewport
    readonly property color viewportTop: pick("viewportTop", "#1A1B1F", "#EEF1F5")
    readonly property color viewportBottom: pick("viewportBottom", "#0E0F11", "#D9DEE5")
    readonly property color gridMinor: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.07)
    readonly property color gridMajor: dark ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(0, 0, 0, 0.15)
    readonly property color edge: pick("edge", "#0B0B0D", "#1F2023") // muchiile fețelor (contrast cu fața)
    readonly property color edgeFree: dark ? "#C9CBD1" : "#1F2023" // liniile fără fețe (contrast cu fundalul)
    readonly property color faceFront: pick("faceFront", "#D9D6D0", "#ECEAE5")
    readonly property color faceBack: pick("faceBack", "#7D8AA3", "#9AA6BD")

    // Axe
    readonly property color axisX: "#EF4444"
    readonly property color axisY: "#22C55E"
    readonly property color axisZ: "#3B82F6"

    // Inferență (snap): capăt = axisY, pe muchie = axisX, pe față = axisZ; în plus:
    readonly property color inferenceCyan: "#06B6D4" // mijloc
    readonly property color inferenceMagenta: "#D946EF" // paralel / perpendicular

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

    // Tipografie
    readonly property string fontUi: "Inter"
    readonly property string fontMono: "Geist Mono"
    readonly property int fontSizeXs: 11
    readonly property int fontSizeSm: 12
    readonly property int fontSizeMd: 13
    readonly property int fontSizeLg: 15
    readonly property int iconSizeSm: 14
    readonly property int iconSizeMd: 16
    readonly property int iconSizeLg: 18

    // Dimensiuni
    readonly property int titleBarHeight: 44
    readonly property int statusBarHeight: 28
    readonly property int sidePanelWidth: 272
    readonly property int controlHeight: 32
    readonly property int controlHeightSm: 28
    // Spațiul lăsat butoanelor native ale ferestrei (macOS).
    readonly property int trafficLightsInset: Qt.platform.os === "osx" ? 78 : 0

    // Mișcare. Cu „Reduce Motion” activ, tranzițiile devin instantanee.
    readonly property bool reduceMotion: AppInfo.reduceMotion
    readonly property int motionFast: reduceMotion ? 0 : 120
    readonly property int motionNormal: reduceMotion ? 0 : 180
}
