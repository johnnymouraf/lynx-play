# The portable package keeps the executable at its root, so Qt's deployment
# must put its libraries there too.
set(CMAKE_INSTALL_BINDIR .)

find_package(Qt6 6.10 REQUIRED COMPONENTS Gui Network Qml Quick Svg)
qt_standard_project_setup(REQUIRES 6.10)

# CI passes the run number, so a downloaded build says which one it is.
set(MFPLAY_VERSION "${PROJECT_VERSION}")
if(MFPLAY_BUILD_NUMBER)
    string(APPEND MFPLAY_VERSION "-build.${MFPLAY_BUILD_NUMBER}")
endif()
configure_file(Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/app/Version.h" @ONLY)

qt_add_executable(mfplay WIN32
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
    InputController.cpp
    InputController.h
    OverlayInput.h
    PairingController.cpp
    PairingController.h
    PsnLoginClient.cpp
    PsnLoginClient.h
    QtLogBridge.cpp
    QtLogBridge.h
    SettingsController.cpp
    SettingsController.h
    StreamController.cpp
    StreamController.h
    main.cpp
    mfplay.rc)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(mfplay_fonts
    assets/fonts/Inter-Bold.ttf
    assets/fonts/Inter-Medium.ttf
    assets/fonts/Inter-Regular.ttf
    assets/fonts/Inter-SemiBold.ttf
    assets/fonts/JetBrainsMono-Bold.ttf
    assets/fonts/JetBrainsMono-Medium.ttf
    assets/fonts/JetBrainsMono-SemiBold.ttf
    assets/fonts/Sora-Bold.ttf
    assets/fonts/Sora-Regular.ttf
    assets/fonts/Sora-SemiBold.ttf)

set(mfplay_licenses
    assets/fonts/Inter-OFL.txt
    assets/fonts/JetBrainsMono-OFL.txt
    assets/fonts/Sora-OFL.txt
    assets/icons/LICENSE-lucide.txt)

qt_add_qml_module(mfplay
    URI MFPlay
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
        qml/controls/ControllerHintBar.qml
        qml/controls/DialogShell.qml
        qml/controls/FocusRing.qml
        qml/controls/Icon.qml
        qml/controls/IconButton.qml
        qml/controls/IconTile.qml
        qml/controls/InputField.qml
        qml/controls/Keycap.qml
        qml/controls/MenuRow.qml
        qml/controls/NavItem.qml
        qml/controls/PinInput.qml
        qml/controls/PopupMenu.qml
        qml/controls/RoundedImage.qml
        qml/controls/ScanIndicator.qml
        qml/controls/SegmentItem.qml
        qml/controls/SegmentedControl.qml
        qml/controls/Select.qml
        qml/controls/SettingRow.qml
        qml/controls/Spinner.qml
        qml/controls/StateBadge.qml
        qml/controls/ThinScrollBar.qml
        qml/controls/Toast.qml
        qml/controls/Toggle.qml
        qml/controls/ValueSlider.qml
        qml/settings/AboutSettings.qml
        qml/settings/AudioSettings.qml
        qml/settings/ControllerSettings.qml
        qml/settings/RtxSettings.qml
        qml/settings/SectionHeader.qml
        qml/settings/SettingsPage.qml
        qml/settings/VideoSettings.qml
        qml/stream/ConnectingPage.qml
        qml/stream/ConnectionStep.qml
    RESOURCES
        ${mfplay_fonts}
        ${mfplay_licenses}
        assets/icons/check.svg
        assets/icons/chevron-down.svg
        assets/icons/chevron-up.svg
        assets/icons/chevron-right.svg
        assets/icons/circle-check.svg
        assets/icons/circle-exclamation-point.svg
        assets/icons/circle-question-mark.svg
        assets/icons/circle.svg
        assets/icons/clock.svg
        assets/icons/ellipsis.svg
        assets/icons/external-link.svg
        assets/icons/folder-open.svg
        assets/icons/gamepad-2.svg
        assets/icons/flask-conical.svg
        assets/icons/house-wifi.svg
        assets/icons/info.svg
        assets/icons/layout-grid.svg
        assets/icons/link-2.svg
        assets/icons/list.svg
        assets/icons/loader-circle.svg
        assets/icons/lock-keyhole.svg
        assets/icons/menu.svg
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
        assets/icons/sparkles.svg
        assets/icons/square.svg
        assets/icons/trash.svg
        assets/icons/triangle.svg
        assets/icons/tv-minimal-play.svg
        assets/icons/volume-2.svg
        assets/icons/wifi-off.svg
        assets/icons/x.svg
        assets/images/app-icon.svg
        assets/images/console-glyph.svg
        assets/images/console-render.png)

# Qt's generated type registration includes AppController.h by file name.
target_include_directories(mfplay PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(mfplay PRIVATE
    MFPlay::audio
    MFPlay::core
    MFPlay::input
    MFPlay::media
    MFPlay::remoteplay
    MFPlay::render
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::Svg)
mfplay_set_warnings(mfplay)

install(TARGETS mfplay RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${mfplay_licenses} DESTINATION licenses)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/nvapi/License.txt"
        DESTINATION licenses RENAME NVAPI-License.txt)
install(FILES assets/NGX-Notices.txt DESTINATION licenses RENAME NGX-Notices.txt)

# NVIDIA's DLSS runtimes ship next to the executable, with their license; the
# loader imports nvsdk_ngx_d.lib and finds the DLLs in the application folder.
if(EXISTS "${MFPLAY_NGX_ROOT}/bin/nvngx_dlss.dll")
    install(FILES
            "${MFPLAY_NGX_ROOT}/bin/nvngx_dlss.dll"
            "${MFPLAY_NGX_ROOT}/bin/nvngx_dlssg.dll"
            DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES "${MFPLAY_NGX_ROOT}/LICENSE.txt"
            DESTINATION licenses RENAME NGX-License.txt)
endif()

# Dear ImGui is static inside the executable; its notice ships with it.
find_file(MFPLAY_IMGUI_COPYRIGHT NAMES copyright
    PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/imgui"
    NO_DEFAULT_PATH)
if(MFPLAY_IMGUI_COPYRIGHT)
    install(FILES "${MFPLAY_IMGUI_COPYRIGHT}" DESTINATION licenses RENAME ImGui-License.txt)
endif()

# The software OpenGL fallback is 20 MB and Qt Quick uses Direct3D here.
# The MSVC runtime DLLs come with the Qt deployment, so the redistributable
# installer is not shipped.
qt_generate_deploy_qml_app_script(
    TARGET mfplay
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    DEPLOY_TOOL_OPTIONS --no-opengl-sw)
install(SCRIPT "${deploy_script}")
