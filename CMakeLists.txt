cmake_minimum_required(VERSION 3.24)
project(ShineShift VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_program(ZIG_EXECUTABLE zig REQUIRED)
find_package(Qt6 6.10 REQUIRED COMPONENTS Quick Qml QuickControls2 DBus)
# 6.6 added setScreen and setWantsToBeOnActiveScreen, and it needs Qt 6.10 itself.
find_package(LayerShellQt 6.6 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DDCUTIL REQUIRED IMPORTED_TARGET ddcutil>=2.2.1)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(CORE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/zig-out")
set(CORE_LIBRARY "${CORE_PREFIX}/lib/libshineshift_core.a")
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/*.zig src/core.h)
add_custom_command(
    OUTPUT "${CORE_LIBRARY}"
    COMMAND "${CMAKE_COMMAND}" -E env "ZIG_GLOBAL_CACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/zig-cache"
        "${ZIG_EXECUTABLE}" build -Doptimize=ReleaseFast --prefix "${CORE_PREFIX}"
        --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-local-cache"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS build.zig ${CORE_SOURCES}
    VERBATIM
)
add_custom_target(shineshift_core_build DEPENDS "${CORE_LIBRARY}")
add_library(shineshift_core STATIC IMPORTED GLOBAL)
set_target_properties(shineshift_core PROPERTIES IMPORTED_LOCATION "${CORE_LIBRARY}")
add_dependencies(shineshift_core shineshift_core_build)

set(QML_SOURCES
    ui/App.qml
    ui/Theme.qml
    ui/FluentIcon.qml
    ui/IconButton.qml
    ui/Tip.qml
    ui/Segmented.qml
    ui/Toggle.qml
    ui/Dropdown.qml
    ui/Field.qml
    ui/Chip.qml
    ui/Spinner.qml
    ui/Scroller.qml
    ui/WheelScroll.qml
    ui/ColorPicker.qml
    ui/Surface.qml
    ui/LevelSlider.qml
    ui/MonitorCard.qml
    ui/Flyout.qml
    ui/LevelOverlay.qml
    ui/SettingsWindow.qml
    ui/icons/Icons.js
)
foreach(source IN LISTS QML_SOURCES)
    get_filename_component(alias "${source}" NAME)
    set_source_files_properties("${source}" PROPERTIES QT_RESOURCE_ALIAS "${alias}")
endforeach()
set_source_files_properties(ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(shineshift
    app/main.cpp
    app/displays.cpp
    app/displays.h
    app/instance.h
    app/settings.cpp
    app/settings.h
    app/shell.cpp
    app/shell.h
    app/themesource.cpp
    app/themesource.h
    app/tray.cpp
    app/tray.h
    app/worker.cpp
    app/worker.h
)
qt_add_qml_module(shineshift
    URI ShineShift
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
)
add_dependencies(shineshift shineshift_core_build)
target_include_directories(shineshift PRIVATE src app)
target_compile_definitions(shineshift PRIVATE SHINESHIFT_VERSION="${PROJECT_VERSION}")
target_link_libraries(shineshift PRIVATE
    shineshift_core Qt6::Quick Qt6::Qml Qt6::QuickControls2 Qt6::DBus LayerShellQt::Interface PkgConfig::DDCUTIL)
install(TARGETS shineshift RUNTIME DESTINATION bin)
install(FILES data/net.electris.ShineShift.desktop DESTINATION share/applications)
install(FILES data/net.electris.ShineShift.svg DESTINATION share/icons/hicolor/scalable/apps)
