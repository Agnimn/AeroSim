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

    // =========================================================
    // SCENARIO DATA
    // =========================================================

    property int entityCount: 0
    property string entityType: ""
    property string region: ""
    property bool scenarioGenerated: false
    property string statusMessage: "READY"

    // =========================================================
    // MAP PLUGIN
    // =========================================================

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    // =========================================================
    // ENTITY MODEL
    // =========================================================
    // This model contains the aircraft/ship positions.
    // MapItemView below will convert each item into a
    // MapQuickItem on the map.

    ListModel {
        id: entityModel
    }

    // =========================================================
    // REGION CENTER
    // =========================================================

    function getRegionCenter() {

        if (region === "North India") {
            return QtPositioning.coordinate(29.0, 79.0)
        }

        if (region === "South India") {
            return QtPositioning.coordinate(13.5, 78.5)
        }

        if (region === "East India") {
            return QtPositioning.coordinate(23.5, 87.5)
        }

        if (region === "West India") {
            return QtPositioning.coordinate(21.0, 73.5)
        }

        return QtPositioning.coordinate(22.5, 79.0)
    }

    // =========================================================
    // ENTITY COORDINATES
    // =========================================================

    function getEntityCoordinate(index) {

        // -----------------------------------------------------
        // NORTH INDIA
        // -----------------------------------------------------

        if (region === "North India") {

            var northPoints = [
                QtPositioning.coordinate(28.6139, 77.2090), // Delhi
                QtPositioning.coordinate(30.7333, 76.7794), // Chandigarh
                QtPositioning.coordinate(26.9124, 75.7873), // Jaipur
                QtPositioning.coordinate(27.1767, 78.0081), // Agra
                QtPositioning.coordinate(31.6340, 74.8723), // Amritsar
                QtPositioning.coordinate(30.3165, 78.0322), // Dehradun
                QtPositioning.coordinate(29.9457, 78.1642), // Haridwar
                QtPositioning.coordinate(25.4358, 81.8463)  // Prayagraj
            ]

            return northPoints[index % northPoints.length]
        }

        // -----------------------------------------------------
        // SOUTH INDIA
        // -----------------------------------------------------

        if (region === "South India") {

            var southPoints = [
                QtPositioning.coordinate(12.9716, 77.5946), // Bengaluru
                QtPositioning.coordinate(13.0827, 80.2707), // Chennai
                QtPositioning.coordinate(17.3850, 78.4867), // Hyderabad
                QtPositioning.coordinate(11.0168, 76.9558), // Coimbatore
                QtPositioning.coordinate(15.3173, 75.7139), // Hubballi
                QtPositioning.coordinate(9.9312, 76.2673)   // Kochi
            ]

            return southPoints[index % southPoints.length]
        }

        // -----------------------------------------------------
        // EAST INDIA
        // -----------------------------------------------------

        if (region === "East India") {

            var eastPoints = [
                QtPositioning.coordinate(22.5726, 88.3639), // Kolkata
                QtPositioning.coordinate(20.2961, 85.8245), // Bhubaneswar
                QtPositioning.coordinate(26.1445, 91.7362), // Guwahati
                QtPositioning.coordinate(23.3441, 85.3096), // Ranchi
                QtPositioning.coordinate(25.5941, 85.1376)  // Patna
            ]

            return eastPoints[index % eastPoints.length]
        }

        // -----------------------------------------------------
        // WEST INDIA
        // -----------------------------------------------------

        if (region === "West India") {

            var westPoints = [
                QtPositioning.coordinate(19.0760, 72.8777), // Mumbai
                QtPositioning.coordinate(23.0225, 72.5714), // Ahmedabad
                QtPositioning.coordinate(21.1702, 72.8311), // Surat
                QtPositioning.coordinate(15.4909, 73.8278), // Goa
                QtPositioning.coordinate(18.5204, 73.8567)  // Pune
            ]

            return westPoints[index % westPoints.length]
        }

        return QtPositioning.coordinate(22.5, 79.0)
    }

    // =========================================================
    // CREATE ENTITY MODEL
    // =========================================================

    function createEntities() {

        // Remove previous aircraft
        entityModel.clear()

        console.log("Creating entities...")
        console.log("Count:", entityCount)
        console.log("Type:", entityType)
        console.log("Region:", region)

        for (var i = 0; i < entityCount; i++) {

            var coordinate = getEntityCoordinate(i)

            entityModel.append({
                latitude: coordinate.latitude,
                longitude: coordinate.longitude,
                entityNumber: i + 1,
                entityName: entityType + " " +
                             String(i + 1).padStart(2, "0")
            })

            console.log(
                "Entity",
                i + 1,
                "Lat:",
                coordinate.latitude,
                "Lon:",
                coordinate.longitude
            )
        }

        console.log(
            "Entities in model:",
            entityModel.count
        )
    }

    // =========================================================
    // GENERATE SCENARIO
    // =========================================================

    function generateScenario() {

        console.log("======================================")
        console.log("GENERATE 2D BUTTON CLICKED")

        var prompt = promptInput.text.trim().toLowerCase()

        console.log("Prompt:", prompt)

        // -----------------------------------------------------
        // EMPTY PROMPT
        // -----------------------------------------------------

        if (prompt.length === 0) {

            statusMessage = "PLEASE ENTER A SCENARIO"

            console.log("ERROR: Empty prompt")

            return
        }

        // -----------------------------------------------------
        // DETECT ENTITY TYPE
        // -----------------------------------------------------

        if (
            prompt.includes("aircraft") ||
            prompt.includes("aircrafts") ||
            prompt.includes("fighter") ||
            prompt.includes("plane") ||
            prompt.includes("jet")
        ) {

            entityType = "Aircraft"

        } else if (
            prompt.includes("ship") ||
            prompt.includes("ships") ||
            prompt.includes("shipborne")
        ) {

            entityType = "Ship"

        } else {

            entityType = "Unknown"
        }

        // -----------------------------------------------------
        // DETECT REGION
        // -----------------------------------------------------

        if (prompt.includes("north india")) {

            region = "North India"

        } else if (prompt.includes("south india")) {

            region = "South India"

        } else if (prompt.includes("east india")) {

            region = "East India"

        } else if (prompt.includes("west india")) {

            region = "West India"

        } else {

            region = "Unknown Region"
        }

        // -----------------------------------------------------
        // DETECT NUMBER
        // -----------------------------------------------------

        var match = prompt.match(/\d+/)

        if (match !== null && match.length > 0) {

            entityCount = parseInt(match[0], 10)

        } else {

            entityCount = 1
        }

        // -----------------------------------------------------
        // SAFETY LIMIT
        // -----------------------------------------------------

        if (entityCount < 1)
            entityCount = 1

        if (entityCount > 20)
            entityCount = 20

        console.log("Entity Type:", entityType)
        console.log("Region:", region)
        console.log("Entity Count:", entityCount)

        // -----------------------------------------------------
        // RESET
        // -----------------------------------------------------

        scenarioGenerated = false
        statusMessage = "GENERATING..."

        // -----------------------------------------------------
        // CREATE ENTITIES
        // -----------------------------------------------------

        createEntities()

        // -----------------------------------------------------
        // UPDATE MAP
        // -----------------------------------------------------

        Qt.callLater(function() {

            scenarioMap.center = getRegionCenter()
            scenarioMap.zoomLevel = 6

            scenarioGenerated = true
            statusMessage = "GENERATED"

            console.log(
                "Map items:",
                scenarioMap.mapItems.length
            )

            console.log(
                "Scenario generated successfully"
            )

            console.log("======================================")
        })
    }

    // =========================================================
    // MAIN LAYOUT
    // =========================================================

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

                    text: "STEP 1 • PROMPT → 2D"

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

                    model: entityModel

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
                                    entityType === "Aircraft"
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
                                        entityType === "Aircraft"
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

                                        console.log(
                                            "Selected:",
                                            model.entityName
                                        )
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
                                        model.entityName

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
}