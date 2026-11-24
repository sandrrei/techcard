import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

ApplicationWindow {
    id: root
    visible: true
    width: 420
    height: 800
    title: "TechCard"

    property bool settingsOpen: false
    readonly property bool showSetup: !store.configured || settingsOpen

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            Label {
                text: root.showSetup ? qsTr("Set up your card")
                                     : (tabs.currentItem ? tabs.currentItem.text : "TechCard")
                font.bold: true
                font.pixelSize: 18
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            BusyIndicator {
                running: store.loading
                visible: running
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32
            }
            ToolButton {
                visible: !root.showSetup
                // PNG icon: the Android font has no "refresh" glyph (it showed as a box)
                icon.source: "qrc:/icons/refresh.png"
                icon.width: 24
                icon.height: 24
                enabled: !store.loading
                onClicked: store.refresh()
            }
            ToolButton {
                visible: !root.showSetup
                text: qsTr("Edit")
                onClicked: root.settingsOpen = true
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        visible: !root.showSetup
        currentIndex: tabs.currentIndex

        CardPage {}
        ExperiencePage {}
        ProjectsPage {}
        SharePage {}
    }

    SetupPage {
        anchors.fill: parent
        visible: root.showSetup
        canCancel: store.configured
        onDone: root.settingsOpen = false
    }

    footer: ColumnLayout {
        spacing: 0
        visible: !root.showSetup
        Label {
            Layout.fillWidth: true
            visible: store.error !== ""
            text: store.error + (store.lastUpdated ? "  (offline: " + store.lastUpdated + ")" : "")
            wrapMode: Text.Wrap
            color: "white"
            padding: 6
            background: Rectangle { color: "#b3261e" }
        }
        TabBar {
            id: tabs
            // The bar takes its width from the layout and does not ask for one:
            // otherwise "tab width = bar width / count" makes a polish loop (white screen)
            implicitWidth: 0
            Layout.fillWidth: true
            TabButton { text: qsTr("Card"); width: tabs.width / tabs.count }
            TabButton { text: qsTr("Experience"); width: tabs.width / tabs.count }
            TabButton { text: qsTr("Projects"); width: tabs.width / tabs.count }
            TabButton { text: qsTr("Share"); width: tabs.width / tabs.count }
        }
    }
}
