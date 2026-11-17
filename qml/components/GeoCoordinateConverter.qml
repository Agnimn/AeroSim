import QtQuick

QtObject {
    id: converter

    property real originLatitude: 0
    property real originLongitude: 0
    property real metersPerSceneUnit: 1500
    property real verticalExaggeration: 10

    function toLocal(latitude, longitude, altitudeFeet) {
        var meanLatitudeRadians = (latitude + converter.originLatitude) * Math.PI / 360
        var metersPerDegreeLatitude = 111320
        var metersPerDegreeLongitude = metersPerDegreeLatitude * Math.cos(meanLatitudeRadians)
        var eastMeters = (longitude - converter.originLongitude) * metersPerDegreeLongitude
        var northMeters = (latitude - converter.originLatitude) * metersPerDegreeLatitude
        var altitudeMeters = altitudeFeet * 0.3048 * converter.verticalExaggeration
        return Qt.vector3d(eastMeters / converter.metersPerSceneUnit,
                           altitudeMeters / converter.metersPerSceneUnit,
                           -northMeters / converter.metersPerSceneUnit)
    }
}
