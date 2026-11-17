import QtQuick
import QtQuick3D

Node {
    id: threatZoneRoot

    property string label: "THREAT ZONE"
    property string threatColor: "#ff5b5b"
    property real radius: 120
    property real height: 140
    property real centerY: 0

    Model {
        position: Qt.vector3d(0, centerY + height * 0.3, 0)
        scale: Qt.vector3d(radius / 18, height / 18, radius / 18)
        source: "#Cylinder"
        materials: PrincipledMaterial {
            baseColor: threatColor
            opacity: 0.22
            roughness: 0.4
            metalness: 0.08
        }
    }

    Model {
        position: Qt.vector3d(0, centerY + height, 0)
        scale: Qt.vector3d(radius / 11, radius / 8, radius / 11)
        source: "#Cone"
        materials: PrincipledMaterial {
            baseColor: threatColor
            opacity: 0.12
            roughness: 0.5
            metalness: 0.0
        }
    }
}
