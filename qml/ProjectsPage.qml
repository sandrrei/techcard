import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

ListView {
    id: list
    clip: true
    ScrollBar.vertical: ScrollBar {}

    // Highlighted projects from the CV first, then public GitHub repos.
    // A ListModel (not a JS array) is needed for section headers.
    model: ListModel { id: projectModel }

    function rebuild() {
        projectModel.clear()
        var featured = store.cv.projects || []
        for (var i = 0; i < featured.length; i++)
            projectModel.append({ section: qsTr("Highlighted"), name: featured[i].name || "",
                                  description: featured[i].description || "",
                                  meta: featured[i].tech || "", url: featured[i].url || "" })
        var repos = store.repos
        for (var j = 0; j < repos.length; j++) {
            var r = repos[j]
            var meta = [r.language, "\u2605 " + r.stars, r.pushed].filter(function(x) { return !!x })
            projectModel.append({ section: "GitHub", name: r.name, description: r.description || "",
                                  meta: meta.join("  \u00b7  "), url: r.url })
        }
    }

    Component.onCompleted: rebuild()
    Connections {
        target: store
        onCvChanged: list.rebuild()
        onReposChanged: list.rebuild()
    }

    section.property: "section"
    section.delegate: Label {
        text: section
        font.bold: true
        font.pixelSize: 15
        padding: 12
        opacity: 0.6
    }

    delegate: ItemDelegate {
        width: list.width
        enabled: model.url !== ""
        onClicked: Qt.openUrlExternally(model.url)
        contentItem: ColumnLayout {
            spacing: 2
            Label {
                text: model.name
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                visible: !!model.description
                text: model.description || ""
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                visible: !!model.meta
                text: model.meta || ""
                opacity: 0.6
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
        }
    }
}
