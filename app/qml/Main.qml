import QtQuick
import Forma

Window {
    id: window

    width: 1440
    height: 900
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Forma"
    color: Theme.bgApp

    property bool sidePanelsVisible: true

    Shortcut {
        sequences: ["Ctrl+\\"]
        onActivated: window.sidePanelsVisible = !window.sidePanelsVisible
    }

    TitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        onCommandPaletteRequested: statusBar.message = "Command palette vine în Faza 1"
    }

    Panel {
        id: leftPanel
        title: "Straturi"
        anchors.top: titleBar.bottom
        anchors.bottom: statusBar.top
        anchors.left: parent.left
        width: window.sidePanelsVisible ? Theme.sidePanelWidth : 0
        visible: width > 0
        clip: true

        Behavior on width { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }

        Text {
            text: "Modelul e gol"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeMd
        }
    }

    Rectangle {
        anchors.top: leftPanel.top
        anchors.bottom: leftPanel.bottom
        anchors.left: leftPanel.right
        width: 1
        color: Theme.borderSubtle
        visible: leftPanel.visible
    }

    // Loc rezervat pentru viewport-ul 3D (QQuickRhiItem, Faza 1).
    Item {
        id: viewport
        anchors.top: titleBar.bottom
        anchors.bottom: statusBar.top
        anchors.left: leftPanel.right
        anchors.right: rightPanel.left

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.viewportTop }
                GradientStop { position: 1.0; color: Theme.viewportBottom }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: Theme.space2

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.space2
                Repeater {
                    model: [Theme.axisX, Theme.axisY, Theme.axisZ]
                    delegate: Rectangle {
                        required property color modelData
                        width: 24; height: 3; radius: 1.5
                        color: modelData
                    }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Viewport 3D"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeLg
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Randarea începe în Faza 1"
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeSm
            }
        }

        Toolbar {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space6
        }
    }

    Panel {
        id: rightPanel
        title: "Inspector"
        anchors.top: titleBar.bottom
        anchors.bottom: statusBar.top
        anchors.right: parent.right
        width: window.sidePanelsVisible ? Theme.sidePanelWidth : 0
        visible: width > 0
        clip: true

        Behavior on width { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }

        Text {
            text: "Nimic selectat"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeMd
        }
    }

    Rectangle {
        anchors.top: rightPanel.top
        anchors.bottom: rightPanel.bottom
        anchors.right: rightPanel.left
        width: 1
        color: Theme.borderSubtle
        visible: rightPanel.visible
    }

    StatusBar {
        id: statusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
    }
}
