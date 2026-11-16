import QtQuick
import ShineShift.Core

Window {
    id: window
    width: pill.width + 32
    height: pill.height + 32
    color: "transparent"
    title: "ShineShift level"
    flags: Displays.layered
        ? Qt.FramelessWindowHint | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus
        : Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus | Qt.X11BypassWindowManagerHint

    Surface {
        id: pill
        anchors.centerIn: parent
        width: 300
        height: 52
        radius: height / 2

        FluentIcon {
            id: glyph
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            name: Displays.overlayIcon
            size: 20
        }
        Text {
            id: title
            anchors.left: glyph.right
            anchors.leftMargin: 10
            anchors.right: number.left
            anchors.rightMargin: 8
            y: 9
            text: Displays.overlayTitle
            elide: Text.ElideRight
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.smallSize
        }
        Rectangle {
            anchors.left: title.left
            anchors.right: title.right
            y: 31
            height: 5
            radius: 2.5
            color: Theme.containerHighest
            Rectangle {
                width: Math.max(height, parent.width * Displays.overlayLevel)
                height: parent.height
                radius: 2.5
                color: Theme.accent
                Behavior on width { NumberAnimation { duration: 90 } }
            }
        }
        Text {
            id: number
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            horizontalAlignment: Text.AlignRight
            text: Displays.overlayText
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 15
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
    }
}
