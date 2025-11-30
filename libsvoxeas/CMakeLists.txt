set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set( HEADERS
    programsettings.h
    synthcontroller.h
    synthrenderer.h
)

set( SOURCES
    programsettings.cpp
    synthcontroller.cpp 
    synthrenderer.cpp
    filewrapper.cpp
)

add_library( mp_svoxeas ${HEADERS} ${SOURCES} )

if (BUILD_SHARED_LIBS)
    target_compile_definitions( mp_svoxeas PRIVATE mp_svoxeas_EXPORTS )
else()
    target_compile_definitions ( mp_svoxeas PUBLIC MP_SVOXEAS_STATIC_DEFINE )
endif()

include(GenerateExportHeader)
generate_export_header( mp_svoxeas
  EXPORT_FILE_NAME     ${CMAKE_CURRENT_BINARY_DIR}/mp_svoxeas_visibility.h
  EXPORT_MACRO_NAME    MP_SVOXEAS_PUBLIC
  NO_EXPORT_MACRO_NAME MP_SVOXEAS_PRIVATE
  INCLUDE_GUARD_NAME   MP_SVOXEAS_VISIBILITY_H
)

set_target_properties( mp_svoxeas PROPERTIES 
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_link_libraries( mp_svoxeas 
    PUBLIC
        sonivox::sonivox
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Multimedia
    PRIVATE
        Drumstick::RT
)

target_include_directories( mp_svoxeas
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions( mp_svoxeas PRIVATE
    VERSION=${PROJECT_VERSION}
    $<$<CONFIG:RELEASE>:QT_NO_DEBUG_OUTPUT>
)

install( TARGETS mp_svoxeas
         DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install ( FILES ${HEADERS}
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )
