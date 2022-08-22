cmake_minimum_required(VERSION 3.9)
project( multiplatform_sonivoxeas
         VERSION 2.0.1
         DESCRIPTION "Multiplatform Sonivox EAS for Qt" )

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED)
if ((CMAKE_SYSTEM_NAME MATCHES "Linux") AND (QT_VERSION_MAJOR EQUAL 6) AND (QT_VERSION VERSION_LESS 6.4))
    message(WARNING "Unsupported Qt version ${QT_VERSION} for ${CMAKE_SYSTEM_NAME}")
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets Multimedia REQUIRED)
find_package(Drumstick 2.6 COMPONENTS RT Widgets REQUIRED)

find_package(sonivox CONFIG)
if (NOT sonivox_FOUND)
    add_subdirectory(sonivox)
endif()
add_subdirectory(libsvoxeas)
add_subdirectory(cmdlnsynth)
add_subdirectory(guisynth)
