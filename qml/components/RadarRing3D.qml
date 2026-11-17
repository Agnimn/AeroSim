import QtQuick
import QtQuick3D

Node {
    id: radarRoot

    property real radius: 140
    property real ringHeight: 16
    property color ringColor: "#62d0ff"
    property string ringLabel: "5 KM"

    Model {
        position: Qt.vector3d(0, ringHeight * 0.5, 0)
        scale: Qt.vector3d(radius / 14, 0.9, radius / 14)
        source: "#Cylinder"
        materials: PrincipledMaterial {
            baseColor: ringColor
            opacity: 0.25
            roughness: 0.35
            metalness: 0.15
        }
    }
}
