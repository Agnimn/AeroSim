import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.AssetUtils
import QtQuick3D.Helpers

Item {
    id: aircraft3dView

    property string aircraftId: ""
    property string aircraftName: "Aircraft"
    property real aircraftLatitude: 0
    property real aircraftLongitude: 0
    property real aircraftAltitude: 0
    property real aircraftSpeed: 0
    property real aircraftHeading: 0
    property real aircraftVerticalSpeed: 0
    property string aircraftWaypoint: "HOLD"

    // The production GLB is optional and is not present in this checkout.
    // Keep the RuntimeLoader inactive until an authorized aircraft.glb is supplied.
    property url aircraftModelSource: ""

    signal backRequested()

    // =========================================================
    // CAMERA / VIEW SETTINGS
    // =========================================================

    property real yaw: 0
    property real pitch: 0
    property real cameraDistance: 650

    // Aircraft height above terrain
    property real aircraftHeight: 120

    property real referenceLatitude: 0
    property real referenceLongitude: 0

    readonly property var aircraftLocalPosition:
        geoCoordinateConverter.toLocal(aircraftLatitude,
                                       aircraftLongitude,
                                       aircraftAltitude)

    readonly property bool realTerrainAvailable:
        terrainData.available && realTerrainImage.status === Image.Ready
    readonly property real terrainElevationRange:
        realTerrainAvailable
            ? terrainData.maximumElevationMeters - terrainData.minimumElevationMeters
            : 0
    readonly property real terrainBaseY:
        realTerrainAvailable
            ? aircraft3dView.aircraftHeight +
              terrainData.minimumElevationMeters * 0.3048 *
              geoCoordinateConverter.verticalExaggeration /
              geoCoordinateConverter.metersPerSceneUnit
            : -120
        readonly property real terrainWidth:
                realTerrainAvailable
                        ? (terrainData.maximumLongitude - terrainData.minimumLongitude) *
                            111320 * Math.cos((terrainData.minimumLatitude +
                                                                    terrainData.maximumLatitude) * Math.PI / 360) /
                            geoCoordinateConverter.metersPerSceneUnit
                        : 900
        readonly property real terrainDepth:
                realTerrainAvailable
                        ? (terrainData.maximumLatitude - terrainData.minimumLatitude) *
                            111320 / geoCoordinateConverter.metersPerSceneUnit
                        : 900

    property point lastDragTranslation: Qt.point(0, 0)

    readonly property bool externalModelLoaded:
        aircraftModel.status === RuntimeLoader.Success

    GeoCoordinateConverter {
        id: geoCoordinateConverter
        originLatitude: aircraft3dView.referenceLatitude
        originLongitude: aircraft3dView.referenceLongitude
    }

    Image {
        id: realTerrainImage
        source: terrainData.available
                ? "qrc:/AeroSim/assets/terrain/north_india_dem.png"
                : "qrc:/AeroSim/assets/terrain/test_heightmap.png"
        asynchronous: false
        visible: false
    }


    // =========================================================
    // BACKGROUND
    // =========================================================

    Rectangle {
        anchors.fill: parent
        color: "#07121c"
    }


    // =========================================================
    // 3D VIEW
    // =========================================================

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


        // =====================================================
        // CAMERA
        // =====================================================

        PerspectiveCamera {

            id: camera

            /*
             * IMPORTANT:
             *
             * The camera and aircraft are now at almost the
             * same Y level.
             *
             * This keeps the aircraft in the CENTER.
             */

            position: Qt.vector3d(
                0,
                aircraft3dView.aircraftHeight,
                aircraft3dView.cameraDistance
            )

            /*
             * 0 rotation means:
             *
             * Camera looks straight toward -Z.
             *
             * No unwanted downward angle.
             */

            eulerRotation: Qt.vector3d(
                0,
                0,
                0
            )

            clipNear: 1

            clipFar: 10000

            fieldOfView: 40
        }


        // =====================================================
        // MAIN LIGHT
        // =====================================================

        DirectionalLight {

            eulerRotation: Qt.vector3d(
                -35,
                -35,
                0
            )

            brightness: 1.8

            ambientColor: "#71839b"
        }


        // =====================================================
        // SECOND LIGHT
        // =====================================================

        DirectionalLight {

            eulerRotation: Qt.vector3d(
                20,
                140,
                0
            )

            brightness: 0.7
        }


        // =====================================================
        // TERRAIN
        // =====================================================

        Model {

            id: testTerrain

            position: Qt.vector3d(
                0,
                aircraft3dView.terrainBaseY,
                0
            )

            geometry: HeightFieldGeometry {

                source:
                    aircraft3dView.realTerrainAvailable
                    ? "qrc:/AeroSim/assets/terrain/north_india_dem.png"
                    : "qrc:/AeroSim/assets/terrain/test_heightmap.png"

                extents: Qt.vector3d(
                    aircraft3dView.terrainWidth,
                    aircraft3dView.realTerrainAvailable
                    ? aircraft3dView.terrainElevationRange * 0.3048 *
                      geoCoordinateConverter.verticalExaggeration /
                      geoCoordinateConverter.metersPerSceneUnit
                    : 180,
                    aircraft3dView.terrainDepth
                )

                smoothShading: true
            }

            materials: PrincipledMaterial {

                baseColor: "#52664a"

                roughness: 0.92

                metalness: 0.0
            }

            castsShadows: true

            receivesShadows: true
        }


        // =====================================================
        // DEVELOPMENT AIRCRAFT
        // =====================================================

        Node {

            id: aircraftRoot

            /*
             * Aircraft starts exactly in front
             * of the camera.
             */

            position: Qt.vector3d(
                aircraft3dView.aircraftLocalPosition.x,
                aircraft3dView.aircraftLocalPosition.y + aircraft3dView.aircraftHeight,
                aircraft3dView.aircraftLocalPosition.z
            )

            /*
             * Start with NO rotation.
             *
             * This makes debugging much easier.
             */

            eulerRotation: Qt.vector3d(
                aircraft3dView.pitch,
                aircraft3dView.yaw - aircraft3dView.aircraftHeading,
                0
            )

            /*
             * Slightly larger so that it is clearly visible.
             */

            scale: Qt.vector3d(
                1.25,
                1.25,
                1.25
            )

            visible:
                !aircraft3dView.externalModelLoaded


            // =================================================
            // AIRCRAFT BODY
            // =================================================

            Model {

                source: "#Cylinder"

                scale: Qt.vector3d(
                    0.38,
                    2.75,
                    0.38
                )

                eulerRotation:
                    Qt.vector3d(90, 0, 0)

                materials: PrincipledMaterial {

                    baseColor: "#b9c7d5"

                    metalness: 0.48

                    roughness: 0.32
                }
            }


            // =================================================
            // NOSE
            // =================================================

            Model {

                source: "#Cone"

                position: Qt.vector3d(
                    0,
                    0,
                    -165
                )

                scale: Qt.vector3d(
                    0.38,
                    1.0,
                    0.38
                )

                eulerRotation:
                    Qt.vector3d(-90, 0, 0)

                materials: PrincipledMaterial {

                    baseColor: "#d4dce4"

                    metalness: 0.42

                    roughness: 0.3
                }
            }


            // =================================================
            // MAIN WINGS
            // =================================================

            Model {

                source: "#Cube"

                position: Qt.vector3d(
                    0,
                    0,
                    18
                )

                scale: Qt.vector3d(
                    2.15,
                    0.09,
                    0.72
                )

                materials: PrincipledMaterial {

                    baseColor: "#71879d"

                    metalness: 0.38

                    roughness: 0.38
                }
            }


            // =================================================
            // TAIL WING
            // =================================================

            Model {

                source: "#Cube"

                position: Qt.vector3d(
                    0,
                    0,
                    112
                )

                scale: Qt.vector3d(
                    0.92,
                    0.07,
                    0.35
                )

                materials: PrincipledMaterial {

                    baseColor: "#71879d"

                    metalness: 0.38

                    roughness: 0.38
                }
            }


            // =================================================
            // VERTICAL TAIL
            // =================================================

            Model {

                source: "#Cube"

                position: Qt.vector3d(
                    0,
                    32,
                    112
                )

                scale: Qt.vector3d(
                    0.09,
                    0.72,
                    0.34
                )

                materials: PrincipledMaterial {

                    baseColor: "#3184b8"

                    metalness: 0.35

                    roughness: 0.36
                }
            }


            // =================================================
            // LEFT ENGINE
            // =================================================

            Model {

                source: "#Cylinder"

                position: Qt.vector3d(
                    -34,
                    -4,
                    38
                )

                scale: Qt.vector3d(
                    0.16,
                    1.55,
                    0.16
                )

                eulerRotation:
                    Qt.vector3d(90, 0, 0)

                materials: PrincipledMaterial {

                    baseColor: "#384b5e"

                    metalness: 0.55

                    roughness: 0.3
                }
            }


            // =================================================
            // RIGHT ENGINE
            // =================================================

            Model {

                source: "#Cylinder"

                position: Qt.vector3d(
                    34,
                    -4,
                    38
                )

                scale: Qt.vector3d(
                    0.16,
                    1.55,
                    0.16
                )

                eulerRotation:
                    Qt.vector3d(90, 0, 0)

                materials: PrincipledMaterial {

                    baseColor: "#384b5e"

                    metalness: 0.55

                    roughness: 0.3
                }
            }
        }


        // =====================================================
        // REAL GLB AIRCRAFT
        // =====================================================

        RuntimeLoader {

            id: aircraftModel

            source:
                aircraft3dView.aircraftModelSource

            visible:
                aircraft3dView.externalModelLoaded

            position: Qt.vector3d(
                aircraft3dView.aircraftLocalPosition.x,
                aircraft3dView.aircraftLocalPosition.y + aircraft3dView.aircraftHeight,
                aircraft3dView.aircraftLocalPosition.z
            )

            eulerRotation: Qt.vector3d(0, -aircraft3dView.aircraftHeading, 0)

            scale: Qt.vector3d(
                1,
                1,
                1
            )

            onStatusChanged: {

                if (status === RuntimeLoader.Error) {

                    console.warn(
                        "Could not load local aircraft GLB:",
                        errorString
                    )
                }
            }
        }


        // =====================================================
        // MOUSE WHEEL = ZOOM
        // =====================================================

        WheelHandler {

            target: null

            onWheel: function(event) {

                aircraft3dView.cameraDistance =
                    Math.max(
                        350,
                        Math.min(
                            1200,
                            aircraft3dView.cameraDistance -
                            event.angleDelta.y * 0.5
                        )
                    )
            }
        }


        // =====================================================
        // MOUSE DRAG = ROTATE AIRCRAFT
        // =====================================================

        DragHandler {

            target: null

            onActiveChanged: {

                if (!active) {

                    aircraft3dView.lastDragTranslation =
                        Qt.point(0, 0)
                }
            }

            onTranslationChanged: {

                aircraft3dView.yaw +=
                    (
                        translation.x -
                        aircraft3dView.lastDragTranslation.x
                    ) * 0.15


                aircraft3dView.pitch =
                    Math.max(
                        -75,
                        Math.min(
                            75,
                            aircraft3dView.pitch -
                            (
                                translation.y -
                                aircraft3dView.lastDragTranslation.y
                            ) * 0.15
                        )
                    )


                aircraft3dView.lastDragTranslation =
                    translation
            }
        }
    }


    // =========================================================
    // TITLE
    // =========================================================

    Text {

        anchors.top:
            parent.top

        anchors.horizontalCenter:
            parent.horizontalCenter

        anchors.topMargin:
            28

        text:
            aircraft3dView.aircraftName +
            "  •  " +
            aircraft3dView.aircraftId

        color:
            "#ffffff"

        font.pixelSize:
            22

        font.bold:
            true

        z: 10
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 22
        anchors.bottomMargin: 22
        text: aircraft3dView.realTerrainAvailable
              ? "TERRAIN: LOCAL DEM"
              : "TERRAIN: SYNTHETIC TEST SURFACE"
        color: aircraft3dView.realTerrainAvailable ? "#4cd964" : "#f2b84b"
        font.pixelSize: 12
        font.bold: true
        z: 10
    }

    Text {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 82
        text: "ALT " + Math.round(aircraft3dView.aircraftAltitude) + " ft  •  SPD " +
              Math.round(aircraft3dView.aircraftSpeed) + " kt  •  HDG " +
              Math.round(aircraft3dView.aircraftHeading) + "°  •  VS " +
              Math.round(aircraft3dView.aircraftVerticalSpeed) + " ft/min  •  NEXT " +
              aircraft3dView.aircraftWaypoint
        color: "#9fb6c5"
        font.pixelSize: 12
        z: 10
    }


    // =========================================================
    // MODEL STATUS
    // =========================================================

    Text {

        anchors.top:
            parent.top

        anchors.horizontalCenter:
            parent.horizontalCenter

        anchors.topMargin:
            62

        text:
            aircraft3dView.externalModelLoaded
            ? "LOCAL AIRCRAFT MODEL"
            : "DEVELOPMENT MODEL  •  LOCAL GLB NOT LOADED"

        color:
            aircraft3dView.externalModelLoaded
            ? "#4cd964"
            : "#8fa8ba"

        font.pixelSize:
            12

        z: 10
    }


    // =========================================================
    // BACK BUTTON
    // =========================================================

    Button {

        id: backButton

        anchors.left:
            parent.left

        anchors.top:
            parent.top

        anchors.margins:
            22

        text:
            "←  Back to 2D"

        z:
            10

        onClicked:
            aircraft3dView.backRequested()
    }


    // =========================================================
    // RESET VIEW
    // =========================================================

    Button {

        anchors.right:
            parent.right

        anchors.top:
            parent.top

        anchors.margins:
            22

        text:
            "Reset View"

        z:
            10

        onClicked: {

            aircraft3dView.yaw = 0

            aircraft3dView.pitch = 0

            aircraft3dView.cameraDistance = 650
        }
    }
}