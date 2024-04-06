cmake_minimum_required(VERSION 3.9)
project( multiplatform_sonivoxeas
         VERSION 2.1.1
         DESCRIPTION "Multiplatform Sonivox EAS for Qt" )

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

option(USE_QT5 "Choose Qt5 instead of the default Qt6")

if (USE_QT5)
    find_package(QT NAMES Qt5 REQUIRED)
else()
    find_package(QT NAMES Qt6 REQUIRED)
endif()

if ((CMAKE_SYSTEM_NAME MATCHES "Linux") AND (QT_VERSION_MAJOR EQUAL 6) AND (QT_VERSION VERSION_LESS 6.4))
    message(WARNING "Unsupported Qt version ${QT_VERSION} for ${CMAKE_SYSTEM_NAME}")
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets Multimedia REQUIRED)
find_package(Drumstick 2.9 COMPONENTS RT Widgets REQUIRED)
if (Drumstick_FOUND)
    message(STATUS "Drumstick v${Drumstick_VERSION} found")
endif()

find_package(sonivox 3.6.13 CONFIG)
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
