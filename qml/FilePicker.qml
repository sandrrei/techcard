import QtQuick 2.12
import QtQuick.Dialogs 1.3

// Loaded on demand (Android only), so Ubuntu Touch never needs QtQuick.Dialogs.
// No name filters: downloaded .json files often have a generic MIME type.
Item {
    property alias dialog: dlg

    FileDialog {
        id: dlg
        title: qsTr("Choose a JSON CV")
        selectExisting: true
        selectMultiple: false
    }
}
