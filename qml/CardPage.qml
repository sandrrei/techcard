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

    property var cv: store.cv

    ColumnLayout {
        id: column
        width: page.width
        spacing: 12

        Item { Layout.preferredHeight: 8 }

        Label {
            visible: store.demo
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            wrapMode: Text.Wrap
            opacity: 0.7
            text: qsTr("Demo profile. Tap Edit to set up your own card.")
        }

        // Avatar + name / title / location
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 16

            Item {
                Layout.preferredWidth: 88
                Layout.preferredHeight: 88

                // Initial as fallback while the GitHub avatar is not available
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "#37474F"
                    visible: avatar.status !== Image.Ready
                    Label {
                        anchors.centerIn: parent
                        text: (cv.name || "?").charAt(0)
                        font.pixelSize: 36
                        color: "white"
                    }
                }
                Image {
                    id: avatar
                    anchors.fill: parent
                    source: store.avatarUrl
                    sourceSize.width: 176
                    sourceSize.height: 176
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    text: cv.name || qsTr("Loading…")
                    font.pixelSize: 26
                    font.bold: true
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Label {
                    text: cv.title || ""
                    font.pixelSize: 16
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Label {
                    text: cv.location || ""
                    opacity: 0.7
                    Layout.fillWidth: true
                }
            }
        }

        Label {
            visible: !!cv.status
            text: cv.status || ""
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 8
            Button {
                visible: !!cv.email
                text: qsTr("Email")
                onClicked: Qt.openUrlExternally("mailto:" + cv.email)
            }
            Button {
                visible: !!cv.phone
                text: qsTr("Phone")
                onClicked: Qt.openUrlExternally("tel:" + cv.phone.replace(/[^+\d]/g, ""))
            }
            Button {
                visible: !!cv.linkedin
                text: "LinkedIn"
                onClicked: Qt.openUrlExternally(cv.linkedin)
            }
            Button {
                visible: !!cv.github
                text: "GitHub"
                onClicked: Qt.openUrlExternally(cv.github)
            }
        }

        Label {
            visible: !!cv.bio
            text: cv.bio || ""
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
        }

        // Skills: { category: [ "skill", ... ] } shown as chips
        Repeater {
            model: cv.skills ? Object.keys(cv.skills) : []
            delegate: ColumnLayout {
                property string category: modelData
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 6

                Label {
                    // "cloudAndDevOps" -> "Cloud And Dev Ops"; names with spaces stay as they are
                    text: /\s/.test(category)
                          ? category
                          : category.replace(/([A-Z])/g, " $1")
                                    .replace(/^./, function(c) { return c.toUpperCase() })
                    font.bold: true
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: cv.skills[category] || []
                        delegate: Rectangle {
                            width: chip.implicitWidth + 16
                            height: chip.implicitHeight + 8
                            radius: height / 2
                            color: "transparent"
                            border.color: "#7C4DFF"
                            Label {
                                id: chip
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
