# The portable package keeps the executable at its root, so Qt's deployment
# must put its libraries there too.
set(CMAKE_INSTALL_BINDIR .)

find_package(Qt6 6.12 REQUIRED COMPONENTS Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.12)

configure_file(Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/app/Version.h" @ONLY)

qt_add_executable(lynx-play WIN32
    AppController.cpp
    AppController.h
    QtLogBridge.cpp
    QtLogBridge.h
    main.cpp)

qt_add_qml_module(lynx-play
    URI LynxPlay
    QML_FILES qml/Main.qml)

# Qt's generated type registration includes AppController.h by file name.
target_include_directories(lynx-play PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(lynx-play PRIVATE
    Lynx::core
    Lynx::remoteplay
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick)
lynx_set_warnings(lynx-play)

install(TARGETS lynx-play RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "$<TARGET_PDB_FILE:lynx-play>" DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)

qt_generate_deploy_qml_app_script(
    TARGET lynx-play
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS)
install(SCRIPT "${deploy_script}")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
include(InstallRequiredSystemLibraries)
