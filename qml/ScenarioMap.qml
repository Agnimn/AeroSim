import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtLocation
import QtPositioning

Map {
    id: scenarioMap

    property bool showAircraftTrails: true
    property bool showAirbases: true

    function formatSimulationTime(seconds) {
        var totalSeconds = Math.max(0, Math.floor(seconds))
        var hours = Math.floor(totalSeconds / 3600) % 24
        var minutes = Math.floor(totalSeconds / 60) % 60
        var paddedHours = hours < 10 ? "0" + hours : hours
        var paddedMinutes = minutes < 10 ? "0" + minutes : minutes
        return paddedHours + ":" + paddedMinutes
    }

    plugin: mapPlugin
    activeMapType: supportedMapTypes.length > 0
                   ? supportedMapTypes[supportedMapTypes.length - 1]
                   : null
        center: scenarioGenerated
            ? QtPositioning.coordinate(scenarioEngine.centerLatitude,
                           scenarioEngine.centerLongitude)
            : QtPositioning.coordinate(29.75, 77.0)
        zoomLevel: scenarioGenerated &&
               (region === "North India" || region === "South India" ||
                region === "East India" || region === "West India")
               ? 5.5 : 6
    copyrightsVisible: true
    visible: !window.showingAircraft3D
    z: 0

    DragHandler {
        id: mapPanHandler
        target: null
        acceptedDevices: PointerDevice.Mouse
        acceptedButtons: Qt.LeftButton
        grabPermissions: PointerHandler.CanTakeOverFromItems

        property point previousTranslation: Qt.point(0, 0)

        onActiveChanged: {
            if (active)
                previousTranslation = Qt.point(0, 0)
        }

        onTranslationChanged: {
            if (!active)
                return

            var deltaX = translation.x - previousTranslation.x
            var deltaY = translation.y - previousTranslation.y
            previousTranslation = Qt.point(translation.x, translation.y)
            if (deltaX === 0 && deltaY === 0)
                return

            var mapCenterPoint = Qt.point(scenarioMap.width / 2,
                                          scenarioMap.height / 2)
            var targetPoint = Qt.point(mapCenterPoint.x - deltaX,
                                       mapCenterPoint.y - deltaY)
            var targetCoordinate = scenarioMap.toCoordinate(targetPoint, false)
            if (targetCoordinate.isValid)
                scenarioMap.center = targetCoordinate
        }
    }

    WheelHandler {
        id: wheelHandler
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

        onWheel: function(event) {
            if (event.angleDelta.y > 0)
                scenarioMap.zoomLevel += 0.5
            else
                scenarioMap.zoomLevel -= 0.5
        }
    }

    TapHandler {
        id: doubleClickZoomHandler
        acceptedDevices: PointerDevice.Mouse
        acceptedButtons: Qt.LeftButton

        onDoubleTapped: {
            scenarioMap.zoomLevel = Math.min(scenarioMap.maximumZoomLevel,
                                             scenarioMap.zoomLevel + 1)
        }
    }

    PinchHandler {
        id: pinchHandler
        target: null

        property var startCentroid: QtPositioning.coordinate(29.0, 79.0)

        onActiveChanged: {
            if (active)
                startCentroid = scenarioMap.toCoordinate(centroid.position, false)
        }

        onScaleChanged: function(delta) {
            scenarioMap.zoomLevel += Math.log2(delta)
            scenarioMap.alignCoordinateToPoint(startCentroid, centroid.position)
        }
    }

    MapItemView {
        id: airbaseMapView
        model: scenarioEngine.airbaseModel

        delegate: MapQuickItem {
            id: airbaseMarker
            visible: scenarioMap.showAirbases && model.coordinateAvailable &&
                     (scenarioEngine.airbaseDisplayRegion === "" ||
                      scenarioEngine.airbaseDisplayRegion === "India" ||
                      scenarioEngine.airbaseDisplayRegion === model.region)
            coordinate: QtPositioning.coordinate(model.latitude, model.longitude)
            anchorPoint.x: airbaseIcon.width / 2
            anchorPoint.y: airbaseIcon.height / 2
            zoomLevel: 0
            z: scenarioEngine.selectedAirbaseId === model.airbaseId ? 180 : 90

            sourceItem: Item {
                id: airbaseIcon
                width: 42
                height: 58

                Rectangle {
                    width: 34
                    height: 34
                    radius: 17
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: scenarioEngine.selectedAirbaseId === model.airbaseId ? "#f4b942" : "#d95763"
                    border.width: scenarioEngine.selectedAirbaseId === model.airbaseId ? 4 : 2
                    border.color: "#ffffff"

                    Text {
                        anchors.centerIn: parent
                        text: "◆"
                        color: "#ffffff"
                        font.pixelSize: 19
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: scenarioEngine.selectAirbase(model.airbaseId)
                    }
                }

                Text {
                    visible: scenarioMap.zoomLevel >= 7 || scenarioEngine.selectedAirbaseId === model.airbaseId
                    anchors.top: parent.top
                    anchors.topMargin: 36
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: model.airbaseName
                    color: "#ffffff"
                    font.pixelSize: 10
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#111e29"
                }
            }
        }
    }

    MapItemView {
        model: scenarioEngine.entityModel

        delegate: MapPolyline {
            visible: model.entityType === "aircraft" && model.route.length > 1
            path: model.route
            line.width: scenarioEngine.selectedEntityId === model.id ? 2 : 1
            line.color: scenarioEngine.selectedEntityId === model.id
                        ? "#72c9ff"
                        : ["#46b3ff", "#f2b84b", "#69d38a", "#d58cff", "#ff7d6e"][index % 5]
            z: 30
        }
    }

    MapItemView {
        model: scenarioEngine.routes

        delegate: MapPolyline {
            path: modelData.path
            line.width: 3
            line.color: "#ffcc66"
            z: 35
        }
    }

    MapItemView {
        model: scenarioEngine.routeWaypoints

        delegate: MapQuickItem {
            coordinate: QtPositioning.coordinate(modelData.latitude,
                                                 modelData.longitude)
            anchorPoint.x: waypointMarker.width / 2
            anchorPoint.y: waypointMarker.height / 2
            z: modelData.isStart || modelData.isEnd ? 85 : 80

            sourceItem: Item {
                id: waypointMarker
                width: 52
                height: 48

                Rectangle {
                    width: 20
                    height: 20
                    radius: modelData.isStart || modelData.isEnd ? 10 : 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: modelData.isStart ? "#69d38a"
                           : modelData.isEnd ? "#d95763" : "#f2b84b"
                    border.width: 2
                    border.color: "#ffffff"
                }

                Text {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.label + " " + modelData.name
                    color: "#ffffff"
                    font.pixelSize: 10
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#111e29"
                }
            }
        }
    }

    MapItemView {
        model: scenarioEngine.entityModel

        delegate: MapPolyline {
            visible: scenarioMap.showAircraftTrails &&
                     model.entityType === "aircraft" &&
                     model.trail.length > 1
            path: model.trail
            line.width: 1
            line.color: "#567184"
            z: 40
        }
    }

    MapItemView {
        model: scenarioEngine.entityModel

        delegate: MapCircle {
            visible: model.entityType === "aircraft" &&
                     model.conflictStatus !== "NORMAL"
            center: QtPositioning.coordinate(model.conflictLatitude,
                                             model.conflictLongitude)
            radius: scenarioEngine.conflictZoneRadiusMeters
            color: model.conflictStatus === "CONFLICT"
                   ? "#35f04444"
                   : "#35f2c94c"
            border.width: 2
            border.color: model.conflictStatus === "CONFLICT"
                          ? "#f04444"
                          : "#f2c94c"
            z: 50
        }
    }

    MapItemView {
        id: entityMapView
        parent: scenarioMap
        model: scenarioEngine.entityModel

        delegate: MapQuickItem {
            id: entityMarker
            coordinate: QtPositioning.coordinate(model.latitude, model.longitude)
            anchorPoint.x: marker.width / 2
            anchorPoint.y: marker.height / 2
            zoomLevel: 0
            autoFadeIn: false
            z: 100

            sourceItem: Item {
                id: marker
                width: 70
                height: 85

                property real labelAngle: (index * 137.507764) % 360
                property real labelDistance: 38 + (index % 3) * 9

                Rectangle {
                    id: aircraftCircle
                    width: 58
                    height: 58
                    radius: 29
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: model.entityType === "aircraft"
                           ? model.conflictStatus === "CONFLICT" ? "#d64545"
                             : model.conflictStatus === "CAUTION" ? "#d8a92e" : "#0b8fce"
                           : model.entityType === "radar_site" ? "#f39c12" : "#16a085"
                    border.color: model.entityType !== "aircraft" ? "#ffffff"
                                  : scenarioEngine.selectedEntityId === model.id ? "#ffffff"
                                  : model.conflictStatus === "CONFLICT" ? "#f04444"
                                  : model.conflictStatus === "CAUTION" ? "#f2c94c"
                                  : "#4cd964"
                    border.width: scenarioEngine.selectedEntityId === model.id ? 5 : 4

                    Canvas {
                        id: aircraftGlyph
                        visible: model.entityType === "aircraft"
                        anchors.centerIn: parent
                        width: 34
                        height: 38
                        rotation: model.entityType === "aircraft" ? model.heading : 0

                        onPaint: {
                            var context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.fillStyle = "#ffffff"
                            context.beginPath()
                            context.moveTo(17, 1)
                            context.lineTo(21, 15)
                            context.lineTo(32, 21)
                            context.lineTo(32, 24)
                            context.lineTo(21, 21)
                            context.lineTo(21, 31)
                            context.lineTo(26, 34)
                            context.lineTo(26, 36)
                            context.lineTo(17, 33)
                            context.lineTo(8, 36)
                            context.lineTo(8, 34)
                            context.lineTo(13, 31)
                            context.lineTo(13, 21)
                            context.lineTo(2, 24)
                            context.lineTo(2, 21)
                            context.lineTo(13, 15)
                            context.closePath()
                            context.fill()
                        }
                    }

                    Text {
                        visible: model.entityType !== "aircraft"
                        anchors.centerIn: parent
                        text: model.entityType === "radar_site" ? "📡" : model.entityType === "ship" ? "⛴" : "◆"
                        color: "#ffffff"
                        font.pixelSize: model.entityType === "radar_site" ? 22 : 29
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (model.entityType === "aircraft")
                                scenarioEngine.selectAircraft(model.id)
                        }
                    }
                }

                Rectangle {
                    id: labelLeader
                    x: marker.width / 2 - width / 2
                    y: marker.height / 2 - 1
                    width: marker.labelDistance
                    height: 1
                    color: "#b6d8e8"
                    opacity: 0.7
                    transformOrigin: Item.Left
                    rotation: marker.labelAngle
                }

                Rectangle {
                    id: entityLabelBackground
                    x: marker.width / 2 + Math.cos(marker.labelAngle * Math.PI / 180) * marker.labelDistance - width / 2
                    y: marker.height / 2 + Math.sin(marker.labelAngle * Math.PI / 180) * marker.labelDistance - height / 2
                    width: entityLabel.width + 14
                    height: 22
                    radius: 5
                    color: "#111e29"
                    opacity: 0.95

                    Text {
                        id: entityLabel
                        anchors.centerIn: parent
                        text: model.entityType === "aircraft"
                            ? model.callsign + (model.lifecycle !== "Airborne"
                                          ? "  [" + model.lifecycle + "]" : "")
                            : model.name
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

            }
        }
    }

    Rectangle {
        visible: scenarioGenerated
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 16
        width: simulationControls.implicitWidth + 30
        height: 58
        radius: 9
        color: "#e6111e29"
        border.color: "#29485c"
        z: 300

        RowLayout {
            id: simulationControls
            anchors.centerIn: parent
            spacing: 8

            Button {
                text: scenarioEngine.simulationRunning ? "⏸ PAUSE" : "▶ PLAY"
                onClicked: scenarioEngine.simulationRunning
                           ? scenarioEngine.pauseSimulation()
                           : scenarioEngine.playSimulation()
            }

            Button {
                text: "↻ RESET"
                onClicked: scenarioEngine.resetSimulation()
            }

            Text {
                text: "SIM"
                color: "#9fb6c5"
                font.bold: true
            }

            ComboBox {
                id: simulationSpeedSelector
                model: ["0.5x", "1x", "2x", "5x", "10x"]
                currentIndex: Math.max(0, model.indexOf(scenarioEngine.simulationSpeed + "x"))
                onActivated: scenarioEngine.setSimulationSpeed(
                                 Number(currentText.slice(0, -1)))
            }

            CheckBox {
                text: "Trails"
                checked: scenarioMap.showAircraftTrails
                onToggled: scenarioMap.showAircraftTrails = checked
            }

            CheckBox {
                text: "Airbases"
                checked: scenarioMap.showAirbases
                onToggled: scenarioMap.showAirbases = checked
            }
        }

    }

    Rectangle {
        visible: scenarioEngine.selectedAirbaseId !== ""
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 25
        anchors.topMargin: 25
        width: 290
        height: 185
        radius: 10
        color: "#ed111e29"
        border.color: "#d95763"
        z: 300

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 5

            Text {
                text: scenarioEngine.selectedAirbaseName
                color: "#ffffff"
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
                width: parent.width
            }
            Text { text: "CATEGORY  " + scenarioEngine.selectedAirbaseCategory; color: "#cbd8df" }
            Text { text: "STATE/UT  " + scenarioEngine.selectedAirbaseState; color: "#cbd8df" }
            Text { text: "REGION    " + scenarioEngine.selectedAirbaseRegion; color: "#cbd8df" }
            Text { text: "STATUS    " + scenarioEngine.selectedAirbaseVerificationStatus; color: "#cbd8df" }
            Button {
                text: "CLEAR AIRBASE"
                onClicked: scenarioEngine.clearSelectedAirbase()
            }
        }
    }

    Rectangle {
        visible: scenarioEngine.selectedEntityId !== ""
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 25
        anchors.bottomMargin: 25
        width: 350
        height: 425
        radius: 10
        color: "#ed111e29"
        border.color: scenarioEngine.selectedAircraftConflictStatus === "CONFLICT"
                      ? "#f04444"
                      : scenarioEngine.selectedAircraftConflictStatus === "CAUTION"
                        ? "#f2c94c" : "#29485c"
        z: 300

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 5

            Text {
                text: scenarioEngine.selectedAircraftCallsign + "  •  " +
                      scenarioEngine.selectedEntityId
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
            }
            Text { text: "TYPE   " + scenarioEngine.aircraftInformation(scenarioEngine.selectedEntityId).aircraftType; color: "#9fb6c5" }
            Text {
                text: "POSITION  " + scenarioEngine.selectedAircraftLatitude.toFixed(4)
                      + ", " + scenarioEngine.selectedAircraftLongitude.toFixed(4)
                color: "#9fb6c5"
            }
            Text { text: "ALT     " + Math.round(scenarioEngine.selectedAircraftAltitude) + " ft"; color: "#9fb6c5" }
            Text { text: "SPD    " + Math.round(scenarioEngine.selectedAircraftSpeed) + " kt"; color: "#9fb6c5" }
            Text { text: "HDG   " + Math.round(scenarioEngine.selectedAircraftHeading) + "°"; color: "#9fb6c5" }
            Text { text: "VS      " + Math.round(scenarioEngine.selectedAircraftVerticalSpeed) + " ft/min"; color: "#9fb6c5" }
            Text { text: "NOW  " + scenarioEngine.selectedAircraftCurrentWaypoint; color: "#9fb6c5" }
            Text { text: "NEXT " + scenarioEngine.selectedAircraftNextWaypoint; color: "#9fb6c5" }
            Text {
                text: "DESTINATION  " + (scenarioEngine.selectedAircraftDestination || "UNASSIGNED")
                color: "#9fb6c5"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Text {
                text: "MOVEMENT  " + scenarioEngine.selectedAircraftMovementStatus
                      + "  •  " + scenarioEngine.selectedAircraftConflictStatus
                color: scenarioEngine.selectedAircraftConflictStatus === "CONFLICT"
                       ? "#ff7070" : scenarioEngine.selectedAircraftConflictStatus === "CAUTION"
                       ? "#f2c94c" : scenarioEngine.selectedAircraftMovementStatus === "MOVING"
                       ? "#72c9ff" : "#4cd964"
                font.bold: true
            }

            TextField {
                id: aircraftDestinationInput
                Layout.fillWidth: true
                placeholderText: "Destination (e.g. Pune Airbase)"
                color: "#ffffff"
                placeholderTextColor: "#718596"
                background: Rectangle {
                    color: "#162733"
                    radius: 6
                    border.color: "#29485c"
                    border.width: 1
                }
                onAccepted: scenarioEngine.moveSelectedAircraftToDestination(text)
            }
            Button {
                text: "SET DESTINATION / START"
                Layout.fillWidth: true
                enabled: aircraftDestinationInput.text.trim().length > 0
                onClicked: scenarioEngine.moveSelectedAircraftToDestination(
                               aircraftDestinationInput.text)
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                Button {
                    text: "START"
                    Layout.fillWidth: true
                    enabled: scenarioEngine.selectedAircraftMovementStatus === "STATIONARY"
                    onClicked: scenarioEngine.startSelectedAircraft()
                }
                Button {
                    text: "PAUSE"
                    Layout.fillWidth: true
                    enabled: scenarioEngine.selectedAircraftMovementStatus === "MOVING"
                    onClicked: scenarioEngine.pauseSelectedAircraft()
                }
                Button {
                    text: "RESUME"
                    Layout.fillWidth: true
                    enabled: scenarioEngine.selectedAircraftMovementStatus === "PAUSED"
                    onClicked: scenarioEngine.resumeSelectedAircraft()
                }
                Button {
                    text: "STOP"
                    Layout.fillWidth: true
                    enabled: scenarioEngine.selectedAircraftMovementStatus === "MOVING" ||
                             scenarioEngine.selectedAircraftMovementStatus === "PAUSED"
                    onClicked: scenarioEngine.stopSelectedAircraft()
                }
            }
            Button {
                text: "OPEN AIRCRAFT 3D"
                Layout.fillWidth: true
                onClicked: window.showingAircraft3D = true
            }
        }
    }

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

    Rectangle {
        id: scenarioStatusPanel
        visible: scenarioGenerated
        width: 280
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 25
        anchors.bottomMargin: 25
        color: "#111e29"
        radius: 10
        border.color: "#29485c"
        z: 200

        Column {
            id: scenarioStatusColumn
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
                text: "Region : " + region
                color: "#9fb6c5"
                font.pixelSize: 14
            }

            Text {
                text: "Entity Type : " + entityType
                color: "#9fb6c5"
                font.pixelSize: 14
            }

            Text {
                text: "Entities : " + entityCount
                color: "#9fb6c5"
                font.pixelSize: 14
            }

            Text {
                text: "Simulation time : " + scenarioMap.formatSimulationTime(scenarioEngine.simulationTime)
                color: "#9fb6c5"
                font.pixelSize: 14
            }

            Text {
                visible: scenarioEngine.selectedAirbaseId !== ""
                text: "Airbase : " + scenarioEngine.selectedAirbaseName
                color: "#9fb6c5"
                font.pixelSize: 14
                elide: Text.ElideRight
                width: parent.width
            }

            Text {
                visible: scenarioEngine.scheduledTakeoffs.length > 0
                text: "SCHEDULED EVENTS:\n" + scenarioEngine.scheduledTakeoffs.map(function(event) {
                    return event.aircraftId + "  " + event.time + "  " + event.status
                           + "\n  " + (event.origin || "Current position")
                           + " -> " + (event.destination || "Existing route")
                           + "  [" + event.lifecycle + "]"
                }).join("\n")
                color: "#f5c26b"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                width: parent.width
            }

            Text {
                text: "MAP : " + (mapboxConfigured ? "MAPBOX" : "OPENSTREETMAP")
                color: "#45aaf2"
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                text: "STATUS : " + statusMessage
                color: statusMessage === "GENERATED" ||
                       statusMessage.indexOf("GENERATED:") === 0 ||
                       statusMessage.indexOf("DETERMINISTIC PARSER EXECUTED: GENERATED") >= 0 ||
                      statusMessage.indexOf("TAKE-OFF SCHEDULED") === 0 ||
                      statusMessage.indexOf("TAKE-OFF CANCELLED") === 0
                      ? "#4cd964" : "#f5a623"
                font.pixelSize: 14
                font.bold: true
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }

        height: scenarioStatusColumn.implicitHeight + 40
    }

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
            text: "Enter a scenario prompt to generate the 2D view"
            color: "#9fb6c5"
            font.pixelSize: 16
        }
    }

    Rectangle {
        visible: !mapboxConfigured || scenarioMap.error !== Map.NoError
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
                text: !mapboxConfigured
                      ? mapboxConfigurationError
                      : "Map tiles could not be loaded. Check your network connection "
                        + "and map configuration."
                color: "#ffffff"
                font.pixelSize: 13
                width: 500
                wrapMode: Text.WordWrap
            }
        }
    }

}
