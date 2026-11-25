import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import TechCard 1.0

Item {
    id: page

    property bool withPhone: true

    // What can be shared. Each option only appears when the CV has that link.
    readonly property var options: {
        var cv = store.cv
        var list = []
        if (store.cardUrl)
            list.push({ key: "cv", label: qsTr("CV"), url: store.cardUrl })
        if (cv.name)
            list.push({ key: "contact", label: qsTr("Contact"), url: "" })
        if (cv.linkedin && cv.linkedin !== store.cardUrl)
            list.push({ key: "linkedin", label: "LinkedIn", url: cv.linkedin })
        var gh = cv.github || (store.githubUser ? "https://github.com/" + store.githubUser : "")
        if (gh)
            list.push({ key: "github", label: "GitHub", url: gh })
        return list
    }
    readonly property var current: options.length > 0
        ? options[Math.min(modeBar.currentIndex, options.length - 1)] : null
    readonly property bool isContact: current !== null && current.key === "contact"
    // store.cv is read here so the vCard is rebuilt whenever the CV changes
    readonly property string payload: current === null ? ""
        : (isContact ? (store.cv, store.vcard(withPhone)) : current.url)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        TabBar {
            id: modeBar
            // The bar takes its width from the layout and does not ask for one:
            // otherwise "tab width = bar width / count" makes a polish loop (white screen)
            implicitWidth: 0
            Layout.fillWidth: true
            Repeater {
                model: page.options
                TabButton {
                    text: modelData.label
                    width: modeBar.width / Math.max(1, modeBar.count)
                }
            }
        }

        // White card behind the QR so it scans well in dark themes too
        Rectangle {
            id: qrCard
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(page.width - 32, page.height * 0.55, 420)
            Layout.preferredHeight: Layout.preferredWidth
            color: "white"
            radius: 12

            QrCode {
                anchors.fill: parent
                anchors.margins: 8
                text: page.payload
            }
            Label {
                anchors.centerIn: parent
                visible: page.payload === ""
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Loading…")
                color: "#555555"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: fullScreen.open()
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: page.current === null ? ""
                  : page.isContact ? qsTr("Scan with the camera to save my contact")
                  : page.current.url
        }

        CheckBox {
            Layout.alignment: Qt.AlignHCenter
            visible: page.isContact
            text: qsTr("Include phone number")
            checked: page.withPhone
            onToggled: page.withPhone = checked
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Full screen")
            enabled: page.payload !== ""
            onClicked: fullScreen.open()
        }

        Item { Layout.fillHeight: true }
    }

    // Full-screen QR for showing the phone to someone across a table
    Popup {
        id: fullScreen
        parent: Overlay.overlay
        x: 0
        y: 0
        width: parent ? parent.width : page.width
        height: parent ? parent.height : page.height
        modal: true
        padding: 0
        background: Rectangle { color: "white" }

        QrCode {
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height) * 0.92
            height: width
            text: page.payload
        }
        Label {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Tap to close")
            color: "#555555"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: fullScreen.close()
        }
    }
}
