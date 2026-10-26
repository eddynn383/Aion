import QtQuick
import QtQuick.Shapes
import Aion

// Gizmo de navigare minimalist: axele camerei proiectate pe ecran.
// Click pe un capăt de axă = vederea standard corespunzătoare; click pe centru = Iso.
Item {
    id: root

    property vector3d cameraRight: Qt.vector3d(1, 0, 0)
    property vector3d cameraUp: Qt.vector3d(0, 0, 1)
    property vector3d cameraForward: Qt.vector3d(0, 1, 0)

    // Numele vederilor corespund cu Viewport.StandardView.
    signal viewRequested(string view)

    width: 92
    height: 92

    readonly property real radius: 32

    function screenOf(axis) {
        return Qt.point(width / 2 + axis.dotProduct(cameraRight) * radius,
                        height / 2 - axis.dotProduct(cameraUp) * radius)
    }
    // Adâncimea (spre cameră = mai mare), pentru ordinea de desenare.
    function depthOf(axis) { return -axis.dotProduct(cameraForward) }

    readonly property var axes: [
        { axis: Qt.vector3d(1, 0, 0),  color: Theme.axisX, label: "X", view: "Right",  positive: true },
        { axis: Qt.vector3d(0, 1, 0),  color: Theme.axisY, label: "Y", view: "Back",   positive: true },
        { axis: Qt.vector3d(0, 0, 1),  color: Theme.axisZ, label: "Z", view: "Top",    positive: true },
        { axis: Qt.vector3d(-1, 0, 0), color: Theme.axisX, label: "",  view: "Left",   positive: false },
        { axis: Qt.vector3d(0, -1, 0), color: Theme.axisY, label: "",  view: "Front",  positive: false },
        { axis: Qt.vector3d(0, 0, -1), color: Theme.axisZ, label: "",  view: "Bottom", positive: false }
    ]

    Rectangle {
        anchors.centerIn: parent
        width: root.radius * 2 + 28
        height: width
        radius: width / 2
        color: hover.hovered ? Theme.bgHover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
    HoverHandler { id: hover }

    // Liniile de la centru spre capetele pozitive.
    Repeater {
        model: 3
        delegate: Shape {
            required property int index
            readonly property var a: root.axes[index]
            readonly property point p: root.screenOf(a.axis)
            anchors.fill: parent
            z: root.depthOf(a.axis)
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: a.color
                strokeWidth: 2
                capStyle: ShapePath.RoundCap
                startX: root.width / 2
                startY: root.height / 2
                PathLine { x: p.x; y: p.y }
            }
        }
    }

    Repeater {
        model: root.axes
        delegate: Rectangle {
            id: knob
            required property var modelData
            readonly property point p: root.screenOf(modelData.axis)
            readonly property bool positive: modelData.positive
            z: root.depthOf(modelData.axis) + 0.01
            width: positive ? 18 : 12
            height: width
            radius: width / 2
            x: p.x - width / 2
            y: p.y - height / 2
            color: positive ? modelData.color : Qt.rgba(modelData.color.r, modelData.color.g, modelData.color.b,
                                                         knobHover.hovered ? 0.55 : 0.28)
            border.color: positive ? "transparent" : modelData.color
            border.width: positive ? 0 : 1.5
            scale: knobHover.hovered ? 1.15 : 1
            Behavior on scale { NumberAnimation { duration: Theme.motionFast } }

            Text {
                anchors.centerIn: parent
                visible: knob.positive
                text: knob.modelData.label
                color: "#FFFFFF"
                font.pixelSize: 10
                font.weight: Font.Bold
            }

            HoverHandler { id: knobHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.viewRequested(knob.modelData.view) }
        }
    }

    // Centrul: vederea izometrică.
    Rectangle {
        anchors.centerIn: parent
        z: 10
        width: 8
        height: 8
        radius: 4
        color: centerHover.hovered ? Theme.textPrimary : Theme.textTertiary
        HoverHandler { id: centerHover; cursorShape: Qt.PointingHandCursor; margin: 4 }
        TapHandler { margin: 4; onTapped: root.viewRequested("Iso") }
    }
}
