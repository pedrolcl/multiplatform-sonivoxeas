cmake_minimum_required(VERSION 3.21)
project( multiplatform_sonivoxeas
         VERSION 2.2.0
         DESCRIPTION "Multiplatform Sonivox EAS for Qt" )

include(GNUInstallDirs)

option(USE_QT5 "Choose Qt5 instead of the default Qt6")

if (USE_QT5)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)

    find_package(QT NAMES Qt5 REQUIRED)
else()
    find_package(QT NAMES Qt6 REQUIRED)
endif()
if (QT_FOUND)
    message(STATUS "Qt v${QT_VERSION} found")
endif()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Multimedia)

if (NOT USE_QT5)
    qt6_standard_project_setup()
endif()

find_package(Drumstick 2.10 COMPONENTS RT Widgets REQUIRED)
if (Drumstick_FOUND)
    message(STATUS "Drumstick v${Drumstick_VERSION} found")
endif()

set(sonivox_SHARED_LIBS TRUE)
find_package(sonivox 4.0 CONFIG REQUIRED)
if (sonivox_FOUND)
    message(STATUS "Sonivox v${sonivox_VERSION} found")
else()
    add_subdirectory(sonivox)
    include(CMakePrintHelpers)
    cmake_print_properties(TARGETS sonivox::sonivox PROPERTIES TYPE VERSION SOVERSION)
endif()

add_subdirectory(libsvoxeas)
add_subdirectory(cmdlnsynth)
add_subdirectory(guisynth)

if (NOT USE_QT5)
    qt_generate_deploy_app_script(
        TARGET mp_guisynth
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        # INCLUDE_PLUGINS drumstick2
    )
    install(SCRIPT ${deploy_script})
endif()

install(DIRECTORY ${Drumstick_DIR}/lib/drumstick2
    DESTINATION plugins
    COMPONENT sonivoxeas_runtime
)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    message(STATUS "DEPENDENCY_DIRS: ${DEPENDENCY_DIRS}")
    install(RUNTIME_DEPENDENCY_SET sonivoxeas-dependencies
        COMPONENT sonivoxeas_runtime
        PRE_INCLUDE_REGEXES .*zlib.* .*sonivox.* .*drumstick.*
        PRE_EXCLUDE_REGEXES .*
        DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR} ${DEPENDENCY_DIRS}
    )
endif()

# CPack support
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Multiplatform Sonivox EAS synthesizer" )
set ( CPACK_PACKAGE_VENDOR "https://github.com/pedrolcl/multiplatform_sonivoxeas" )
set ( CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md" )
set ( CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH} )
#set ( CPACK_PACKAGE_EXECUTABLES "mp_cmdlnsynth m_guisynth" "SonivoxEAS" )

# source packages
set ( CPACK_SOURCE_IGNORE_FILES "/.git/;/build/;~$;${CPACK_SOURCE_IGNORE_FILES}" )
set ( CPACK_SOURCE_PACKAGE_FILE_NAME "sonivoxeas-${PROJECT_VERSION}" )
set ( CPACK_SOURCE_STRIP_FILES OFF )

# binary packages
include ( InstallRequiredSystemLibraries )
set ( CPACK_PACKAGE_NAME sonivoxeas )
set ( CPACK_STRIP_FILES ON )

include ( CPack )

cpack_add_component(sonivoxeas_runtime
  DISPLAY_NAME "Runtime Libraries"
  DESCRIPTION "Shared libraries (DLL) needed to run SonivoxEAS programs")
cpack_add_component(sonivoxeas_application
  DISPLAY_NAME "SonivoxEAS Programs"
  DESCRIPTION "SonivoxEAS CLI and GUI Programs"
  DEPENDS sonivoxeas_runtime)
cpack_add_component(sonivoxeas_development
  DISPLAY_NAME "C Headers and import libraries"
  DESCRIPTION "C headers and import libraries for developing with SonivoxEAS"
  DEPENDS sonivoxeas_runtime)
