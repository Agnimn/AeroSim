import QtQuick
import QtQuick3D

Node {
    id: airportRoot

    property real runwayLength: 280
    property real runwayWidth: 34

    position: Qt.vector3d(250, 0, 260)

    Model {
        position: Qt.vector3d(0, 18, 0)
        scale: Qt.vector3d(runwayLength / 14, 0.5, runwayWidth / 14)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#4b5265"
            roughness: 0.8
            metalness: 0.05
        }
    }

    Model {
        position: Qt.vector3d(0, 18.5, 0)
        scale: Qt.vector3d(runwayLength / 30, 0.25, runwayWidth / 20)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#dfe4eb"
            roughness: 0.4
            metalness: 0.1
        }
    }

    Model {
        position: Qt.vector3d(0, 18.2, 0)
        scale: Qt.vector3d(runwayLength / 14, 0.25, 10)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#e8edf5"
            roughness: 0.4
            metalness: 0.1
        }
    }
}
