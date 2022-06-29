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

add_library( mp_svoxeas SHARED ${HEADERS} ${SOURCES} )

set_target_properties( mp_svoxeas PROPERTIES 
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_link_libraries( mp_svoxeas 
    PUBLIC
        sonivox
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Multimedia
    PRIVATE
        Drumstick::ALSA
)

target_include_directories( mp_svoxeas
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions( mp_svoxeas PRIVATE
    VERSION=${PROJECT_VERSION}
    $<$<CONFIG:RELEASE>:QT_NO_DEBUG_OUTPUT>
)

install( TARGETS mp_svoxeas
         DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install ( FILES ${HEADERS}
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )
