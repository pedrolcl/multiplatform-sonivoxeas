set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set( SOURCES main.cpp mainwindow.cpp KDSignalThrottler.cpp)
set( HEADERS mainwindow.h KDSignalThrottler.h)
set( FORMS mainwindow.ui )
set( RESOURCES guisynth.qrc )

add_executable( mp_guisynth
  ${SOURCES}
  ${HEADERS}
  ${FORMS}
  ${RESOURCES}
)

target_link_libraries( mp_guisynth
  Qt${QT_VERSION_MAJOR}::Widgets
  Drumstick::RT
  Drumstick::Widgets
  mp_svoxeas
)

target_compile_definitions( mp_guisynth PRIVATE
    VERSION=${PROJECT_VERSION}
    $<$<CONFIG:RELEASE>:QT_NO_DEBUG_OUTPUT>
)

if (WIN32)
    configure_file(sonivoxeas.rc.in sonivoxeas.rc @ONLY)
    target_sources ( mp_guisynth PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/sonivoxeas.rc
    )
    set_target_properties(mp_guisynth PROPERTIES
        WIN32_EXECUTABLE TRUE )
endif()

if (APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "sonivoxeas.icns")
    set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sonivoxeas.icns")
    set_source_files_properties( ${APP_ICON_PATH} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources" )
    target_sources( mp_guisynth PUBLIC ${APP_ICON_PATH} )
    set_target_properties(mp_guisynth PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE ${MACOSX_BUNDLE_ICON_FILE}
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.pedrolcl.sonivoxeas"
        MACOSX_BUNDLE_BUNDLE_NAME "mp_guisynth"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2016-2026 Pedro López-Cabanillas"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

install( TARGETS mp_guisynth
         DESTINATION ${CMAKE_INSTALL_BINDIR}
         COMPONENT sonivoxeas_application
         RUNTIME_DEPENDENCY_SET sonivoxeas-dependencies
)

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/sonivoxeas.png
         DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
         RENAME sonivoxeas.png )

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/sonivoxeas256.png
         DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
         RENAME sonivoxeas.png )

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/sonivoxeas512.png
         DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
         RENAME sonivoxeas.png )

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/sonivoxeas.desktop
         DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
         RENAME sonivoxeas.desktop )

 install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/sonivoxeas.svg
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
          RENAME sonivoxeas.svg )
