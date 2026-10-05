cmake_minimum_required(VERSION 3.16)

project(AirspaceMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    Quick3D
    Quick3DAssetUtils
    Location
    Positioning
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
    core/ScenarioRequest.h
    core/ScenarioEngine.h
    core/ScenarioEngine.cpp
    ai/AIParser.h
    ai/AIParser.cpp
    data/ScenarioRepository.h
    data/ScenarioRepository.cpp
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
)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets/aircraft/aircraft.glb")
    qt_add_resources(appAirspaceMonitor aircraft_model_resources
        PREFIX "/AeroSim"
        FILES assets/aircraft/aircraft.glb
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
        Qt6::Location
        Qt6::Positioning
)

include(GNUInstallDirs)

install(TARGETS appAirspaceMonitor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)