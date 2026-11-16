import QtQuick
import ShineShift.Core

// Everything the flyout shows for one display.
Item {
    id: root
    required property Monitor monitor

    implicitHeight: column.implicitHeight

    Column {
        id: column
        width: parent.width
        spacing: 4

        Item {
            width: parent.width
            height: 28

            FluentIcon {
                id: screen
                anchors.verticalCenter: parent.verticalCenter
                name: "desktop_regular"
                size: 18
            }
            Text {
                id: title
                anchors.left: screen.right
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, parent.width - 28 - badges.width - 12)
                text: root.monitor.name
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSize
                font.weight: Font.DemiBold
            }
            Row {
                id: badges
                anchors.left: title.right
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Rectangle {
                    visible: root.monitor.hdr
                    anchors.verticalCenter: parent.verticalCenter
                    width: hdrLabel.implicitWidth + 10
                    height: 18
                    radius: 4
                    color: Theme.selection
                    Text {
                        id: hdrLabel
                        anchors.centerIn: parent
                        text: "HDR"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.smallSize - 1
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.4
                    }
                }
                FluentIcon {
                    visible: root.monitor.hold
                    anchors.verticalCenter: parent.verticalCenter
                    name: "lock_closed_regular"
                    size: 14
                    color: Theme.textMuted
                }
            }
            IconButton {
                visible: root.monitor.showPower
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 28
                implicitHeight: 28
                icon: "power_regular"
                iconColor: root.monitor.powered ? Theme.text : Theme.textMuted
                tooltip: root.monitor.powered ? qsTr("Power off") : qsTr("Power on")
                onClicked: root.monitor.setPowered(!root.monitor.powered)
            }
        }

        LevelSlider {
            width: parent.width
            icon: "brightness_high_regular"
            label: qsTr("Brightness")
            value: root.monitor.brightness
            onMoved: value => root.monitor.setBrightness(Math.round(value))
        }
        LevelSlider {
            visible: root.monitor.hdr && root.monitor.sdrAdjustable
            width: parent.width
            icon: "hdr_regular"
            label: qsTr("SDR brightness")
            from: 0.25
            to: 3.0
            stepSize: 0.05
            wheelAmount: 0.05
            decimals: 2
            value: root.monitor.sdr
            onMoved: value => root.monitor.setSdr(value)
        }
        Text {
            visible: root.monitor.hdr && !root.monitor.sdrAdjustable && Displays.compositor === "hyprland"
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("SDR brightness needs an hl.monitor rule for %1 in your Hyprland config.").arg(root.monitor.connector)
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.smallSize
        }
        LevelSlider {
            visible: root.monitor.showContrast
            width: parent.width
            icon: "circle_half_fill_regular"
            label: qsTr("Contrast")
            value: root.monitor.contrast
            onMoved: value => root.monitor.setContrast(Math.round(value))
        }
        LevelSlider {
            visible: root.monitor.showVolume
            width: parent.width
            icon: "speaker_2_regular"
            label: qsTr("Volume")
            value: root.monitor.volume
            onMoved: value => root.monitor.setVolume(Math.round(value))
        }
        Repeater {
            model: root.monitor.features
            LevelSlider {
                required property var modelData
                width: column.width
                icon: "options_regular"
                enabled: modelData.known
                label: modelData.label
                value: modelData.value
                onMoved: value => root.monitor.setFeature(modelData.code, Math.round(value))
            }
        }
        Item {
            visible: root.monitor.showInput && root.monitor.inputs.length > 0
            width: parent.width
            height: visible ? 36 : 0

            FluentIcon {
                id: plug
                anchors.verticalCenter: parent.verticalCenter
                name: "plug_connected_regular"
                size: 18
                color: Theme.textMuted
            }
            Dropdown {
                anchors.left: plug.right
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                Accessible.name: qsTr("Input")
                model: root.monitor.inputs
                current: root.monitor.input
                onActivated: root.monitor.setInput(currentValue)
            }
        }
    }
}
