import QtQuick
import QtQuick3D

Node {
    id: aircraftRoot

    property string aircraftId: "AIR01"
    property string aircraftName: "AIR01"
    property string status: "friendly"
    property string entityType: "aircraft"
    property bool selected: false
    property bool isThreat: false
    property real sceneX: 0
    property real sceneZ: 0
    property real sceneY: 200
    property real heading: 0
    property real speed: 320
    property real altitude: 0
    property string displayStatus: "FRIENDLY"

    signal activated(string entityId)

    position: Qt.vector3d(sceneX, sceneY, sceneZ)
    eulerRotation: Qt.vector3d(0, -heading + 90, 0)

    function roleColor() {
        if (status === "hostile")
            return "#ff6a5a"
        if (status === "unknown")
            return "#ffd166"
        if (selected)
            return "#8fffe8"
        return "#5ad1ff"
    }

    Model {
        id: selectionHalo
        visible: selected
        position: Qt.vector3d(0, 12, 0)
        scale: Qt.vector3d(28, 16, 28)
        source: "#Sphere"
        materials: PrincipledMaterial {
            baseColor: "#98f5ff"
            opacity: 0.18
            roughness: 0.2
            metalness: 0.0
        }
    }

    Model {
        id: fuselage
        position: Qt.vector3d(0, 8, 0)
        scale: Qt.vector3d(18, 3, 7)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: roleColor()
            roughness: 0.35
            metalness: 0.4
        }
        pickable: true

        ObjectPicker {
            id: aircraftPicker
            onClicked: aircraftRoot.activated(aircraftRoot.aircraftId)
        }
    }

    Model {
        position: Qt.vector3d(0, 8, -14)
        scale: Qt.vector3d(28, 2, 5)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: roleColor()
            roughness: 0.35
            metalness: 0.4
        }
    }

    Model {
        position: Qt.vector3d(0, 12, 18)
        scale: Qt.vector3d(10, 2, 4)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#cfe5ff"
            roughness: 0.3
            metalness: 0.2
        }
    }

    Model {
        position: Qt.vector3d(0, 4, 18)
        scale: Qt.vector3d(6, 1, 4)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#dfe9f6"
            roughness: 0.3
            metalness: 0.2
        }
    }

    Model {
        position: Qt.vector3d(-10, 5, 9)
        scale: Qt.vector3d(2.5, 12, 2.5)
        source: "#Cylinder"
        eulerRotation: Qt.vector3d(90, 0, 0)
        materials: PrincipledMaterial {
            baseColor: "#374253"
            roughness: 0.4
            metalness: 0.55
        }
    }

    Model {
        position: Qt.vector3d(10, 5, 9)
        scale: Qt.vector3d(2.5, 12, 2.5)
        source: "#Cylinder"
        eulerRotation: Qt.vector3d(90, 0, 0)
        materials: PrincipledMaterial {
            baseColor: "#374253"
            roughness: 0.4
            metalness: 0.55
        }
    }

    Model {
        position: Qt.vector3d(0, 0, 0)
        scale: Qt.vector3d(5, 1, 5)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#a8b7c7"
            roughness: 0.3
            metalness: 0.2
        }
    }
}
