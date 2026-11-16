import QtQuick
import QtQuick.Controls.Basic
import ShineShift.Core

Window {
    id: window

    readonly property int gap: 12
    readonly property string corner: Config.flyoutCorner
    readonly property int limit: Displays.layered ? Math.max(200, height - 2 * gap) : 720

    title: "ShineShift"
    color: "transparent"
    flags: Displays.layered ? Qt.FramelessWindowHint : Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    width: Displays.layered ? 640 : card.width + 2 * gap
    height: Displays.layered ? 480 : card.height + 2 * gap
    onActiveChanged: if (!active && visible && !Displays.layered) Displays.closeFlyout()

    Shortcut {
        sequence: "Escape"
        onActivated: Displays.closeFlyout()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: Displays.closeFlyout()
    }

    Surface {
        id: card
        width: 360
        height: Math.min(window.limit, header.height + list.contentHeight + 38)
        x: window.corner.endsWith("left") ? window.gap : window.width - width - window.gap
        y: window.corner.startsWith("top") ? window.gap : window.height - height - window.gap

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Item {
            id: header
            x: 16
            y: 12
            width: parent.width - 28
            height: 32

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Brightness")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                IconButton {
                    visible: Displays.monitors.length > 1
                    icon: "link_regular"
                    checked: Config.linked
                    iconColor: Config.linked ? Theme.accent : Theme.text
                    tooltip: Config.linked ? qsTr("Unlink levels") : qsTr("Link levels")
                    onClicked: Config.linked = !Config.linked
                }
                IconButton {
                    icon: "power_regular"
                    tooltip: qsTr("Turn off displays")
                    onClicked: Displays.turnOffDisplays()
                }
                IconButton {
                    icon: "settings_regular"
                    tooltip: qsTr("Settings")
                    onClicked: Displays.openSettings()
                }
            }
        }

        Flickable {
            id: list
            x: 16
            y: header.y + header.height + 6
            width: parent.width - 32
            height: parent.height - y - 14
            contentHeight: content.implicitHeight
            clip: contentHeight > height
            // A mouse drag belongs to the sliders; touch and the wheel still scroll.
            acceptedButtons: Qt.NoButton
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: Scroller {}

            Column {
                id: content
                width: list.width
                spacing: 14

                Repeater {
                    model: Displays.monitors
                    MonitorCard {
                        required property var modelData
                        width: content.width
                        monitor: modelData
                    }
                }

                Column {
                    visible: Displays.monitors.length === 0
                    width: parent.width
                    spacing: 10
                    topPadding: 6
                    bottomPadding: 4

                    Row {
                        visible: Displays.detecting
                        spacing: 10
                        Spinner { anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Looking for displays")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSize
                        }
                    }
                    Text {
                        visible: !Displays.detecting
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: Displays.error.length > 0
                            ? qsTr("DDC/CI could not start: %1").arg(Displays.error)
                            : qsTr("No displays answered. Check that DDC/CI is switched on in your monitor's own menu.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSize
                    }
                    IconButton {
                        visible: !Displays.detecting
                        variant: "standard"
                        icon: "arrow_sync_regular"
                        text: qsTr("Look again")
                        onClicked: Displays.refresh()
                    }
                }
            }
        }
    }
}
