import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
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
    readonly property bool aiRequestPending: scenarioEngine.aiRequestPending

    property bool showingAircraft3D: false
    property bool showingAirspace3D: false

    function openAirspace3D() {
        if (!window.scenarioGenerated)
            return;

        window.showingAircraft3D = false;
        window.showingAirspace3D = true;
    }

    Plugin {
        id: mapPlugin
        name: "osm"

        PluginParameter {
            name: "osm.mapping.custom.host"
            value: mapboxConfigured
                   ? "https://api.mapbox.com/styles/v1/" + mapboxStyleId
                     + "/tiles/256/%z/%x/%y@2x?access_token="
                     + mapboxAccessToken
                   : "https://tile.openstreetmap.org/%z/%x/%y.png"
        }

        PluginParameter {
            name: "osm.mapping.custom.datacopyright"
            value: mapboxConfigured
                   ? "© Mapbox, © OpenStreetMap contributors"
                   : "© OpenStreetMap contributors"
        }
    }

    function generateScenario() {
        console.log("AI command submitted; entity count before:", scenarioEngine.entityCount)
        scenarioEngine.generateScenarioWithAI(promptInput.text)
    }

    Connections {
        target: scenarioEngine
        function onStatusMessageChanged() {
            console.log("AI/scenario status:", scenarioEngine.statusMessage,
                        "entity count:", scenarioEngine.entityCount)
        }
        function onAiRequestPendingChanged() {
            console.log("AI request pending:", scenarioEngine.aiRequestPending)
        }
    }

    FileDialog {
        id: geoJsonSaveDialog

        title: "Export scenario as GeoJSON"
        fileMode: FileDialog.SaveFile
        nameFilters: ["GeoJSON files (*.geojson)"]
        defaultSuffix: "geojson"
        selectedFile: "scenario.geojson"

        onAccepted: {
            var selectedUrl = geoJsonSaveDialog.selectedFile.toString()
            console.info("[GEOJSON EXPORT] FileDialog accepted; selectedFile=",
                         selectedUrl)

            if (selectedUrl.length === 0) {
                console.error("[GEOJSON EXPORT] FileDialog returned an empty URL")
                geoJsonExportResultDialog.title = "GeoJSON export failed"
                geoJsonExportResultDialog.text = "No save path was selected."
                geoJsonExportResultDialog.open()
                return
            }

            var succeeded = scenarioEngine.exportScenarioGeoJson(selectedUrl)
            console.info("[GEOJSON EXPORT] ScenarioEngine returned", succeeded,
                         "status=", scenarioEngine.statusMessage)
            geoJsonExportResultDialog.title = succeeded
                    ? "GeoJSON export complete"
                    : "GeoJSON export failed"
            geoJsonExportResultDialog.text = scenarioEngine.statusMessage
            geoJsonExportResultDialog.open()
        }
    }

    FileDialog {
        id: csvSaveDialog

        title: "Export scenario as CSV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV files (*.csv)"]
        defaultSuffix: "csv"
        selectedFile: "scenario.csv"

        onAccepted: {
            var selectedUrl = csvSaveDialog.selectedFile.toString()
            console.info("[CSV EXPORT] FileDialog accepted; selectedFile=",
                         selectedUrl)

            if (selectedUrl.length === 0) {
                console.error("[CSV EXPORT] FileDialog returned an empty URL")
                csvExportResultDialog.title = "CSV export failed"
                csvExportResultDialog.text = "No save path was selected."
                csvExportResultDialog.open()
                return
            }

            var succeeded = scenarioEngine.exportScenarioCsv(selectedUrl)
            console.info("[CSV EXPORT] ScenarioEngine returned", succeeded,
                         "status=", scenarioEngine.statusMessage)
            csvExportResultDialog.title = succeeded
                    ? "CSV export complete"
                    : "CSV export failed"
            csvExportResultDialog.text = scenarioEngine.statusMessage
            csvExportResultDialog.open()
        }
    }

    MessageDialog {
        id: geoJsonExportResultDialog
        buttons: MessageDialog.Ok
    }

    MessageDialog {
        id: csvExportResultDialog
        buttons: MessageDialog.Ok
    }

    ColumnLayout {

        anchors.fill: parent
        spacing: 0

        // =====================================================
        // HEADER
        // =====================================================

        Rectangle {

            Layout.fillWidth: true
            height: 76

            color: "#111e29"
            z: 100

            RowLayout {

                anchors.fill: parent

                anchors.leftMargin: 32
                anchors.rightMargin: 32

                Text {

                    text: "AIRSPACE SCENARIO GENERATOR"

                    color: "#ffffff"

                    Layout.alignment: Qt.AlignVCenter
                    font.pixelSize: 26
                    font.bold: true
                }
            }
        }

        // =====================================================
        // PROMPT AREA
        // =====================================================

        Rectangle {

            Layout.fillWidth: true
            height: 142

            color: "#0b1822"
            z: 100

            ColumnLayout {

                anchors.fill: parent

                anchors.leftMargin: 32
                anchors.rightMargin: 32
                anchors.topMargin: 14
                anchors.bottomMargin: 14

                spacing: 8

                Text {

                    text: "ENTER SCENARIO PROMPT"

                    color: "#8fa8ba"

                    font.pixelSize: 13
                    font.bold: true
                }

                RowLayout {

                    Layout.fillWidth: true

                    spacing: 12

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
                        enabled: !window.aiRequestPending

                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 55

                        z: 200

                        contentItem: Text {

                            text: window.aiRequestPending ? "ASKING LOCAL AI..." : generateButton.text

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
                            window.showingAirspace3D = false
                            window.showingAircraft3D = false
                            console.log("BUTTON CLICK RECEIVED")
                            window.generateScenario()
                        }
                    }

                    Button {
                        id: exportGeoJsonButton

                        text: "EXPORT GEOJSON"
                        visible: window.scenarioGenerated
                                 || scenarioEngine.commandHistoryCount > 0

                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 55

                        onClicked: {
                            console.info("[GEOJSON EXPORT] Button action received")
                            geoJsonSaveDialog.open()
                        }
                    }

                    Button {
                        id: exportCsvButton

                        text: "EXPORT CSV"
                        visible: window.scenarioGenerated
                                 || scenarioEngine.commandHistoryCount > 0

                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 55

                        onClicked: {
                            console.info("[CSV EXPORT] Button action received")
                            csvSaveDialog.open()
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    text: "STATUS: " + window.statusMessage
                    color: window.statusMessage.startsWith("GEOJSON EXPORT SUCCESS")
                           || window.statusMessage.startsWith("CSV EXPORT SUCCESS")
                           || window.statusMessage.startsWith("OLLAMA GENERATED JSON")
                           ? "#8de1a4"
                           : window.statusMessage.indexOf("DETERMINISTIC PARSER EXECUTED") >= 0
                             ? "#ffcc66"
                             : window.statusMessage.indexOf("FAILED") >= 0
                               || window.statusMessage.indexOf("REJECTED") >= 0
                               ? "#ff8b82"
                               : "#b8c5cc"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
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

            ScenarioMap {
                id: scenarioMap
                anchors.fill: parent
                visible: !window.showingAircraft3D && !window.showingAirspace3D
            }

        }
    }

    AirspaceScene {
        id: airspaceScene
        anchors.fill: parent
        visible: window.showingAirspace3D
        z: 1000

        onAircraftSelected: function(entityId) {
            if (scenarioEngine.selectAircraft(entityId)) {
                window.showingAirspace3D = false
                window.showingAircraft3D = true
            }
        }

        onBackRequested: {
            window.showingAirspace3D = false
        }
    }

    Aircraft3DView {
        anchors.fill: parent
        visible: window.showingAircraft3D
        z: 1000
        aircraftId: scenarioEngine.selectedEntityId
        aircraftName: scenarioEngine.selectedEntityName
        aircraftLatitude: scenarioEngine.selectedAircraftLatitude
        aircraftLongitude: scenarioEngine.selectedAircraftLongitude
        aircraftAltitude: scenarioEngine.selectedAircraftAltitude
        aircraftSpeed: scenarioEngine.selectedAircraftSpeed
        aircraftHeading: scenarioEngine.selectedAircraftHeading
        aircraftVerticalSpeed: scenarioEngine.selectedAircraftVerticalSpeed
        aircraftWaypoint: scenarioEngine.selectedAircraftNextWaypoint
        referenceLatitude: scenarioEngine.centerLatitude
        referenceLongitude: scenarioEngine.centerLongitude

        onBackRequested: {
            window.showingAircraft3D = false
            scenarioEngine.clearSelectedAircraft()
        }
    }
}