# Unit tests (Qt Test). Run them with "ctest --test-dir build".
#
# The application's classes are compiled once more into a static library,
# without main() and without the QML module, and tested against FakeEngine,
# a JSON-RPC server answering like a recorded guitarix engine.

find_package(Qt6 REQUIRED COMPONENTS Test)

list(TRANSFORM GUITARIQX_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/" OUTPUT_VARIABLE _sources)
add_library(guitariqx_testlib STATIC
    ${_sources}
    fakeengine.cpp
    fakeengine.h
)
target_include_directories(guitariqx_testlib PUBLIC "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(guitariqx_testlib PUBLIC
    GUITARIQX_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
target_link_libraries(guitariqx_testlib PUBLIC
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Test
)

function(guitariqx_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE guitariqx_testlib)
    add_test(NAME ${name} COMMAND ${name})
    # QStandardPaths::setTestModeEnabled() keeps the tests out of the
    # user's settings; offscreen: no display needed
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 120)
endfunction()

guitariqx_add_test(tst_parameter)
guitariqx_add_test(tst_uitree)
guitariqx_add_test(tst_rpcclient)
guitariqx_add_test(tst_machine)
guitariqx_add_test(tst_dirmodel)
guitariqx_add_test(tst_skin)
target_compile_definitions(tst_skin PRIVATE GUITARIQX_SKINS="${PROJECT_SOURCE_DIR}/skins")

# runs the application against FakeEngine and fails on QML errors
guitariqx_add_test(tst_ui)
target_compile_definitions(tst_ui PRIVATE GUITARIQX_BINARY="$<TARGET_FILE:guitariqx>")
add_dependencies(tst_ui guitariqx)

# a stand-alone FakeEngine to run guitariqx without guitarix:
# build/tests/fake-engine -p 7124 & build/guitariqx -p 7124
add_executable(fake-engine fakeengine_main.cpp)
target_link_libraries(fake-engine PRIVATE guitariqx_testlib)
