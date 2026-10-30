# guitariqx: Qt Quick/QML user interface for the guitarix engine.
#
# Standalone CMake project, independent of the waf build. It talks to a
# guitarix engine started with "guitarix -N -p PORT" over JSON-RPC and
# does not link against any guitarix library.

cmake_minimum_required(VERSION 3.21)

project(guitariqx VERSION 0.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 6.4 is what Debian 12 / Raspberry Pi OS Bookworm ship
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)

qt_standard_project_setup()

# qt_policy() does not exist in Qt 6.4
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# everything but main(): also compiled into the tests (tests/CMakeLists.txt)
set(GUITARIQX_SOURCES
    src/rpcclient.cpp
    src/rpcclient.h
    src/parameter.cpp
    src/parameter.h
    src/machine.cpp
    src/machine.h
    src/uitree.cpp
    src/uitree.h
    src/glade.cpp
    src/tuner.cpp
    src/tuner.h
    src/midimap.cpp
    src/midimap.h
    src/presets.cpp
    src/bankfiles.cpp
    src/files.cpp
    src/sequencer.cpp
    src/unitpresets.cpp
    src/dirmodel.cpp
    src/dirmodel.h
    src/skin.cpp
    src/skin.h
    src/oscilloscope.cpp
    src/oscilloscope.h
    src/impulseresponse.cpp
    src/impulseresponse.h
    src/waveview.cpp
    src/waveview.h
    src/systemkeyboard.cpp
    src/systemkeyboard.h
    src/pluginmanager.cpp
    src/pluginmanager.h
)

qt_add_executable(guitariqx
    src/main.cpp
    ${GUITARIQX_SOURCES}
)

# must be set before qt_add_qml_module
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(guitariqx
    URI Guitariqx
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
        qml/RackLabel.qml
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
        qml/MidiValueBar.qml
        qml/SwitcherView.qml
        qml/TunerView.qml
        qml/TextInputDialog.qml
        qml/ConfirmDialog.qml
        qml/SavePresetDialog.qml
        qml/UnitPresetDialog.qml
        qml/FileIcon.qml
        qml/FileChooserDialog.qml
        qml/FileButton.qml
        qml/SkinDialog.qml
        qml/OutputWatch.qml
        qml/LoopPosition.qml
        qml/ScopeView.qml
        qml/IrEditor.qml
        qml/SequencerDialog.qml
        qml/PluginManagerDialog.qml
)

# the gx_head "Guitarix" skin and the libgxw icons (skins/README.md)
file(GLOB GUITARIQX_SKIN_IMAGES RELATIVE "${PROJECT_SOURCE_DIR}" CONFIGURE_DEPENDS skins/gxw/*.png)
qt_add_resources(guitariqx skins
    PREFIX /Guitariqx
    FILES skins/gx_head_Guitarix.css ${GUITARIQX_SKIN_IMAGES}
)

target_include_directories(guitariqx PRIVATE src)

target_compile_definitions(guitariqx PRIVATE
    GUITARIQX_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(guitariqx PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS guitariqx
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES guitariqx.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

option(BUILD_TESTING "Build the tests (needs the Qt Test module)" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/Packaging.cmake)
