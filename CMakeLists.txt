# qt_head: Qt Quick/QML user interface for the guitarix engine.
#
# Standalone CMake project, independent of the waf build. It talks to a
# guitarix engine started with "guitarix -N -p PORT" over JSON-RPC and
# does not link against any guitarix library.

cmake_minimum_required(VERSION 3.21)

project(qt_head VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 6.4 is what Debian 12 / Raspberry Pi OS Bookworm ship
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)

qt_standard_project_setup()

# qt_policy() does not exist in Qt 6.4
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(qt_head
    src/main.cpp
    src/rpcclient.cpp
    src/rpcclient.h
    src/parameter.cpp
    src/parameter.h
    src/machine.cpp
    src/machine.h
    src/uitree.cpp
    src/uitree.h
    src/tuner.cpp
    src/tuner.h
    src/midimap.cpp
    src/midimap.h
    src/presets.cpp
    src/files.cpp
    src/dirmodel.cpp
    src/dirmodel.h
)

# must be set before qt_add_qml_module
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(qt_head
    URI QtHead
    VERSION 1.0
    # explicit prefix: the default changed in Qt 6.5 (policy QTP0001)
    RESOURCE_PREFIX /
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/TopBar.qml
        qml/PresetPanel.qml
        qml/Rack.qml
        qml/RackUnit.qml
        qml/AmpPanel.qml
        qml/UiNode.qml
        qml/Spacer.qml
        qml/Control.qml
        qml/Knob.qml
        qml/ParamSwitch.qml
        qml/ParamSelector.qml
        qml/ParamSpin.qml
        qml/ParamSlider.qml
        qml/LevelMeter.qml
        qml/AddUnitDialog.qml
        qml/ConnectDialog.qml
        qml/MidiLearnArea.qml
        qml/MidiLearnDialog.qml
        qml/MidiMapDialog.qml
        qml/TunerView.qml
        qml/TextInputDialog.qml
        qml/ConfirmDialog.qml
        qml/SavePresetDialog.qml
        qml/FileIcon.qml
        qml/FileChooserDialog.qml
        qml/FileButton.qml
)

target_include_directories(qt_head PRIVATE src)

target_compile_definitions(qt_head PRIVATE
    QT_HEAD_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(qt_head PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS qt_head
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES qt_head.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
