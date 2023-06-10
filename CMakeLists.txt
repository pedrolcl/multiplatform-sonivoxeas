cmake_minimum_required(VERSION 3.9)
project( multiplatform_sonivoxeas
         VERSION 2.1.0
         DESCRIPTION "Multiplatform Sonivox EAS for Qt" )

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

option(USE_QT "Choose which Qt major version (5 or 6) to prefer. By default uses whatever is found")

if (USE_QT)
    if (NOT (USE_QT EQUAL 5 OR USE_QT EQUAL 6))
        message(FATAL_ERROR "Wrong Qt major version. Only 5 and 6 are valid options")
    endif()
endif()

if (USE_QT EQUAL 5)
    find_package(QT NAMES Qt5 REQUIRED)
elseif (USE_QT EQUAL 6)
    find_package(QT NAMES Qt6 REQUIRED)
else()
    find_package(QT NAMES Qt5 Qt6 REQUIRED)
endif()

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
