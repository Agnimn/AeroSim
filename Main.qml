import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtLocation
import QtPositioning

ApplicationWindow {
    id: window

    width: 1400
    height: 850
    visible: true

    title: "Airspace Scenario Generator"
    color: "#07121c"

    readonly property int entityCount: scenarioEngine.entityCount
    readonly property string entityType: scenarioEngine.entityType
    readonly property string region: scenarioEngine.region
    readonly property bool scenarioGenerated: scenarioEngine.scenarioGenerated
    readonly property string statusMessage: scenarioEngine.statusMessage

    property bool showingAircraft3D: false

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    function generateScenario() {
        if (scenarioEngine.generateScenario(promptInput.text)) {
            scenarioMap.center = QtPositioning.coordinate(
                        scenarioEngine.centerLatitude,
                        scenarioEngine.centerLongitude)
            scenarioMap.zoomLevel = 6
        }
    }

    ColumnLayout {

        anchors.fill: parent
        spacing: 0

        // =====================================================
        // HEADER
        // =====================================================

        Rectangle {

            Layout.fillWidth: true
            height: 80

            color: "#111e29"
            z: 100

            RowLayout {

                anchors.fill: parent

                anchors.leftMargin: 30
                anchors.rightMargin: 30

                Text {

                    text: "AIRSPACE SCENARIO GENERATOR"

                    color: "#ffffff"

                    font.pixelSize: 28
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {

                      text: window.showingAircraft3D
                          ? "STEP 2 • AIRCRAFT 3D"
                          : "STEP 1 • PROMPT → 2D"

                    color: "#45aaf2"

                    font.pixelSize: 16
                    font.bold: true
                }
            }
        }

        // =====================================================
        // PROMPT AREA
        // =====================================================

        Rectangle {

            Layout.fillWidth: true
            height: 150

            color: "#0b1822"
            z: 100

            ColumnLayout {

                anchors.fill: parent

                anchors.leftMargin: 30
                anchors.rightMargin: 30
                anchors.topMargin: 15
                anchors.bottomMargin: 15

                spacing: 10

                Text {

                    text: "ENTER SCENARIO PROMPT"

                    color: "#8fa8ba"

                    font.pixelSize: 14
                    font.bold: true
                }

                RowLayout {

                    Layout.fillWidth: true

                    spacing: 15

                    // =========================================
                    // PROMPT INPUT
                    // =========================================

                    TextField {

                        id: promptInput

                        Layout.fillWidth: true

                        Layout.preferredHeight: 55

                        placeholderText:
                            "Example: Create 3 aircrafts in North India"

                        font.pixelSize: 16

                        color: "#ffffff"

                        placeholderTextColor: "#718596"

                        background: Rectangle {

                            color: "#162733"

                            radius: 8

                            border.color: "#29485c"
                            border.width: 1
                        }

                        Keys.onReturnPressed: {
                            window.generateScenario()
                        }
                    }

                    // =========================================
                    // GENERATE BUTTON
                    // =========================================

                    Button {

                        id: generateButton

                        text: "GENERATE 2D"

                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 55

                        z: 200

                        contentItem: Text {

                            text: generateButton.text

                            color: "#ffffff"

                            horizontalAlignment:
                                Text.AlignHCenter

                            verticalAlignment:
                                Text.AlignVCenter

                            font.pixelSize: 15
                            font.bold: true
                        }

                        background: Rectangle {

                            radius: 8

                            color:
                                generateButton.down
                                ? "#075ea8"
                                : generateButton.hovered
                                  ? "#1688dc"
                                  : "#2196f3"

                            border.color:
                                generateButton.hovered
                                ? "#5ab8ff"
                                : "#2196f3"

                            border.width: 1
                        }

                        onClicked: {

                            console.log(
                                "BUTTON CLICK RECEIVED"
                            )

                            window.generateScenario()
                        }
                    }
                }
            }
        }

        // =====================================================
        // 2D SCENARIO AREA
        // =====================================================

        Rectangle {

            id: scenarioArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            color: "#020d15"

            // =================================================
            // REAL MAP
            // =================================================

            Map {

                id: scenarioMap

                anchors.fill: parent
                visible: !window.showingAircraft3D

                plugin: mapPlugin

                center:
                    QtPositioning.coordinate(
                        29.0,
                        79.0
                    )

                zoomLevel: 6

                copyrightsVisible: true

                z: 0

                // =============================================
                // MOUSE WHEEL ZOOM
                // =============================================

                WheelHandler {

                    id: wheelHandler

                    acceptedDevices:
                        PointerDevice.Mouse |
                        PointerDevice.TouchPad

                    onWheel: function(event) {

                        if (event.angleDelta.y > 0) {

                            scenarioMap.zoomLevel += 0.5

                        } else {

                            scenarioMap.zoomLevel -= 0.5
                        }
                    }
                }

                // =============================================
                // PINCH ZOOM
                // =============================================

                PinchHandler {

                    id: pinchHandler

                    target: null

                    property var startCentroid:
                        QtPositioning.coordinate(
                            29.0,
                            79.0
                        )

                    onActiveChanged: {

                        if (active) {

                            startCentroid =
                                scenarioMap.toCoordinate(
                                    centroid.position,
                                    false
                                )
                        }
                    }

                    onScaleChanged: function(delta) {

                        scenarioMap.zoomLevel +=
                            Math.log2(delta)

                        scenarioMap.alignCoordinateToPoint(
                            startCentroid,
                            centroid.position
                        )
                    }
                }

                // =================================================
                // ENTITY MAP VIEW
                // =================================================
                //
                // IMPORTANT:
                // We use MapItemView instead of Repeater.
                // This is the correct model/view mechanism for
                // dynamically displaying MapQuickItems.
                //
                // =================================================

                MapItemView {

                    id: entityMapView

                    parent: scenarioMap

                    model: scenarioEngine.entityModel

                    delegate: MapQuickItem {

                        id: entityMarker

                        coordinate:
                            QtPositioning.coordinate(
                                model.latitude,
                                model.longitude
                            )

                        anchorPoint.x:
                            marker.width / 2

                        anchorPoint.y:
                            marker.height / 2

                        zoomLevel: 0

                        autoFadeIn: false

                        // Make sure the marker is above
                        // the map tiles.

                        z: 100

                        sourceItem: Item {

                            id: marker

                            width: 70
                            height: 85

                            // =================================
                            // AIRCRAFT CIRCLE
                            // =================================

                            Rectangle {

                                id: aircraftCircle

                                width: 58
                                height: 58

                                radius: 29

                                anchors.horizontalCenter:
                                    parent.horizontalCenter

                                color:
                                    model.entityType === "aircraft"
                                    ? "#0b8fce"
                                    : "#f39c12"

                                border.color: "#ffffff"
                                border.width: 3

                                // =================================
                                // AIRCRAFT ICON
                                // =================================

                                Text {

                                    anchors.centerIn: parent

                                    text:
                                        model.entityType === "aircraft"
                                        ? "✈"
                                        : "◆"

                                    color: "#ffffff"

                                    font.pixelSize: 29
                                    font.bold: true
                                }

                                // =================================
                                // CLICK AIRCRAFT
                                // =================================

                                MouseArea {

                                    anchors.fill: parent

                                    cursorShape:
                                        Qt.PointingHandCursor

                                    onClicked: {
                                        if (model.entityType === "aircraft" &&
                                                scenarioEngine.selectAircraft(model.id)) {
                                            window.showingAircraft3D = true
                                        }
                                    }
                                }
                            }

                            // =================================
                            // ENTITY LABEL
                            // =================================

                            Rectangle {

                                anchors.top:
                                    aircraftCircle.bottom

                                anchors.topMargin: 3

                                anchors.horizontalCenter:
                                    parent.horizontalCenter

                                width:
                                    entityLabel.width + 14

                                height: 22

                                radius: 5

                                color: "#111e29"

                                opacity: 0.95

                                Text {

                                    id: entityLabel

                                    anchors.centerIn: parent

                                    text:
                                        model.name

                                    color: "#ffffff"

                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }

            // =================================================
            // REGION LABEL
            // =================================================

            Rectangle {

                visible: scenarioGenerated

                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter

                anchors.topMargin: 20

                width: regionText.width + 40
                height: 45

                radius: 8

                color: "#111e29"

                opacity: 0.94

                z: 200

                Text {

                    id: regionText

                    anchors.centerIn: parent

                    text: region

                    color: "#45aaf2"

                    font.pixelSize: 22
                    font.bold: true
                }
            }

            // =================================================
            // INFORMATION PANEL
            // =================================================

            Rectangle {

                visible: scenarioGenerated

                width: 280
                height: 190

                anchors.left: parent.left
                anchors.bottom: parent.bottom

                anchors.leftMargin: 25
                anchors.bottomMargin: 25

                color: "#111e29"

                radius: 10

                border.color: "#29485c"

                z: 200

                Column {

                    anchors.fill: parent

                    anchors.margins: 20

                    spacing: 10

                    Text {

                        text: "SCENARIO STATUS"

                        color: "#ffffff"

                        font.pixelSize: 18
                        font.bold: true
                    }

                    Text {

                        text:
                            "Region : " +
                            region

                        color: "#9fb6c5"

                        font.pixelSize: 14
                    }

                    Text {

                        text:
                            "Entity Type : " +
                            entityType

                        color: "#9fb6c5"

                        font.pixelSize: 14
                    }

                    Text {

                        text:
                            "Entities : " +
                            entityCount

                        color: "#9fb6c5"

                        font.pixelSize: 14
                    }

                    Text {

                        text:
                            "MAP : ONLINE"

                        color: "#45aaf2"

                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {

                        text:
                            "STATUS : " +
                            statusMessage

                        color:
                            statusMessage === "GENERATED"
                            ? "#4cd964"
                            : "#f5a623"

                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }

            // =================================================
            // EMPTY STATE
            // =================================================

            Rectangle {

                visible: !scenarioGenerated

                anchors.centerIn: parent

                width: 430
                height: 70

                radius: 10

                color: "#111e29"

                opacity: 0.92

                z: 200

                Text {

                    anchors.centerIn: parent

                    text:
                        "Enter a scenario prompt to generate the 2D view"

                    color: "#9fb6c5"

                    font.pixelSize: 16
                }
            }

            // =================================================
            // MAP ERROR
            // =================================================

            Rectangle {

                visible:
                    scenarioMap.error !== Map.NoError

                anchors.centerIn: parent

                width: 550
                height: 120

                radius: 10

                color: "#321820"

                border.color: "#ff5c5c"

                z: 300

                Column {

                    anchors.centerIn: parent

                    spacing: 8

                    Text {

                        text: "MAP ERROR"

                        color: "#ff7070"

                        font.bold: true
                        font.pixelSize: 18
                    }

                    Text {

                        text:
                            scenarioMap.errorString

                        color: "#ffffff"

                        font.pixelSize: 13

                        width: 500

                        wrapMode:
                            Text.WordWrap
                    }
                }
            }

            // =================================================
            // GENERATION SUCCESS
            // =================================================

            Rectangle {

                visible: scenarioGenerated

                anchors.top: parent.top
                anchors.right: parent.right

                anchors.topMargin: 20
                anchors.rightMargin: 25

                width: 190
                height: 45

                radius: 8

                color: "#123d32"

                border.color: "#4cd964"

                opacity: 0.95

                z: 250

                Text {

                    anchors.centerIn: parent

                    text: "✓ 2D GENERATED"

                    color: "#4cd964"

                    font.pixelSize: 14
                    font.bold: true
                }
            }
        }
    }

    Aircraft3DView {
        anchors.fill: parent
        visible: window.showingAircraft3D
        z: 1000
        aircraftId: scenarioEngine.selectedEntityId
        aircraftName: scenarioEngine.selectedEntityName

        onBackRequested: {
            window.showingAircraft3D = false
            scenarioEngine.clearSelectedAircraft()
        }
    }
}