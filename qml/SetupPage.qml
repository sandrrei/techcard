import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// Flickable instead of ScrollView: with Qt 5.12 (Ubuntu Touch 20.04) the
// ScrollView did not compute its content height, so the page did not scroll.
Flickable {
    id: page
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    property bool canCancel: false
    property string message: ""
    // The file picker (QtQuick.Dialogs) is only used on Android; Ubuntu Touch
    // apps are confined, so there the CV comes from a link or pasted JSON.
    readonly property bool canPickFile: Qt.platform.os === "android"

    signal done()

    function loadCurrent() {
        urlField.text = store.demo ? "" : store.sourceUrl
        ghField.text = store.demo ? "" : store.githubUser
        pageField.text = store.demo ? "" : store.pageUrl
        sourceBar.currentIndex = store.sourceMode === "local" ? 1 : 0
        message = ""
    }

    function finish(err) {
        if (err === "") {
            message = ""
            jsonArea.text = ""
            done()
        } else {
            message = err
        }
    }

    function save() {
        if (sourceBar.currentIndex === 0)
            finish(store.setSourceUrl(urlField.text, ghField.text, pageField.text))
        else
            finish(store.importJson(jsonArea.text, ghField.text, pageField.text))
    }

    function openPicker() {
        if (picker.item)
            picker.item.dialog.open()
        else
            picker.active = true
    }

    onVisibleChanged: if (visible) loadCurrent()
    Component.onCompleted: loadCurrent()

    Loader {
        id: picker
        active: false
        source: "FilePicker.qml"
        onLoaded: {
            item.dialog.accepted.connect(function() {
                page.finish(store.importFile(item.dialog.fileUrl, ghField.text, pageField.text))
            })
            item.dialog.open()
        }
    }

    ColumnLayout {
        id: column
        width: page.width
        spacing: 10

        Item { Layout.preferredHeight: 4 }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            wrapMode: Text.Wrap
            text: qsTr("Load your CV from a link to a JSON file, or paste or import the JSON. "
                       + "The JSON Resume format (jsonresume.org) is supported.")
        }

        TabBar {
            id: sourceBar
            // The bar takes its width from the layout and does not ask for one:
            // otherwise "tab width = bar width / count" makes a polish loop (white screen)
            implicitWidth: 0
            Layout.fillWidth: true
            TabButton { text: qsTr("Link"); width: sourceBar.width / sourceBar.count }
            TabButton { text: qsTr("Paste"); width: sourceBar.width / sourceBar.count }
            TabButton { text: qsTr("File"); width: sourceBar.width / sourceBar.count }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            currentIndex: sourceBar.currentIndex
            // Only as tall as the visible tab (otherwise the Paste tab leaves a gap)
            Layout.preferredHeight: [linkTab, pasteTab, fileTab][currentIndex].implicitHeight

            ColumnLayout {
                id: linkTab
                Label { text: qsTr("Link to your CV (JSON)") }
                TextField {
                    id: urlField
                    Layout.fillWidth: true
                    placeholderText: "https://example.com/resume.json"
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                    selectByMouse: true
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    font.pixelSize: 12
                    text: qsTr("Tip: a GitHub Gist works. Use its \"Raw\" link.")
                }
            }

            ColumnLayout {
                id: pasteTab
                Label { text: qsTr("Paste your CV (JSON)") }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    TextArea {
                        id: jsonArea
                        wrapMode: TextArea.WrapAnywhere
                        selectByMouse: true
                        font.family: "monospace"
                        placeholderText: "{ \"basics\": { \"name\": \"…\" } }"
                    }
                }
            }

            ColumnLayout {
                id: fileTab
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: page.canPickFile
                          ? qsTr("Choose a .json file on this phone.")
                          : qsTr("File import is only available on Android. Use a link or paste the JSON instead.")
                }
                Button {
                    visible: page.canPickFile
                    text: qsTr("Choose file…")
                    onClicked: page.openPicker()
                }
            }
        }

        Label {
            Layout.leftMargin: 16
            text: qsTr("GitHub user (optional)")
        }
        TextField {
            id: ghField
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            placeholderText: qsTr("detected from the CV if empty")
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            selectByMouse: true
        }

        Label {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Public CV page for the QR code (optional)")
        }
        TextField {
            id: pageField
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            placeholderText: qsTr("your website or LinkedIn if empty")
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            selectByMouse: true
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            visible: page.message !== ""
            text: page.message
            wrapMode: Text.Wrap
            color: "#d32f2f"
        }

        RowLayout {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 8
            Button {
                text: qsTr("Save")
                highlighted: true
                enabled: sourceBar.currentIndex !== 2
                onClicked: page.save()
            }
            Button {
                visible: page.canCancel
                text: qsTr("Cancel")
                onClicked: page.done()
            }
        }

        Label {
            Layout.leftMargin: 16
            Layout.topMargin: 8
            opacity: 0.6
            text: qsTr("or")
        }
        Button {
            Layout.leftMargin: 16
            flat: true
            text: qsTr("Try the demo profile (fictional)")
            onClicked: {
                store.useDemo()
                page.finish("")
            }
        }
        Button {
            Layout.leftMargin: 16
            flat: true
            visible: store.configured
            text: qsTr("Remove my CV from this device")
            onClicked: store.resetConfig()
        }

        // Appearance (Android). Ubuntu Touch follows the system theme by itself.
        ColumnLayout {
            visible: store.isAndroid
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 16
            spacing: 4

            Label {
                text: qsTr("Appearance")
                font.bold: true
            }
            ComboBox {
                id: themeBox
                readonly property var values: ["system", "light", "dark"]
                Layout.fillWidth: true
                model: [qsTr("Follow system"), qsTr("Light"), qsTr("Dark")]
                currentIndex: Math.max(0, values.indexOf(store.theme))
                onActivated: {
                    store.theme = values[index]
                    themeNote.visible = true
                }
            }
            Label {
                id: themeNote
                visible: false
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                text: qsTr("Close and reopen TechCard to apply the new theme.")
            }
        }

        // Extra space so the last fields can be scrolled above the on-screen keyboard
        Item { Layout.preferredHeight: Qt.inputMethod.visible ? page.height / 2 : 16 }
    }
}
