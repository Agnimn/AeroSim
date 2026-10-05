cmake_minimum_required(VERSION 3.16)

project(AirspaceMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    Location
    Positioning
)

qt_standard_project_setup(REQUIRES 6.10)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appAirspaceMonitor
    main.cpp
)

qt_add_qml_module(appAirspaceMonitor
    URI AirspaceMonitor
    VERSION 1.0

    QML_FILES
        Main.qml
)

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
        Qt6::Location
        Qt6::Positioning
)

include(GNUInstallDirs)

install(TARGETS appAirspaceMonitor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)