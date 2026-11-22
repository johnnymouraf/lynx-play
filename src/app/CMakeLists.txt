# The portable package keeps the executable at its root, so Qt's deployment
# must put its libraries there too.
set(CMAKE_INSTALL_BINDIR .)

find_package(Qt6 6.10 REQUIRED COMPONENTS Gui Qml Quick Svg)
qt_standard_project_setup(REQUIRES 6.10)

configure_file(Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/app/Version.h" @ONLY)

qt_add_executable(lynx-play WIN32
    AppController.cpp
    AppController.h
    ConsoleListModel.cpp
    ConsoleListModel.h
    ConsolePresentation.cpp
    ConsolePresentation.h
    ConsolesController.cpp
    ConsolesController.h
    FilePlayback.cpp
    FilePlayback.h
    Fonts.cpp
    Fonts.h
    IconProvider.cpp
    IconProvider.h
    PairingController.cpp
    PairingController.h
    QtLogBridge.cpp
    QtLogBridge.h
    SettingsController.cpp
    SettingsController.h
    StreamController.cpp
    StreamController.h
    main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(lynx_fonts
    assets/fonts/Inter-Medium.ttf
    assets/fonts/Inter-Regular.ttf
    assets/fonts/Inter-SemiBold.ttf
    assets/fonts/JetBrainsMono-Medium.ttf
    assets/fonts/JetBrainsMono-SemiBold.ttf
    assets/fonts/Sora-Bold.ttf
    assets/fonts/Sora-Regular.ttf
    assets/fonts/Sora-SemiBold.ttf)

set(lynx_licenses
    assets/fonts/Inter-OFL.txt
    assets/fonts/JetBrainsMono-OFL.txt
    assets/fonts/Sora-OFL.txt
    assets/icons/LICENSE-lucide.txt)

qt_add_qml_module(lynx-play
    URI LynxPlay
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/consoles/AddByAddressDialog.qml
        qml/consoles/ConsoleCard.qml
        qml/consoles/ConsolesPage.qml
        qml/consoles/DiscoveryBanner.qml
        qml/consoles/EmptyState.qml
        qml/consoles/ForgetDialog.qml
        qml/consoles/PairingDialog.qml
        qml/consoles/RenameDialog.qml
        qml/controls/ActionButton.qml
        qml/controls/AppRail.qml
        qml/controls/AppText.qml
        qml/controls/Banner.qml
        qml/controls/BrandLogo.qml
        qml/controls/DialogShell.qml
        qml/controls/FocusRing.qml
        qml/controls/Icon.qml
        qml/controls/IconButton.qml
        qml/controls/InputField.qml
        qml/controls/LinkButton.qml
        qml/controls/MenuRow.qml
        qml/controls/NavItem.qml
        qml/controls/PinInput.qml
        qml/controls/PopupMenu.qml
        qml/controls/RoundedImage.qml
        qml/controls/ScanIndicator.qml
        qml/controls/SegmentItem.qml
        qml/controls/SegmentedControl.qml
        qml/controls/SettingRow.qml
        qml/controls/Spinner.qml
        qml/controls/StateBadge.qml
        qml/controls/ThinScrollBar.qml
        qml/controls/Toast.qml
        qml/controls/ValueSlider.qml
        qml/settings/AboutSettings.qml
        qml/settings/SectionHeader.qml
        qml/settings/SettingsPage.qml
        qml/settings/VideoSettings.qml
        qml/stream/ConnectingPage.qml
        qml/stream/ConnectionStep.qml
    RESOURCES
        ${lynx_fonts}
        ${lynx_licenses}
        assets/icons/check.svg
        assets/icons/chevron-right.svg
        assets/icons/circle-check.svg
        assets/icons/circle-exclamation-point.svg
        assets/icons/circle-question-mark.svg
        assets/icons/circle.svg
        assets/icons/clock.svg
        assets/icons/ellipsis.svg
        assets/icons/external-link.svg
        assets/icons/folder-open.svg
        assets/icons/house-wifi.svg
        assets/icons/info.svg
        assets/icons/link-2.svg
        assets/icons/loader-circle.svg
        assets/icons/lock-keyhole.svg
        assets/icons/monitor-play.svg
        assets/icons/monitor.svg
        assets/icons/moon.svg
        assets/icons/network.svg
        assets/icons/pencil.svg
        assets/icons/play.svg
        assets/icons/plus.svg
        assets/icons/power.svg
        assets/icons/refresh-cw.svg
        assets/icons/rotate-ccw.svg
        assets/icons/scroll-text.svg
        assets/icons/settings.svg
        assets/icons/trash.svg
        assets/icons/tv-minimal-play.svg
        assets/icons/wifi-off.svg
        assets/icons/x.svg
        assets/images/brand-mark.png
        assets/images/console-glyph.svg
        assets/images/console-render.png)

# Qt's generated type registration includes AppController.h by file name.
target_include_directories(lynx-play PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(lynx-play PRIVATE
    Lynx::core
    Lynx::media
    Lynx::remoteplay
    Lynx::render
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Svg)
lynx_set_warnings(lynx-play)

install(TARGETS lynx-play RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "$<TARGET_PDB_FILE:lynx-play>" DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
install(FILES ${lynx_licenses} DESTINATION licenses)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/nvapi/License.txt"
        DESTINATION licenses RENAME NVAPI-License.txt)

qt_generate_deploy_qml_app_script(
    TARGET lynx-play
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS)
install(SCRIPT "${deploy_script}")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
include(InstallRequiredSystemLibraries)
