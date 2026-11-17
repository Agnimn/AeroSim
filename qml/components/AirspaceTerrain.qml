import QtQuick
import QtQuick3D

Node {
    id: terrainRoot

    property real terrainWidth: 1800
    property real terrainDepth: 1800
    property real terrainHeight: 270
    property string heightmapSource: "qrc:/AeroSim/assets/terrain/airspace_heightmap.png"

    Model {
        id: mainTerrain

        position: Qt.vector3d(0, -42, 0)
        scale: Qt.vector3d(1.35, 1.0, 1.35)

        geometry: HeightFieldGeometry {
            source: terrainRoot.heightmapSource
            extents: Qt.vector3d(terrainRoot.terrainWidth, terrainRoot.terrainHeight, terrainRoot.terrainDepth)
            rows: 128
            columns: 128
            smoothShading: true
        }

        materials: PrincipledMaterial {
            baseColor: "#6d7d4c"
            roughness: 0.95
            metalness: 0.04
        }

        receivesShadows: true
        castsShadows: true
    }

    Model {
        position: Qt.vector3d(0, -80, 0)
        scale: Qt.vector3d(1400, 28, 1400)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#4c5d3d"
            roughness: 1.0
            metalness: 0.0
        }
        receivesShadows: true
        castsShadows: true
    }

    Model {
        position: Qt.vector3d(0, -72, 700)
        scale: Qt.vector3d(1600, 8, 220)
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: "#2c4b69"
            roughness: 0.75
            metalness: 0.05
        }
        receivesShadows: true
        castsShadows: true
    }
}
