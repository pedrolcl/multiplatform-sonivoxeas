add_executable( mp_cmdlnsynth main.cpp )

target_link_libraries( mp_cmdlnsynth
    Qt${QT_VERSION_MAJOR}::Core
    Drumstick::RT
    mp_svoxeas
)

target_compile_definitions( mp_cmdlnsynth PRIVATE
    VERSION=${PROJECT_VERSION}
    $<$<CONFIG:RELEASE>:QT_NO_DEBUG_OUTPUT>
)

install( TARGETS mp_cmdlnsynth 
         DESTINATION ${CMAKE_INSTALL_BINDIR} )
