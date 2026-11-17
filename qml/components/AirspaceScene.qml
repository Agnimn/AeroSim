import QtQuick
import QtQuick.Controls
import QtQuick3D

Item {
    id: airspaceScene

    signal aircraftSelected(string entityId)
    signal backRequested()

    Component.onCompleted: console.log("AIRSPACE 3D PAGE LOADED")

    Rectangle {
        anchors.fill: parent
        color: "#07121c"
    }

    View3D {
        id: view3d
        anchors.fill: parent
        camera: camera

        Component.onCompleted: console.log("AIRSPACE View3D CREATED")

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#07121c"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            id: camera
            position: Qt.vector3d(0, 0, 600)
            eulerRotation: Qt.vector3d(0, 0, 0)
            clipNear: 1
            clipFar: 10000
            fieldOfView: 40
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-35, -35, 0)
            brightness: 1.8
            ambientColor: "#71839b"
        }

        Model {
            source: "#Cube"
            position: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(1, 1, 1)

            Component.onCompleted: console.log("TEST CUBE CREATED")

            materials: PrincipledMaterial {
                baseColor: "#00aaff"
                roughness: 0.4
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: "3D RENDER TEST"
        color: "white"
        font.pixelSize: 24
        z: 200
    }

    Button {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 18
        text: "←  Back to 2D"
        z: 200
        onClicked: airspaceScene.backRequested()
    }
}
