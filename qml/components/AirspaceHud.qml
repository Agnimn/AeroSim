import QtQuick
import QtQuick.Controls

Item {
    id: hudRoot

    property int aircraftCount: 0
    property int threatCount: 0
    property string selectedName: "NONE"
    property string selectedAltitude: "--"
    property string selectedSpeed: "--"
    property string selectedHeading: "--"
    property string selectedStatus: "--"

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 18
        color: "#eaf5ff"
        text: "AIRSPACE MONITOR"
        font.pixelSize: 20
        font.bold: true
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        width: 170
        height: 54
        radius: 10
        color: "#0d1b26"
        border.color: "#4ec6ff"

        Column {
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: "LIVE"
                color: "#86d7ff"
                font.pixelSize: 11
                font.bold: true
            }

            Text {
                text: "AIRCRAFT: " + hudRoot.aircraftCount
                color: "#eff9ff"
                font.pixelSize: 13
            }

            Text {
                text: "THREATS: " + hudRoot.threatCount
                color: "#ff8c6b"
                font.pixelSize: 13
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 18
        width: 320
        height: 140
        radius: 12
        color: "#081822"
        border.color: "#31647d"
        opacity: 0.9

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Text {
                text: hudRoot.selectedName
                color: "#f0fbff"
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                text: "ALT " + hudRoot.selectedAltitude
                color: "#9fe3ff"
                font.pixelSize: 14
            }

            Text {
                text: "SPD " + hudRoot.selectedSpeed
                color: "#9fe3ff"
                font.pixelSize: 14
            }

            Text {
                text: "HDG " + hudRoot.selectedHeading
                color: "#9fe3ff"
                font.pixelSize: 14
            }

            Text {
                text: "STATUS: " + hudRoot.selectedStatus
                color: "#e1f4ff"
                font.pixelSize: 14
                font.bold: true
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 18
        width: 240
        height: 140
        radius: 12
        color: "#081822"
        border.color: "#31647d"
        opacity: 0.9

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Text {
                text: "LEGEND"
                color: "#ebf3ff"
                font.pixelSize: 16
                font.bold: true
            }

            Row {
                spacing: 8
                Rectangle { width: 12; height: 12; radius: 6; color: "#5ad1ff" }
                Text { text: "FRIENDLY"; color: "#dff6ff"; font.pixelSize: 12 }
            }

            Row {
                spacing: 8
                Rectangle { width: 12; height: 12; radius: 6; color: "#ffd166" }
                Text { text: "UNKNOWN"; color: "#dff6ff"; font.pixelSize: 12 }
            }

            Row {
                spacing: 8
                Rectangle { width: 12; height: 12; radius: 6; color: "#ff6a5a" }
                Text { text: "HOSTILE"; color: "#dff6ff"; font.pixelSize: 12 }
            }

            Row {
                spacing: 8
                Rectangle { width: 12; height: 12; radius: 6; color: "#ff9f59" }
                Text { text: "THREAT ZONE"; color: "#dff6ff"; font.pixelSize: 12 }
            }
        }
    }
}
