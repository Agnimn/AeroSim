import QtQuick
import QtQuick3D

Node {
    id: flightPathRoot

    property var points: []
    property color pathColor: "#35d7ff"
    property bool selected: false
    property real trailHeight: 18

    Repeater {
        model: flightPathRoot.points.length > 1 ? flightPathRoot.points.length - 1 : 0

        delegate: Node {
            id: pathSegment

            property var segmentStart: flightPathRoot.points[index]
            property var segmentEnd: flightPathRoot.points[index + 1]
            property real dx: segmentEnd.x - segmentStart.x
            property real dz: segmentEnd.z - segmentStart.z
            property real distance: Math.sqrt(dx * dx + dz * dz)
            property real angle: Math.atan2(dx, dz) * 180 / Math.PI

            position: Qt.vector3d(
                (segmentStart.x + segmentEnd.x) * 0.5,
                Math.max(segmentStart.y, segmentEnd.y) + flightPathRoot.trailHeight,
                (segmentStart.z + segmentEnd.z) * 0.5
            )

            eulerRotation: Qt.vector3d(0, -angle, 0)

            Model {
                source: "#Cube"
                scale: Qt.vector3d(Math.max(2, pathSegment.distance), 0.7, 1.2)
                position: Qt.vector3d(0, 0, 0)
                materials: PrincipledMaterial {
                    baseColor: flightPathRoot.pathColor
                    opacity: flightPathRoot.selected ? 0.98 : 0.7
                    roughness: 0.35
                    metalness: 0.1
                }
            }
        }
    }
}
