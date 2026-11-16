import QtQuick
import QtQuick.Controls.Basic

// One labelled level: an icon, a slider and its number. `moved` fires for drags and the wheel alike.
Item {
    id: root
    property string icon
    property string label
    property real value: 0
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real wheelAmount: 5
    property int decimals: 0
    signal moved(real value)

    implicitHeight: 30
    opacity: enabled ? 1 : 0.4

    FluentIcon {
        id: glyph
        anchors.verticalCenter: parent.verticalCenter
        name: root.icon
        size: 18
        color: Theme.textMuted
    }

    MouseArea {
        id: glyphMouse
        anchors.fill: glyph
        anchors.margins: -4
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    Tip {
        parent: glyph
        visible: glyphMouse.containsMouse && root.label.length > 0
        text: root.label
    }

    Slider {
        id: slider
        anchors.left: glyph.right
        anchors.leftMargin: 10
        anchors.right: number.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        implicitHeight: 24
        padding: 0
        from: root.from
        to: root.to
        stepSize: root.stepSize
        Accessible.name: root.label
        onMoved: root.moved(value)

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: slider.availableWidth
            height: 6
            radius: 3
            color: Theme.containerHighest
            Rectangle {
                width: Math.max(height, slider.visualPosition * parent.width)
                height: parent.height
                radius: 3
                color: Theme.accent
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: slider.pressed ? 18 : 16
            height: width
            radius: width / 2
            color: Theme.accent
            border.width: slider.visualFocus ? 2 : 3
            border.color: slider.visualFocus ? Theme.text : Theme.surface
        }
    }

    // The backend's value only drives the handle while nobody is holding it.
    Binding {
        target: slider
        property: "value"
        value: root.value
        when: !slider.pressed
        restoreMode: Binding.RestoreNone
    }

    Text {
        id: number
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 34
        horizontalAlignment: Text.AlignRight
        text: Number(slider.pressed ? slider.value : root.value).toFixed(root.decimals)
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSize
        font.features: { "tnum": 1 }
    }

    WheelHandler {
        // Wheels report 120 per notch and touchpads much less, so partial notches add up.
        property real travelled: 0
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            travelled += event.angleDelta.y
            const notches = travelled > 0 ? Math.floor(travelled / 120) : Math.ceil(travelled / 120)
            if (notches === 0) return
            travelled -= notches * 120
            root.moved(Math.max(root.from, Math.min(root.to, root.value + notches * root.wheelAmount)))
        }
    }
}
