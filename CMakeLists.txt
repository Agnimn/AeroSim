cmake_minimum_required(VERSION 3.16)

project(AirspaceMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    Quick3D
    Quick3DAssetUtils
    Quick3DHelpers
    Location
    Positioning
    Network
    Sql
    Test
)

qt_standard_project_setup(REQUIRES 6.10)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appAirspaceMonitor
    main.cpp
    core/Entity.h
    core/Entity.cpp
    core/Scenario.h
    core/Scenario.cpp
    core/Route.h
    core/Route.cpp
    core/CommandHistoryEntry.h
    core/SimulationState.h
    core/FormationGenerator.h
    core/FormationGenerator.cpp
    core/FormationController.h
    core/FormationController.cpp
    core/SimulationEngine.h
    core/SimulationEngine.cpp
    core/ScenarioRequest.h
    core/TerrainData.h
    core/TerrainData.cpp
    core/ScenarioEngine.h
    core/ScenarioEngine.cpp
    ai/AIParser.h
    ai/AIParser.cpp
    ai/AICommand.h
    ai/AICommand.cpp
    ai/OllamaClient.h
    ai/OllamaClient.cpp
    data/ScenarioRepository.h
    data/ScenarioRepository.cpp
    data/ScenarioDatabase.h
    data/ScenarioDatabase.cpp
    data/GeoJsonExporter.h
    data/GeoJsonExporter.cpp
    data/CsvExporter.h
    data/CsvExporter.cpp
    data/ScenarioDatabase.h
    data/ScenarioDatabase.cpp
    data/AirbaseRegistry.h
    data/AirbaseRegistry.cpp
    models/EntityModel.h
    models/EntityModel.cpp
)

set_source_files_properties(qml/Aircraft3DView.qml PROPERTIES
    QT_RESOURCE_ALIAS Aircraft3DView.qml
)

qt_add_qml_module(appAirspaceMonitor
    URI AirspaceMonitor
    VERSION 1.0

    QML_FILES
        Main.qml
        qml/Aircraft3DView.qml
        qml/ScenarioMap.qml
        qml/components/AirspaceScene.qml
        qml/components/AirspaceTerrain.qml
        qml/components/AirspaceAircraft.qml
        qml/components/FlightPath3D.qml
        qml/components/ThreatZone3D.qml
        qml/components/RadarRing3D.qml
        qml/components/Airport3D.qml
        qml/components/AirspaceHud.qml
        qml/components/GeoCoordinateConverter.qml
)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets/aircraft/aircraft.glb")
    qt_add_resources(appAirspaceMonitor aircraft_model_resources
        PREFIX "/AeroSim"
        FILES assets/aircraft/aircraft.glb
    )
endif()

qt_add_resources(appAirspaceMonitor terrain_heightmap_resources
    PREFIX "/AeroSim"
    FILES
        assets/terrain/test_heightmap.png
        assets/terrain/airspace_heightmap.png
)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets/terrain/north_india_dem.png" AND
    EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets/terrain/north_india_dem.json")
    qt_add_resources(appAirspaceMonitor north_india_dem_resources
        PREFIX "/AeroSim"
        FILES
            assets/terrain/north_india_dem.png
            assets/terrain/north_india_dem.json
    )
endif()

set_target_properties(appAirspaceMonitor PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.airspacemonitor
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appAirspaceMonitor
    PRIVATE
        Qt6::Quick
        Qt6::Quick3D
        Qt6::Quick3DAssetUtils
        Qt6::Quick3DHelpers
        Qt6::Location
        Qt6::Positioning
        Qt6::Network
        Qt6::Sql
)


include(GNUInstallDirs)

install(TARGETS appAirspaceMonitor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_add_resources(appAirspaceMonitor airbase_registry_resources
    PREFIX "/AeroSim"
    FILES assets/airbases/airbases.json
)

enable_testing()

qt_add_executable(airbaseTests
    tests/airbase_tests.cpp
    core/Entity.h
    core/Entity.cpp
    core/Scenario.h
    core/Scenario.cpp
    core/Route.h
    core/Route.cpp
    core/CommandHistoryEntry.h
    core/SimulationState.h
    core/FormationGenerator.h
    core/FormationGenerator.cpp
    core/FormationController.h
    core/FormationController.cpp
    core/SimulationEngine.h
    core/SimulationEngine.cpp
    core/ScenarioRequest.h
    core/ScenarioEngine.h
    core/ScenarioEngine.cpp
    ai/AIParser.h
    ai/AIParser.cpp
    ai/AICommand.h
    ai/AICommand.cpp
    ai/OllamaClient.h
    ai/OllamaClient.cpp
    data/AirbaseRegistry.h
    data/AirbaseRegistry.cpp
    data/ScenarioRepository.h
    data/ScenarioRepository.cpp
    data/ScenarioDatabase.h
    data/ScenarioDatabase.cpp
    data/GeoJsonExporter.h
    data/GeoJsonExporter.cpp
    data/CsvExporter.h
    data/CsvExporter.cpp
    models/EntityModel.h
    models/EntityModel.cpp
)

qt_add_resources(airbaseTests airbase_test_registry_resources
    PREFIX "/AeroSim"
    FILES assets/airbases/airbases.json
)

target_link_libraries(airbaseTests
    PRIVATE
        Qt6::Core
        Qt6::Positioning
        Qt6::Network
        Qt6::Sql
        Qt6::Test
)

add_test(NAME airbaseTests COMMAND airbaseTests)