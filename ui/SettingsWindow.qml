import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ShineShift.Core

Window {
    id: window

    visible: Displays.settingsOpen
    title: "ShineShift Settings"
    width: 680
    height: 780
    minimumWidth: 540
    minimumHeight: 420
    color: Theme.surface
    onVisibleChanged: if (visible) {
        Appearance.rescan()
        floater.attempts = 0
        floater.interval = 80
        floater.restart()
    }
    onClosing: Displays.closeSettings()

    // A tiling compositor can only float the window once it is mapped, and sway sometimes keeps the tiled size at first.
    Timer {
        id: floater
        property int attempts: 0
        interval: 80
        onTriggered: {
            Displays.floatWindow(window.title, 680, 780)
            attempts += 1
            if (attempts < 2) {
                interval = 300
                start()
            }
        }
    }

    component Heading: Text {
        Layout.topMargin: 20
        Layout.bottomMargin: 6
        Layout.leftMargin: 4
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.smallSize
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }

    component Group: Rectangle {
        default property alias rows: column.data
        Layout.fillWidth: true
        implicitHeight: column.implicitHeight
        radius: Theme.radius
        color: Theme.containerLow
        border.width: 1
        border.color: Theme.outlineVariant
        Column {
            id: column
            width: parent.width
        }
    }

    // One row of a group; `below` puts the control under the text instead of beside it.
    component Setting: Item {
        id: setting
        property string title
        property string description
        property string icon
        property bool divider: true
        property bool below: false
        default property alias control: slot.data
        width: parent ? parent.width : 0
        implicitHeight: Math.max(56, body.implicitHeight + 24)

        Rectangle {
            visible: setting.divider
            x: 14
            width: parent.width - 28
            height: 1
            color: Theme.outlineVariant
        }

        GridLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 14
            anchors.verticalCenter: parent.verticalCenter
            columns: setting.below ? 2 : 3
            columnSpacing: 12
            rowSpacing: 10

            FluentIcon {
                Layout.alignment: Qt.AlignVCenter
                name: setting.icon
                size: 18
                color: Theme.textMuted
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: setting.title
                    color: Theme.text
                    wrapMode: Text.Wrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSize
                }
                Text {
                    Layout.fillWidth: true
                    visible: setting.description.length > 0
                    text: setting.description
                    color: Theme.textMuted
                    wrapMode: Text.Wrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.smallSize + 1
                }
            }
            Item { visible: setting.below; Layout.preferredWidth: 18 }
            Item {
                id: slot
                Layout.fillWidth: setting.below
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                implicitWidth: setting.below ? 0 : childrenRect.width
                implicitHeight: childrenRect.height
            }
        }
    }

    component Percent: Field {
        property int number: 0
        signal committed(int value)
        implicitWidth: 58
        horizontalAlignment: TextInput.AlignHCenter
        validator: IntValidator { bottom: 0; top: 100 }
        text: number
        onEditingFinished: {
            committed(parseInt(text) || 0)
            text = Qt.binding(() => number)
        }
    }

    component MonitorSettings: Group {
        id: group
        required property Monitor monitor
        required property int position
        property string featureError

        Setting {
            divider: false
            icon: "desktop_regular"
            title: group.monitor.connector.length > 0 ? group.monitor.name + " (" + group.monitor.connector + ")" : group.monitor.name
            description: group.monitor.hdr ? qsTr("%1, HDR on").arg(group.monitor.model) : group.monitor.model
            Row {
                spacing: 2
                IconButton {
                    enabled: group.position > 0
                    icon: "arrow_up_regular"
                    tooltip: qsTr("Move up")
                    onClicked: Displays.move(group.monitor, -1)
                }
                IconButton {
                    enabled: group.position < Displays.allMonitors.length - 1
                    icon: "arrow_down_regular"
                    tooltip: qsTr("Move down")
                    onClicked: Displays.move(group.monitor, 1)
                }
            }
        }
        Setting {
            icon: "edit_regular"
            title: qsTr("Name")
            Field {
                width: 220
                Accessible.name: qsTr("Name")
                placeholderText: group.monitor.model
                text: group.monitor.customName
                onEditingFinished: group.monitor.name = text
            }
        }
        Setting {
            icon: "eye_regular"
            title: qsTr("Show in the flyout")
            Toggle {
                Accessible.name: qsTr("Show in the flyout")
                checked: !group.monitor.hidden
                onToggled: group.monitor.hidden = !checked
            }
        }
        Setting {
            icon: "lock_closed_regular"
            title: qsTr("Hold brightness")
            description: qsTr("Keeps checking this display and writes your brightness back whenever something else changes it.")
            Toggle {
                Accessible.name: qsTr("Hold brightness")
                checked: group.monitor.hold
                onToggled: group.monitor.hold = checked
            }
        }
        Setting {
            icon: "brightness_low_regular"
            title: qsTr("Brightness range")
            description: qsTr("The part of this display's range the slider covers, so mismatched displays can line up.")
            Row {
                spacing: 8
                Percent {
                    Accessible.name: qsTr("Lowest brightness")
                    number: group.monitor.minimum
                    onCommitted: value => group.monitor.minimum = value
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("to")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSize
                }
                Percent {
                    Accessible.name: qsTr("Highest brightness")
                    number: group.monitor.maximum
                    onCommitted: value => group.monitor.maximum = value
                }
            }
        }
        Setting {
            below: true
            icon: "options_regular"
            title: qsTr("More controls")
            description: qsTr("Extra DDC/CI controls to show in the flyout. Greyed out ones are not offered by this display.")
            Flow {
                width: parent.width
                spacing: 6
                Chip {
                    enabled: group.monitor.supportsContrast
                    opacity: enabled ? 1 : 0.4
                    text: qsTr("Contrast")
                    checked: group.monitor.showContrast
                    onClicked: group.monitor.showContrast = !group.monitor.showContrast
                }
                Chip {
                    enabled: group.monitor.supportsVolume
                    opacity: enabled ? 1 : 0.4
                    text: qsTr("Volume")
                    checked: group.monitor.showVolume
                    onClicked: group.monitor.showVolume = !group.monitor.showVolume
                }
                Chip {
                    enabled: group.monitor.supportsInput && group.monitor.inputs.length > 0
                    opacity: enabled ? 1 : 0.4
                    text: qsTr("Input")
                    checked: group.monitor.showInput
                    onClicked: group.monitor.showInput = !group.monitor.showInput
                }
                Chip {
                    enabled: group.monitor.supportsPower
                    opacity: enabled ? 1 : 0.4
                    text: qsTr("Power")
                    checked: group.monitor.showPower
                    onClicked: group.monitor.showPower = !group.monitor.showPower
                }
            }
        }
        Setting {
            below: true
            icon: "code_regular"
            title: qsTr("Custom VCP codes")
            description: group.featureError.length > 0 ? group.featureError
                : group.monitor.capabilities.length > 0 ? qsTr("This display reports: %1").arg(group.monitor.capabilities)
                : qsTr("This display does not report which codes it supports.")
            Column {
                width: parent.width
                spacing: 6
                Repeater {
                    model: group.monitor.features
                    Row {
                        id: entry
                        required property var modelData
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 220
                            elide: Text.ElideRight
                            text: entry.modelData.label + "  (0x" + entry.modelData.code.toString(16).toUpperCase() + ")"
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSize
                        }
                        IconButton {
                            icon: "delete_regular"
                            tooltip: qsTr("Remove %1").arg(entry.modelData.label)
                            onClicked: group.monitor.removeFeature(entry.modelData.code)
                        }
                    }
                }
                Row {
                    spacing: 8
                    Field {
                        id: code
                        width: 90
                        Accessible.name: qsTr("VCP code")
                        placeholderText: "0x16"
                    }
                    Field {
                        id: label
                        width: 180
                        Accessible.name: qsTr("Label")
                        placeholderText: qsTr("Label")
                    }
                    IconButton {
                        variant: "standard"
                        icon: "add_regular"
                        text: qsTr("Add")
                        onClicked: {
                            group.featureError = group.monitor.addFeature(code.text, label.text)
                            if (group.featureError.length > 0) return
                            code.text = ""
                            label.text = ""
                        }
                    }
                }
            }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.implicitHeight + 48
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: Scroller {}

        ColumnLayout {
            id: content
            x: 24
            y: 20
            width: flick.width - 48
            spacing: 0

            Text {
                text: qsTr("Settings")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.titleSize
                font.weight: Font.DemiBold
            }

            Heading { text: qsTr("Displays") }
            Group {
                visible: Displays.allMonitors.length === 0
                Setting {
                    divider: false
                    icon: "desktop_regular"
                    title: Displays.detecting ? qsTr("Looking for displays") : qsTr("No displays answered")
                    description: Displays.error.length > 0 ? Displays.error : qsTr("Check that DDC/CI is switched on in your monitor's own menu.")
                    IconButton {
                        variant: "standard"
                        icon: "arrow_sync_regular"
                        text: qsTr("Look again")
                        onClicked: Displays.refresh()
                    }
                }
            }
            Repeater {
                model: Displays.allMonitors
                MonitorSettings {
                    required property var modelData
                    required property int index
                    Layout.topMargin: index > 0 ? 10 : 0
                    monitor: modelData
                    position: index
                }
            }

            Heading { text: qsTr("Behaviour") }
            Group {
                Setting {
                    divider: false
                    icon: "link_regular"
                    title: qsTr("Link levels")
                    description: qsTr("Moving one brightness slider moves them all.")
                    Toggle {
                        Accessible.name: qsTr("Link levels")
                        checked: Config.linked
                        onToggled: Config.linked = checked
                    }
                }
                Setting {
                    icon: "arrow_reset_regular"
                    title: qsTr("Restore my brightness")
                    description: qsTr("At startup, when a display connects, after sleep and when HDR switches on or off.")
                    Toggle {
                        Accessible.name: qsTr("Restore my brightness")
                        checked: Config.reapply
                        onToggled: Config.reapply = checked
                    }
                }
                Setting {
                    icon: "lock_closed_regular"
                    title: qsTr("Hold check")
                    description: qsTr("How often displays with Hold brightness are checked.")
                    Dropdown {
                        width: 150
                        Accessible.name: qsTr("Hold check")
                        model: [
                            { value: 2, label: qsTr("Every 2 seconds") },
                            { value: 5, label: qsTr("Every 5 seconds") },
                            { value: 10, label: qsTr("Every 10 seconds") },
                            { value: 30, label: qsTr("Every 30 seconds") }
                        ]
                        current: Config.holdInterval
                        onActivated: Config.holdInterval = currentValue
                    }
                }
                Setting {
                    icon: "window_regular"
                    title: qsTr("Flyout corner")
                    Dropdown {
                        width: 150
                        Accessible.name: qsTr("Flyout corner")
                        model: [
                            { value: "top-left", label: qsTr("Top left") },
                            { value: "top-right", label: qsTr("Top right") },
                            { value: "bottom-left", label: qsTr("Bottom left") },
                            { value: "bottom-right", label: qsTr("Bottom right") }
                        ]
                        current: Config.flyoutCorner
                        onActivated: Config.flyoutCorner = currentValue
                    }
                }
                Setting {
                    icon: "brightness_high_regular"
                    title: qsTr("Level overlay")
                    description: qsTr("Shows the new level when a command or the tray icon changes it.")
                    Row {
                        spacing: 10
                        Segmented {
                            anchors.verticalCenter: parent.verticalCenter
                            enabled: Config.overlay
                            options: [
                                { value: "top", label: qsTr("Top") },
                                { value: "bottom", label: qsTr("Bottom") }
                            ]
                            current: Config.overlayPosition
                            onPicked: value => Config.overlayPosition = value
                        }
                        Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: qsTr("Level overlay")
                            checked: Config.overlay
                            onToggled: Config.overlay = checked
                        }
                    }
                }
                Setting {
                    icon: "arrow_sync_regular"
                    title: qsTr("Tray scroll step")
                    description: qsTr("How far one notch on the tray icon moves the brightness.")
                    Segmented {
                        options: [
                            { value: 1, label: "1" },
                            { value: 2, label: "2" },
                            { value: 5, label: "5" },
                            { value: 10, label: "10" }
                        ]
                        current: Config.scrollStep
                        onPicked: value => Config.scrollStep = value
                    }
                }
                Setting {
                    icon: "power_regular"
                    title: qsTr("Turn off displays")
                    description: qsTr("Screen off asks the desktop to blank the screens. A power signal is sent to the monitors over DDC/CI, and not every monitor wakes up from one.")
                    Dropdown {
                        width: 150
                        Accessible.name: qsTr("Turn off displays")
                        model: [
                            { value: "software", label: qsTr("Screen off") },
                            { value: "hardware", label: qsTr("Power signal") },
                            { value: "both", label: qsTr("Both") }
                        ]
                        current: Config.turnOffAction
                        onActivated: Config.turnOffAction = currentValue
                    }
                }
                Setting {
                    icon: "plug_connected_regular"
                    title: qsTr("Power signal")
                    description: qsTr("Standby is the one monitors are most likely to wake up from.")
                    Segmented {
                        options: [
                            { value: "standby", label: qsTr("Standby") },
                            { value: "off", label: qsTr("Off") }
                        ]
                        current: Config.powerSignal
                        onPicked: value => Config.powerSignal = value
                    }
                }
            }

            Heading {
                visible: Displays.compositor === "hyprland"
                text: "Hyprland"
            }
            Group {
                id: hyprland
                property bool failed: false
                visible: Displays.compositor === "hyprland"
                Setting {
                    divider: false
                    icon: "hdr_regular"
                    title: qsTr("Keep SDR brightness in the Hyprland config")
                    description: hyprland.failed ? qsTr("The Hyprland config could not be changed.")
                        : Displays.hyprConfig < 0 ? qsTr("No hyprland.lua was found, so there is nothing to add this to.")
                        : Displays.hyprConfig === 1 ? qsTr("Your hyprland.lua already loads shineshift.lua. While this is on, your SDR brightness is written there and survives reloads without a flash.")
                        : qsTr("Adds require(\"shineshift\") to the end of your hyprland.lua and writes shineshift.lua beside it. Without this, ShineShift sets the value again after every reload.")
                    Toggle {
                        Accessible.name: qsTr("Keep SDR brightness in the Hyprland config")
                        enabled: Displays.hyprConfig >= 0
                        checked: Config.persistSdr
                        onToggled: {
                            hyprland.failed = !Displays.setPersistSdr(checked)
                            checked = Qt.binding(() => Config.persistSdr)
                        }
                    }
                }
            }

            Heading { text: qsTr("Appearance") }
            Group {
                Setting {
                    divider: false
                    below: true
                    icon: "document_text_regular"
                    title: qsTr("Theme")
                    description: qsTr("Material, or a JSON theme file from %1.").arg(Appearance.themesFolder)
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: Appearance.themes
                            Chip {
                                required property string modelData
                                text: modelData === "material" ? qsTr("Material") : modelData
                                checked: Appearance.theme === modelData
                                onClicked: Appearance.theme = modelData
                            }
                        }
                        IconButton {
                            implicitHeight: 30
                            icon: "folder_open_regular"
                            tooltip: qsTr("Open themes folder")
                            onClicked: Appearance.openThemesFolder()
                        }
                    }
                }
                Setting {
                    icon: "desktop_regular"
                    title: qsTr("Mode")
                    description: Appearance.modeFixed ? qsTr("Set by the theme file.") : ""
                    Segmented {
                        enabled: !Appearance.modeFixed
                        options: [
                            { value: "system", label: qsTr("System") },
                            { value: "light", label: qsTr("Light") },
                            { value: "dark", label: qsTr("Dark") }
                        ]
                        current: Appearance.mode
                        onPicked: value => Appearance.mode = value
                    }
                }
                Setting {
                    below: true
                    icon: "paint_brush_regular"
                    title: qsTr("Accent")
                    description: {
                        const list = Appearance.accents
                        for (let index = 0; index < list.length; ++index)
                            if (list[index].id === Appearance.accent) return list[index].label
                        return ""
                    }
                    Column {
                        width: parent.width
                        spacing: 10
                        Flow {
                            width: parent.width
                            spacing: 3
                            Repeater {
                                model: Appearance.accents
                                Item {
                                    id: swatch
                                    required property var modelData
                                    readonly property bool chosen: Appearance.accent === modelData.id
                                    width: 26
                                    height: 26
                                    Accessible.role: Accessible.RadioButton
                                    Accessible.name: modelData.label
                                    Accessible.checked: chosen

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 13
                                        color: "transparent"
                                        border.width: 2
                                        border.color: swatch.chosen ? Theme.text : swatchMouse.containsMouse ? Theme.outline : "transparent"
                                    }
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        radius: 9
                                        color: swatch.modelData.color
                                        border.width: 1
                                        border.color: Qt.alpha(Theme.text, 0.25)
                                    }
                                    // Custom shows a hue ring around its colour so it reads as "pick your own".
                                    Rectangle {
                                        visible: swatch.modelData.id === "custom"
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        radius: 9
                                        rotation: 45
                                        gradient: Gradient {
                                            GradientStop { position: 0; color: "#ff5f5f" }
                                            GradientStop { position: 0.35; color: "#ffd54f" }
                                            GradientStop { position: 0.65; color: "#4dd0c4" }
                                            GradientStop { position: 1; color: "#b39ddb" }
                                        }
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: swatch.modelData.color
                                        }
                                    }
                                    MouseArea {
                                        id: swatchMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: Appearance.accent = swatch.modelData.id
                                    }
                                    Tip {
                                        visible: swatchMouse.containsMouse
                                        text: swatch.modelData.label
                                    }
                                }
                            }
                        }
                        ColorPicker {
                            visible: Appearance.accent === "custom"
                            width: Math.min(parent.width, 360)
                            height: visible ? implicitHeight : 0
                            color: Appearance.customAccent
                            onEdited: value => Appearance.customAccent = value
                        }
                    }
                }
                Setting {
                    icon: "image_regular"
                    title: qsTr("Use desktop icon theme")
                    description: qsTr("Falls back to the built-in icons where your icon theme has no match.")
                    Toggle { checked: Config.desktopIcons; onToggled: Config.desktopIcons = checked }
                }
            }

            Heading { text: qsTr("About") }
            Group {
                Setting {
                    divider: false
                    icon: "keyboard_regular"
                    title: "ShineShift " + Displays.version
                    description: qsTr("Bind keys to commands like \"shineshift offset all -10\" and \"shineshift offset all +10\" in your desktop settings. \"shineshift --help\" lists them all.")
                }
            }
        }
    }
}
