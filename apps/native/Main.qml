import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1120; height: 790
    minimumWidth: 360; minimumHeight: 600
    visible: true
    title: "Quicky"
    color: bg
    readonly property bool compact: width < 780
    readonly property color bg: quicky.darkMode ? "#101218" : "#F5F6FB"
    readonly property color surface: quicky.darkMode ? "#1A1D27" : "#FFFFFF"
    readonly property color ink: quicky.darkMode ? "#F3F4FA" : "#171B2B"
    readonly property color muted: quicky.darkMode ? "#A3ABC0" : "#646C83"
    readonly property color line: quicky.darkMode ? "#2C3142" : "#E5E8F2"
    readonly property color accent: quicky.darkMode ? "#ADA7FF" : "#5144D9"
    property string page: "share"
    property var chosenFiles: []
    property string pendingName: ""
    property string pendingSender: ""
    property string pendingSize: ""
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "sans-serif"
    font.pixelSize: 14
    palette.text: ink
    palette.windowText: ink
    palette.buttonText: ink
    palette.button: surface
    palette.base: surface
    palette.highlight: accent
    palette.highlightedText: bg

    component Caption: Label { color: window.muted; wrapMode: Text.WordWrap }
    component Action: Button {
        id: control
        property bool primary: false
        font.weight: Font.DemiBold
        padding: 14
        contentItem: Text {
            text: control.text; font: control.font
            color: control.enabled ? (control.primary ? "#17132F" : window.ink) : window.muted
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 12
            color: control.primary ? "#B5ADFF" : (control.hovered ? window.line : window.surface)
            border.color: control.primary ? "#B5ADFF" : window.line
            opacity: control.enabled ? 1 : 0.45
            Behavior on color { ColorAnimation { duration: 140 } }
        }
    }
    component Card: Rectangle {
        color: window.surface; radius: 22
        border.color: window.line
    }
    component Input: TextField {
        color: window.ink; selectByMouse: true
        placeholderTextColor: window.muted
        padding: 14
        background: Rectangle { color: window.bg; radius: 12; border.color: parent.activeFocus ? window.accent : window.line }
    }

    FileDialog {
        id: picker
        title: "Dateien zum Senden auswählen"
        fileMode: FileDialog.OpenFiles
        onAccepted: window.chosenFiles = selectedFiles
    }
    FolderDialog {
        id: folderPicker
        title: "Empfangsordner auswählen"
        onAccepted: quicky.receiveFolder = selectedFolder.toString()
    }
    Connections {
        target: ui
        function onInvitationScanned(invitation) { invitationInput.text = invitation }
    }
    Connections {
        target: quicky
        function onIncomingOffer(name, sender, size) {
            window.pendingName = name; window.pendingSender = sender; window.pendingSize = size
            incoming.open()
        }
        function onChanged() {
            if (quicky.state !== "offered") incoming.close()
        }
    }
    Dialog {
        id: incoming
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 420)
        modal: true; closePolicy: Popup.NoAutoClose
        title: "Datei empfangen?"
        background: Rectangle { color: window.surface; radius: 20; border.color: window.line }
        contentItem: ColumnLayout {
            spacing: 18
            Label { text: window.pendingName; color: window.ink; font.pixelSize: 21; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            Caption { text: window.pendingSize + " · von „" + window.pendingSender + "“"; Layout.fillWidth: true }
            Caption { text: "Gerätenamen sind nicht verifiziert. Nimm die Datei nur an, wenn du den Sender erwartest."; Layout.fillWidth: true }
            CheckBox { id: rememberSender; text: "Dieses Gerät dauerhaft koppeln"; checked: false }
            RowLayout {
                Action { text: "Ablehnen"; onClicked: quicky.acceptIncoming(false); Layout.fillWidth: true }
                Action { text: "Annehmen"; primary: true; onClicked: quicky.acceptIncoming(true, rememberSender.checked); Layout.fillWidth: true }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 88; color: window.bg
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: window.compact ? 20 : 36; anchors.rightMargin: 24
                spacing: 12
                Rectangle {
                    width: 38; height: 38; radius: 12; color: "#B5ADFF"
                    Text { anchors.centerIn: parent; text: "Q"; font.pixelSize: 26; font.weight: Font.Bold; color: "#17132F" }
                }
                Label { text: "quicky"; font.pixelSize: 28; font.weight: Font.DemiBold; color: window.ink }
                Item { Layout.fillWidth: true }
                Rectangle { width: 8; height: 8; radius: 4; color: quicky.listening ? "#66D9AC" : "#FF9B9B" }
                Caption { text: quicky.listening ? "Lokal & verschlüsselt" : "Empfang nicht verfügbar"; visible: !window.compact }
                Action { text: quicky.darkMode ? "Hell" : "Dunkel"; onClicked: quicky.darkMode = !quicky.darkMode }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.leftMargin: window.compact ? 16 : 36; Layout.rightMargin: 16; spacing: 10
            Repeater {
                model: [{key:"share",label:"Teilen"}, {key:"history",label:"Verlauf"}, {key:"settings",label:"Einstellungen"}]
                delegate: Action { required property var modelData; text: modelData.label; primary: window.page === modelData.key; onClicked: window.page = modelData.key }
            }
            Item { Layout.fillWidth: true }
        }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: 22
                Item { Layout.preferredHeight: 2 }
                ColumnLayout {
                    visible: window.page === "share"
                    Layout.fillWidth: true; Layout.margins: window.compact ? 16 : 36; spacing: 22
                    Label { text: "Einfach rüber."; font.pixelSize: window.compact ? 32 : 44; font.weight: Font.DemiBold; color: window.ink }
                    Caption { text: "Deine Dateien. Deine Geräte. Direkt in deinem WLAN."; font.pixelSize: 16; Layout.fillWidth: true }
                    Card {
                        Layout.fillWidth: true; implicitHeight: transferColumn.implicitHeight + 40
                        ColumnLayout {
                            id: transferColumn
                            anchors.fill: parent; anchors.margins: 20; spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                BusyIndicator { running: quicky.busy && quicky.state !== "paused" && quicky.state !== "offered"; visible: running; implicitWidth: 32; implicitHeight: 32 }
                                Label { text: quicky.status; color: window.ink; font.pixelSize: 22; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                Label { text: quicky.state === "completed" ? "✓" : ""; color: "#66D9AC"; font.pixelSize: 28 }
                            }
                            Caption { text: quicky.detail || "Wähle Dateien und verbinde dich über eine sichere Einladung."; Layout.fillWidth: true }
                            Action {
                                visible: ui.lastReceived.length > 0 && quicky.state === "completed"
                                text: Qt.platform.os === "android" ? "Empfangene Datei speichern …" : "Empfangene Datei öffnen"
                                onClicked: ui.exportReceived()
                            }
                            ProgressBar {
                                visible: quicky.busy || quicky.state === "completed"
                                Layout.fillWidth: true; value: quicky.progress
                                background: Rectangle { implicitHeight: 7; radius: 4; color: window.line }
                                contentItem: Item { implicitHeight: 7; Rectangle { width: parent.width * parent.parent.visualPosition; height: parent.height; radius: 4; color: window.accent } }
                            }
                            RowLayout {
                                visible: quicky.busy
                                Action { visible: quicky.state === "sending" || quicky.state === "connecting"; text: "Pause"; onClicked: quicky.pause() }
                                Action { visible: quicky.state === "paused"; text: "Fortsetzen"; primary: true; onClicked: quicky.resume() }
                                Action { text: "Abbrechen"; onClicked: quicky.cancel() }
                            }
                        }
                    }
                    Card {
                        visible: quicky.pairedDevices.length > 0
                        Layout.fillWidth: true; implicitHeight: pairedColumn.implicitHeight + 40
                        ColumnLayout {
                            id: pairedColumn; anchors.fill: parent; anchors.margins: 20; spacing: 12
                            Label { text: "Deine gekoppelten Geräte"; color: window.ink; font.pixelSize: 21; font.weight: Font.DemiBold }
                            Repeater {
                                model: quicky.pairedDevices
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Label { text: modelData.name; color: window.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Action { text: "Senden"; primary: true; enabled: !quicky.busy && window.chosenFiles.length > 0; onClicked: quicky.sendToDevice(modelData.fingerprint, window.chosenFiles) }
                                }
                            }
                            Caption { text: "Dateien unten auswählen und hier direkt senden. Die Geräteidentität wird bei jeder Verbindung geprüft."; Layout.fillWidth: true }
                        }
                    }
                    GridLayout {
                        columns: window.compact ? 1 : 2
                        Layout.fillWidth: true; columnSpacing: 22; rowSpacing: 22
                        Card {
                            Layout.fillWidth: true; Layout.preferredWidth: 520; Layout.alignment: Qt.AlignTop
                            implicitHeight: sendColumn.implicitHeight + 40
                            ColumnLayout {
                                id: sendColumn
                                anchors.fill: parent; anchors.margins: 20; spacing: 16
                                Label { text: "Etwas senden"; color: window.ink; font.pixelSize: 23; font.weight: Font.DemiBold }
                                Rectangle {
                                    Layout.fillWidth: true; implicitHeight: 125; color: window.bg; radius: 16; border.color: window.line
                                    ColumnLayout {
                                        anchors.centerIn: parent; width: parent.width - 24
                                        Label { text: window.chosenFiles.length ? window.chosenFiles.length + " Datei(en) ausgewählt" : "Dateien hier ablegen"; color: window.ink; font.pixelSize: 17; Layout.alignment: Qt.AlignHCenter }
                                        Action { text: "Dateien auswählen"; enabled: !quicky.busy; onClicked: picker.open(); Layout.alignment: Qt.AlignHCenter }
                                    }
                                    DropArea { anchors.fill: parent; onDropped: function(drop) { if (drop.hasUrls && !quicky.busy) window.chosenFiles = drop.urls } }
                                }
                                Caption { text: "1. Öffne Quicky auf dem Empfänger.\n2. Kopiere dort die QR-Einladung.\n3. Füge den Link hier ein und sende."; Layout.fillWidth: true }
                                Input { id: invitationInput; Layout.fillWidth: true; placeholderText: "quicky://pair?host=…"; Accessible.name: "Einladung des Empfängers" }
                                Action { visible: Qt.platform.os === "android"; text: "QR-Einladung scannen"; enabled: !quicky.busy; onClicked: ui.scanInvitation(); Layout.fillWidth: true }
                                RowLayout {
                                    Action { text: "Einfügen"; onClicked: invitationInput.text = ui.paste(); Layout.fillWidth: true }
                                    Action { text: "Dateien senden"; primary: true; enabled: !quicky.busy && window.chosenFiles.length > 0 && invitationInput.text.length > 0; onClicked: quicky.sendFiles(window.chosenFiles, invitationInput.text); Layout.fillWidth: true }
                                }
                                TextArea {
                                    id: textPayload; Layout.fillWidth: true; implicitHeight: 80; color: window.ink; placeholderTextColor: window.muted
                                    placeholderText: "Oder Text / Link teilen …"; wrapMode: TextEdit.Wrap; selectByMouse: true
                                    background: Rectangle { color: window.bg; radius: 12; border.color: window.line }
                                }
                                Action { text: "Text als Datei senden"; enabled: !quicky.busy && textPayload.text.length > 0 && invitationInput.text.length > 0; onClicked: quicky.sendText(textPayload.text, invitationInput.text); Layout.fillWidth: true }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.preferredWidth: 380; Layout.alignment: Qt.AlignTop; spacing: 22
                            Card {
                                Layout.fillWidth: true; implicitHeight: pairColumn.implicitHeight + 40
                                ColumnLayout {
                                    id: pairColumn; anchors.fill: parent; anchors.margins: 20; spacing: 12
                                    Label { text: "Mit QR-Code verbinden"; color: window.ink; font.pixelSize: 21; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    Caption { text: "Einladung für „" + quicky.deviceName + "“ · 5 Minuten gültig"; Layout.fillWidth: true }
                                    Image {
                                        Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 200; Layout.preferredHeight: 200
                                        source: quicky.invitation ? "image://qr/" + encodeURIComponent(quicky.invitation) : ""
                                        smooth: false; cache: false
                                        visible: quicky.invitation.length > 0
                                        Accessible.name: "QR-Code mit Geräteidentität und temporärer Einladung"
                                    }
                                    Caption { visible: !quicky.invitation; text: "Einladung abgelaufen oder Sichtbarkeit ausgeschaltet."; Layout.fillWidth: true }
                                    RowLayout {
                                        Action { text: "Link kopieren"; enabled: quicky.invitation.length > 0; onClicked: ui.copy(quicky.invitation); Layout.fillWidth: true }
                                        Action { text: "Erneuern"; onClicked: quicky.renewInvitation(); Layout.fillWidth: true }
                                    }
                                    Caption { text: "Der QR-Link enthält einen Zugangsschlüssel. Nur mit dem gewünschten Sender teilen. Empfang wird zusätzlich bestätigt."; font.pixelSize: 12; Layout.fillWidth: true }
                                }
                            }
                            Card {
                                Layout.fillWidth: true; implicitHeight: deviceColumn.implicitHeight + 40
                                ColumnLayout {
                                    id: deviceColumn; anchors.fill: parent; anchors.margins: 20; spacing: 14
                                    Label { text: "In deiner Nähe"; color: window.ink; font.pixelSize: 21; font.weight: Font.DemiBold }
                                    Caption { text: "Suche Geräte im lokalen Netzwerk …"; visible: quicky.devices.length === 0; Layout.fillWidth: true }
                                    Repeater {
                                        model: quicky.devices
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Rectangle { width: 38; height: 38; radius: 12; color: window.line; Label { anchors.centerIn: parent; text: modelData.platform === "android" ? "▯" : "▣"; color: window.accent; font.pixelSize: 23 } }
                                            ColumnLayout {
                                                Label { text: modelData.name; color: window.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                                Caption { text: modelData.platform + " · " + modelData.host; font.pixelSize: 12 }
                                            }
                                        }
                                    }
                                    Caption { text: "Gefundene Geräte sind noch nicht authentifiziert. Nutze die Einladung des Empfängers."; font.pixelSize: 12; Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                }
                ColumnLayout {
                    visible: window.page === "history"; Layout.fillWidth: true; Layout.margins: window.compact ? 16 : 36; spacing: 20
                    Label { text: "Deine Übertragungen"; color: window.ink; font.pixelSize: 30; font.weight: Font.DemiBold }
                    Caption { text: "Nur erfolgreich abgeschlossene Transfers. Lokal auf diesem Gerät gespeichert."; Layout.fillWidth: true }
                    Caption { text: "Noch keine Übertragungen."; visible: quicky.history.length === 0 }
                    Repeater {
                        model: quicky.history
                        delegate: Card {
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: 82
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 16
                                Label { text: modelData.name; color: window.ink; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                Caption { text: modelData.direction + " · " + modelData.size + " · " + modelData.time; Layout.fillWidth: true }
                            }
                        }
                    }
                    Action { text: "Verlauf löschen"; onClicked: quicky.clearHistory() }
                }
                ColumnLayout {
                    visible: window.page === "settings"; Layout.fillWidth: true; Layout.margins: window.compact ? 16 : 36; spacing: 20
                    Label { text: "Dein Quicky"; color: window.ink; font.pixelSize: 30; font.weight: Font.DemiBold }
                    Caption { text: "Gerätename" }
                    Input { text: quicky.deviceName; Layout.fillWidth: true; maximumLength: 60; onEditingFinished: quicky.deviceName = text }
                    Switch { text: "Im lokalen Netzwerk sichtbar"; checked: quicky.visible; onToggled: quicky.visible = checked }
                    Switch { text: "Nur gekoppelte Geräte zulassen"; checked: quicky.onlyTrusted; onToggled: quicky.onlyTrusted = checked }
                    Caption { text: "Für eine neue Kopplung: diese Einschränkung ausschalten, eine Einladung teilen und beim Empfang ›dauerhaft koppeln‹ auswählen. Jeder Transfer benötigt weiterhin deine Bestätigung."; Layout.fillWidth: true }
                    Repeater {
                        model: quicky.pairedDevices
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label { text: modelData.name; color: window.ink; Layout.fillWidth: true; elide: Text.ElideRight }
                            Action { text: "Entkoppeln"; onClicked: quicky.forgetDevice(modelData.fingerprint) }
                        }
                    }
                    Caption { text: Qt.platform.os === "android" ? "Empfang im privaten App-Speicher. Nach dem Transfer mit ›Speichern‹ an einen gewünschten Ort exportieren. Quicky während des Transfers geöffnet lassen." : "Empfangsordner: " + quicky.receiveFolder; Layout.fillWidth: true }
                    RowLayout {
                        Action { visible: Qt.platform.os !== "android"; text: "Ordner wählen"; enabled: !quicky.busy; onClicked: folderPicker.open() }
                        Action { visible: Qt.platform.os !== "android"; text: "Ordner öffnen"; onClicked: quicky.openReceiveFolder() }
                    }
                    Caption { text: "Geräteidentität (SHA-256)" }
                    Label { text: quicky.fingerprint; color: window.muted; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.family: "monospace" }
                    Caption { text: "Quicky 0.1.0 · Lokale Vorschauversion\nTLS 1.3 · Keine Cloud-Zwischenspeicherung\nBeide Geräte müssen einander im Netzwerk erreichen können."; Layout.fillWidth: true }
                }
                Item { Layout.preferredHeight: 20 }
            }
        }
    }
}
