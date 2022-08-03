PROJECT(dwayland)
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

set(REQUIRED_QT_VERSION 5.15.1)
find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core)
find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS WaylandClient XkbCommonSupport)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(AUTOMOC_COMPILER_PREDEFINES ON)

add_definitions(-DQT5DWAYLANDPLUGIN_LIBRARY)

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
add_definitions(-DQT_DEPRECATED_WARNINGS)

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x060000)    # disables all the APIs deprecated before Qt 6.0.0

if(NOT VERSION)
    execute_process(COMMAND git describe --tags --abbrev=0 OUTPUT_VARIABLE TAG_VERSION)
    string(REGEX REPLACE "[^0-9.]" "" VERSION ${TAG_VERSION})
    if (NOT VERSION)
        set(VERSION 1.1.11)
    endif()
endif()

set(GLOBAL_HEADERS
    dwaylandinterfacehook.h
    dwaylandintegration.h
    dnotitlebarwindowhelper_wl.h
    dhighdpi.h
    dxsettings.h
    ../../src/global.h
    ../../src/vtablehook.h
    ../../src/dxcbxsettings.h
    ../../src/dplatformsettings.h
    ../../src/dnativesettings.h
)
set(GLOBAL_SOURCES
    dwaylandinterfacehook.cpp
    dwaylandintegration.cpp
    dnotitlebarwindowhelper_wl.cpp
    dhighdpi.cpp
    dxsettings.cpp
    ../../src/global.cpp
    ../../src/vtablehook.cpp
    ../../src/dxcbxsettings.cpp
    ../../src/dplatformsettings.cpp
    ../../src/dnativesettings.cpp
)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/plugins/platforms)
add_library(${PROJECT_NAME} MODULE main.cpp ${GLOBAL_SOURCES} ${GLOBAL_HEADERS})

set(COMMON_LIBS
    Qt5::Core
    ${Qt5WaylandClient_LIBRARIES}
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${COMMON_LIBS})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${Qt5WaylandClient_PRIVATE_INCLUDE_DIRS}
    ${Qt5XkbCommonSupport_PRIVATE_INCLUDE_DIRS}
    ../../src
)

if (NOT INSTALL_PATH)
    set(INSTALL_PATH ${CMAKE_INSTALL_LIBDIR}/qt5/plugins/platforms)
endif()
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_PATH})
