import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.AssetUtils

Item {
    id: aircraft3dView

    property string aircraftId: ""
    property string aircraftName: "Aircraft"
    // Optional locally bundled GLB. Keep a generic 3D mesh assembly visible
    // until an authorized project model is placed at the documented path.
    property url aircraftModelSource: "qrc:/AeroSim/assets/aircraft/aircraft.glb"
    signal backRequested()

    property real yaw: -20
    property real pitch: -12
    property real cameraDistance: 560
    property point lastDragTranslation: Qt.point(0, 0)
    readonly property bool externalModelLoaded: aircraftModel.status === RuntimeLoader.Success

    Rectangle {
        anchors.fill: parent
        color: "#07121c"
    }

    View3D {
        id: view3d
        anchors.fill: parent
        camera: camera
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#07121c"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            id: camera
            position: Qt.vector3d(0, 170, aircraft3dView.cameraDistance)
            eulerRotation: Qt.vector3d(-12, 0, 0)
            clipNear: 1
            clipFar: 10000
            fieldOfView: 42
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-35, -35, 0)
            brightness: 1.6
            ambientColor: "#71839b"
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(15, 145, 0)
            brightness: 0.65
        }

        Node {
            id: aircraftRoot
            visible: !aircraft3dView.externalModelLoaded
            eulerRotation: Qt.vector3d(aircraft3dView.pitch, aircraft3dView.yaw, 0)

            // Original, low-poly development aircraft assembled from genuine
            // Qt Quick 3D meshes. Replace with the locally bundled GLB for the
            // final aircraft asset.
            Model {
                source: "#Cylinder"
                scale: Qt.vector3d(0.38, 2.75, 0.38)
                eulerRotation: Qt.vector3d(90, 0, 0)
                materials: PrincipledMaterial {
                    baseColor: "#b9c7d5"
                    metalness: 0.48
                    roughness: 0.32
                }
            }

            Model {
                source: "#Cone"
                position: Qt.vector3d(0, 0, -165)
                scale: Qt.vector3d(0.38, 1.0, 0.38)
                eulerRotation: Qt.vector3d(-90, 0, 0)
                materials: PrincipledMaterial {
                    baseColor: "#d4dce4"
                    metalness: 0.42
                    roughness: 0.3
                }
            }

            Model {
                source: "#Cube"
                position: Qt.vector3d(0, 0, 18)
                scale: Qt.vector3d(2.15, 0.09, 0.72)
                eulerRotation: Qt.vector3d(0, 0, -7)
                materials: PrincipledMaterial {
                    baseColor: "#71879d"
                    metalness: 0.38
                    roughness: 0.38
                }
            }

            Model {
                source: "#Cube"
                position: Qt.vector3d(0, 0, 112)
                scale: Qt.vector3d(0.92, 0.07, 0.35)
                eulerRotation: Qt.vector3d(0, 0, -4)
                materials: PrincipledMaterial {
                    baseColor: "#71879d"
                    metalness: 0.38
                    roughness: 0.38
                }
            }

            Model {
                source: "#Cube"
                position: Qt.vector3d(0, 32, 112)
                scale: Qt.vector3d(0.09, 0.72, 0.34)
                materials: PrincipledMaterial {
                    baseColor: "#3184b8"
                    metalness: 0.35
                    roughness: 0.36
                }
            }

            Model {
                source: "#Cylinder"
                position: Qt.vector3d(-34, -4, 38)
                scale: Qt.vector3d(0.16, 1.55, 0.16)
                eulerRotation: Qt.vector3d(90, 0, 0)
                materials: PrincipledMaterial {
                    baseColor: "#384b5e"
                    metalness: 0.55
                    roughness: 0.3
                }
            }

            Model {
                source: "#Cylinder"
                position: Qt.vector3d(34, -4, 38)
                scale: Qt.vector3d(0.16, 1.55, 0.16)
                eulerRotation: Qt.vector3d(90, 0, 0)
                materials: PrincipledMaterial {
                    baseColor: "#384b5e"
                    metalness: 0.55
                    roughness: 0.3
                }
            }
        }

        RuntimeLoader {
            id: aircraftModel
            source: aircraft3dView.aircraftModelSource
            visible: aircraft3dView.externalModelLoaded
            onStatusChanged: {
                if (status === RuntimeLoader.Error)
                    console.warn("Could not load local aircraft GLB:", errorString)
            }
        }

        WheelHandler {
            target: null
            onWheel: function(event) {
                aircraft3dView.cameraDistance = Math.max(300,
                        Math.min(900, aircraft3dView.cameraDistance - event.angleDelta.y * 0.35))
            }
        }

        DragHandler {
            target: null
            onActiveChanged: aircraft3dView.lastDragTranslation = Qt.point(0, 0)
            onTranslationChanged: {
                aircraft3dView.yaw +=
                        (translation.x - aircraft3dView.lastDragTranslation.x) * 0.12
                aircraft3dView.pitch = Math.max(-75, Math.min(75,
                        aircraft3dView.pitch -
                        (translation.y - aircraft3dView.lastDragTranslation.y) * 0.12))
                aircraft3dView.lastDragTranslation = translation
            }
        }
    }

    Text {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 28
        text: aircraft3dView.aircraftName + "  •  " + aircraft3dView.aircraftId
        color: "#ffffff"
        font.pixelSize: 22
        font.bold: true
        z: 10
    }

    Text {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 62
        text: aircraft3dView.externalModelLoaded
              ? "LOCAL AIRCRAFT MODEL"
              : "DEVELOPMENT MODEL  •  ADD assets/aircraft/aircraft.glb TO USE FINAL MODEL"
        color: aircraft3dView.externalModelLoaded ? "#4cd964" : "#8fa8ba"
        font.pixelSize: 12
        z: 10
    }

    Button {
        id: backButton
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 22
        text: "←  Back to 2D"
        z: 10
        onClicked: aircraft3dView.backRequested()
    }

    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 22
        text: "Reset view"
        z: 10
        onClicked: {
            aircraft3dView.yaw = -20
            aircraft3dView.pitch = -12
            aircraft3dView.cameraDistance = 560
        }
    }

}
