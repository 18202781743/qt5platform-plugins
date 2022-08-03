PROJECT(dwayland)
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

set(REQUIRED_QT_VERSION 5.15.1)
find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core)
find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS WaylandClient XkbCommonSupport)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(AUTOMOC_COMPILER_PREDEFINES ON)

add_definitions(-DQT5DWAYLANDPLUGIN_LIBRARY -DQT_DEPRECATED_WARNINGS)

if(NOT VERSION)
    set(VERSION $$system(git describe --tags --abbrev=0))
    string(REGEX REPLACE "[^0-9.]" "" VERSION ${VERSION})
    if (NOT VERSION)
        set(VERSION 1.1.11)
    endif()
endif()

add_definitions(-DDXCB_VERSION=\"${VERSION}\")

set(HEADERS
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
set(SOURCES
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
add_library(${PROJECT_NAME} SHARED main.cpp ${SOURCES} ${HEADERS})

set(COMMON_LIBS
    Qt5::Core
    ${Qt5WaylandClient_LIBRARIES}
)

# Qt >= 5.8
if (${Qt5_VERSION_MINOR} GREATER 7)
    find_package(Qt5 REQUIRED Gui)
    target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Gui)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${QtGui_PRIVATE_INCLUDE_DIRS}
    )
else()
    find_package(Qt5 REQUIRED PlatformSupport)
endif()

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
