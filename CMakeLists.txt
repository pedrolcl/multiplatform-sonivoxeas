cmake_minimum_required(VERSION 3.9)
project( multiplatform_sonivoxeas
         VERSION 2.1.1
         DESCRIPTION "Multiplatform Sonivox EAS for Qt" )

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include(GNUInstallDirs)

option(USE_QT5 "Choose Qt5 instead of the default Qt6")

if (USE_QT5)
    find_package(QT NAMES Qt5 REQUIRED)
else()
    find_package(QT NAMES Qt6 REQUIRED)
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets Multimedia REQUIRED)
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
