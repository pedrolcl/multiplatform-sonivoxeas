set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set( SOURCES main.cpp mainwindow.cpp )
set( HEADERS mainwindow.h )
set( FORMS mainwindow.ui )
set( RESOURCES guisynth.qrc )

add_executable( mp_guisynth WIN32 MACOSX_BUNDLE
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

install( TARGETS mp_guisynth
         DESTINATION ${CMAKE_INSTALL_BINDIR}
         COMPONENT sonivoxeas_application
         RUNTIME_DEPENDENCY_SET sonivoxeas-dependencies
)

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/icon.png
         DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
         RENAME sonivoxeas.png )

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/icon256.png
         DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
         RENAME sonivoxeas.png )

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/icon512.png
         DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
         RENAME sonivoxeas.png )

install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/mp_guisynth.desktop
         DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
         RENAME sonivoxeas.desktop )

 install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/icon.svg
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
          RENAME sonivoxeas.svg )
