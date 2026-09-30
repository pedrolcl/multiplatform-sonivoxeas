#[==========================================================================[
Multiplatform SonivoxEAS
Copyright (C) 2006-2026 Pedro Lopez-Cabanillas <plcl@users.sourceforge.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
#]==========================================================================]

cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

if (POLICY CMP0153)
    cmake_policy(SET CMP0153 NEW)
endif()

project( multiplatform_sonivoxeas
         VERSION 2.2.1
         LANGUAGES CXX
         DESCRIPTION "Multiplatform Sonivox EAS for Qt" )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(USE_QT5 "Choose Qt5 instead of the default Qt6" OFF)
option(INSTALL_DEPENDENCIES "Deploy Dependencies at Install" OFF)

# CMAKE_SYSTEM_PROCESSOR is broken on Windows with MSVC
# cmake bug https://gitlab.kitware.com/cmake/cmake/-/issues/15170 still open in 2024/12
if(WIN32 AND (CMAKE_VERSION VERSION_GREATER_EQUAL 4.1))
      string(TOLOWER ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} CMAKE_SYSTEM_PROCESSOR)
endif()

if (USE_QT5)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
    include(GNUInstallDirs)

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

find_package(Drumstick 2.11 COMPONENTS RT Widgets REQUIRED)
if (Drumstick_FOUND)
    message(STATUS "Drumstick v${Drumstick_VERSION} found")
#     include(CMakePrintHelpers)
#     cmake_print_properties(TARGETS Drumstick::RT PROPERTIES LOCATION)
endif()

set(sonivox_SHARED_LIBS TRUE)
find_package(sonivox 4.0 CONFIG shared)
if (sonivox_FOUND)
    message(STATUS "Sonivox v${sonivox_VERSION} found")
    # include(CMakePrintHelpers)
    # cmake_print_properties(TARGETS sonivox::sonivox PROPERTIES LOCATION)
else()
    add_subdirectory(sonivox)
    # include(CMakePrintHelpers)
    # cmake_print_properties(TARGETS sonivox::sonivox PROPERTIES TYPE VERSION SOVERSION)
endif()

message (STATUS "${PROJECT_NAME} v${PROJECT_VERSION}
    install prefix: ${CMAKE_INSTALL_PREFIX}
    Build configuration: ${CMAKE_BUILD_TYPE}
    Processor: ${CMAKE_SYSTEM_PROCESSOR}
    Qt: ${QT_VERSION}
    Sonivox: ${sonivox_VERSION}
    Drumstick: ${Drumstick_VERSION}")

add_subdirectory(libsvoxeas)
add_subdirectory(cmdlnsynth)
add_subdirectory(guisynth)

if (INSTALL_DEPENDENCIES AND QT_VERSION VERSION_GREATER_EQUAL 6.5 )
    if (sonivox_FOUND)
        install(IMPORTED_RUNTIME_ARTIFACTS sonivox::sonivox
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    else()
        message(WARNING "Sonivox package not found")
    endif()
    if (Drumstick_FOUND)
        install(IMPORTED_RUNTIME_ARTIFACTS Drumstick::RT
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
        install(IMPORTED_RUNTIME_ARTIFACTS Drumstick::Widgets
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    else()
        message(WARNING "Drumstick package not found")
    endif()

    if (DEFINED Drumstick_DIR)
        if (Drumstick_DIR MATCHES "cmake")
            set(_drumstick_plugins ${Drumstick_DIR}/../../drumstick2)
        else()
            set(_drumstick_plugins ${Drumstick_DIR}/lib/drumstick2)
        endif()
        if (EXISTS ${_drumstick_plugins})
            set(CMAKE_INSTALL_PLUGINS "plugins")
            install(DIRECTORY ${_drumstick_plugins}
                    DESTINATION ${CMAKE_INSTALL_PLUGINS})
        else()
            message(WARNING "${_drumstick_plugins} does not exists")
        endif()
    else()
        message(WARNING "Drumstick_DIR is not defined")
    endif()

    set(_deploy_tool_options_arg "")
    if(WIN32)
        set(_deploy_tool_options_arg "
            --plugindir \".\"
            --translationdir \".\"
        ")
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    else()
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    if(MSVC)
        qt_generate_deploy_app_script(
            TARGET mp_guisynth
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            NO_COMPILER_RUNTIME
            NO_TRANSLATIONS
            DEPLOY_TOOL_OPTIONS ${_deploy_tool_options_arg}
        )
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT sonivoxeas_runtime)
        include ( InstallRequiredSystemLibraries )
    elseif(WIN32 AND NOT MSVC)
        qt_generate_deploy_app_script(
            TARGET mp_guisynth
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            NO_TRANSLATIONS
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        qt_generate_deploy_app_script(
            TARGET mp_guisynth
            OUTPUT_SCRIPT deploy_script
            INCLUDE_PLUGINS xcb wayland
            NO_UNSUPPORTED_PLATFORM_ERROR
            NO_TRANSLATIONS
        )
    endif()
    if (DEFINED deploy_script)
        install(SCRIPT ${deploy_script} COMPONENT sonivoxeas_runtime)
    endif()
    # AppImage
    if (UNIX AND NOT APPLE)
        install( CODE
          "include(${CMAKE_SOURCE_DIR}/cmake/CreateAppImage.cmake)
           create_appimage(
                EXE \"mp_guisynth\"
                ICON \"sonivoxeas.png\"
                DESKTOP \"sonivoxeas.desktop\"
                WORKDIR \"${CMAKE_CURRENT_BINARY_DIR}\"
                APPDIR \"multiplatform-sonivoxeas-v${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.AppDir\"
                ARCH \"${CMAKE_SYSTEM_PROCESSOR}\"
           )"
        )
    endif()
endif()

if (DEFINED ENV{GITHUB_OUTPUT})
    file(APPEND
        "$ENV{GITHUB_OUTPUT}"
        "config_version=${PROJECT_VERSION}\n"
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
set ( CPACK_PACKAGE_EXECUTABLES "mp_guisynth;SonivoxEAS" )

# source packages
set ( CPACK_SOURCE_IGNORE_FILES "/.git/;/build/;~$;${CPACK_SOURCE_IGNORE_FILES}" )
set ( CPACK_SOURCE_PACKAGE_FILE_NAME "sonivoxeas-${PROJECT_VERSION}" )
set ( CPACK_SOURCE_STRIP_FILES OFF )

# binary packages
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
