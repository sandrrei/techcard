import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

ListView {
    id: list
    clip: true
    spacing: 4
    model: store.cv.experience || []
    ScrollBar.vertical: ScrollBar {}

    delegate: ItemDelegate {
        width: list.width
        contentItem: ColumnLayout {
            spacing: 2
            Label {
                text: modelData.role
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: modelData.company + "  ·  " + modelData.period
                opacity: 0.7
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: (modelData.highlights || []).map(function(h) { return "• " + h }).join("\n")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.topMargin: 4
            }
        }
    }
}
